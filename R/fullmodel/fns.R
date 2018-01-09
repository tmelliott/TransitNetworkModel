pbToCSV <- function(pb) {
    ## converts protobuf GTFS file into a dataframe
    feed <- pb$entity
    do.call(rbind, lapply(feed, function(x) {
        data.frame(vehicle_id = as.character(x$vehicle$vehicle$id),
                   route_id = x$vehicle$trip$route_id,
                   trip_id = x$vehicle$trip$trip_id,
                   lat = x$vehicle$position$latitude,
                   lng = x$vehicle$position$longitude,
                   timestamp = x$vehicle$timestamp,
                   stringsAsFactors = FALSE)
    }))
}
makeData <- function(date, ip, dir, db = "busdata.db") {
    ## get a list of files for the chosen day
    dir <- file.path(dir, gsub("-", "/", date))
    # datef <- gsub("-", "", date)
    cmd <- sprintf("ssh %s find %s -type f -name 'vehicle_locations_*.pb'", ip, dir)
    cat(" - Generating list of files ... ")
    files <- system(cmd, intern = TRUE)
    files <- sort(files)
    cat(sprintf("done (%s)", length(files)))

    ## create the database
    cat("\n - Creating database table")
    con <- dbConnect(SQLite(), db)
    if ("vehicle_positions" %in% dbListTables(con)) dbRemoveTable(con, "vehicle_positions")
    dbSendQuery(con, paste(sep="\n",
                          "CREATE TABLE vehicle_positions (",
                          " vehicle_id TEXT,",
                          " route_id TEXT,",
                          " trip_id TEXT,",
                          " lat REAL,",
                          " lng REAL,",
                          " timestamp INTEGER",
                          ")"))
    
    ## read the files one-by-one into a database
    cat("\n - Writing files to database ...\n")
    pbar <- txtProgressBar(0, length(files), style = 3)
    for (file in files) {
        setTxtProgressBar(pbar, which(files == file))
        ip <- PI.IP
        d <- read(transit_realtime.FeedMessage,
                  pipe(sprintf("ssh %s cat %s", ip, file)))
        dbWriteTable(con, "vehicle_positions", pbToCSV(d), append = TRUE)
    }
    close(pbar)

    ## remove duplicates
    cat("\n - Removing duplicates ... ")
    dbSendQuery(con, "DELETE FROM vehicle_positions WHERE rowid NOT IN (SELECT MIN(rowid) FROM vehicle_positions GROUP BY vehicle_id, timestamp)");
    cat("done")
    
    ## finish up
    dbDisconnect(con)
    cat(sprintf("\n\nFinished. Data written to %s\n", db))
    invisible(NULL)
}




getShape <- function(data, id = NULL) UseMethod("getShape")
getShape.gtfs.data <- function(data, id) {
    if (missing(id)) return(attr(data, "shapes"))
    d <- attr(data, "shapes") %>%
        filter(shape_id == id)
    class(d) <- class(attr(data, "shapes"))
    d
}
getStops <- function(data, id = NULL) UseMethod("getStops")
getStops.gtfs.data <- function(data, id) {
    if (missing(id)) return(attr(data, "stops"))
    d <- attr(data, "stops") %>%
        filter(trip_id == id)
    class(d) <- class(attr(data, "stops"))
    d
}
getSegments <- function(data, id = NULL) UseMethod("getSegments")
getSegments.gtfs.data <- function(data, id) {
    if (missing(id)) return(attr(data, "segments"))
    d <- attr(data, "segments") %>%
        filter(shape_id == id)
    class(d) <- class(attr(data, "segments"))
    d
}
plot.gtfs.shape <- function(x, zoom = 12, colour = "orangered", ...) {
    xr <- extendrange(x$lng)
    yr <- extendrange(x$lat)
    bbox <- c(xr[1], yr[1], xr[2], yr[2])
    akl <- get_stamenmap(bbox, zoom = zoom, maptype = "toner-background")
    
    p <- ggmap(akl, darken = 0.85) +
        geom_path(aes(lng, lat), data = x, colour = colour, lwd = 1)
    print(p)
    invisible(p)
}
getRouteData <- function(route, db, gtfs.db) {
    con <- dbConnect(SQLite(), gtfs.db)
    q <- dbSendQuery(con, "SELECT route_id, shape_id FROM routes WHERE route_short_name=?")
    dbBind(q, list(route))
    rid <- dbFetch(q)
    dbClearResult(q)

    shapes <- dbGetQuery(con, sprintf("SELECT * FROM shapes WHERE shape_id IN ('%s') ORDER BY shape_id, seq",
                                      paste(rid$shape_id, collapse = "','")))
    segs <- dbGetQuery(con, sprintf("SELECT * FROM shape_segments WHERE shape_id IN ('%s') ORDER BY shape_id, leg",
                                    paste(rid$shape_id, collapse = "','")))
    
    con2 <- dbConnect(SQLite(), db)
    dat <- dbGetQuery(
        con2,
        sprintf(
            "SELECT * FROM vehicle_positions WHERE route_id IN ('%s') ORDER BY timestamp, vehicle_id",
            paste(rid$route_id, collapse = "','")
        ))
    dbDisconnect(con2)

    stops <- dbGetQuery(con, sprintf("SELECT stops.*, trip_id, stop_sequence, shape_dist_traveled FROM stops, stop_times WHERE stops.stop_id=stop_times.stop_id AND trip_id IN ('%s') ORDER BY trip_id, stop_sequence",
                                     paste(unique(dat$trip_id), collapse = "','")))
    dbDisconnect(con)

    dat <- dat %>% dplyr::mutate(vehicle_id = as.factor(vehicle_id)) %>% 
        merge(rid, by = "route_id")

    class(shapes) <- c("gtfs.shape", class(shapes))
    attr(dat, "shapes") <- shapes
    class(segs) <- c("gtfs.segments", class(segs))
    attr(dat, "segments") <- segs
    class(stops) <- c("gtfs.stops", class(stops))
    attr(dat, "stops") <- stops
    class(dat) <- c("gtfs.data", class(dat))

    

    return(dat)
}






particle <- function(x, ...) {
    if (missing(x))
        x <- initializeParticle(...)
    else
        x <- structure(mutate(x, sd = Sd, rd = Rd, Dmax = max(Sd), ...),
                       "t0" = 0, "Sd" = Sd, "Rd" = Rd)
    t0 <- 0
    if (!is.null(attr(x, "t0"))) t0 <- attr(x, "t0")
    if (any(diff(x) < 0))
        stop("Invalid particle: trajectory must be non-decreasing")
    atts <- attributes(x)
    attributes(x) <- NULL
    structure(list("distance" = x, "t0" = t0,
                   "Rd" = atts$Rd, "Sd" = atts$Sd),
              class = "particle")
}
print.particle <- function(x, ...)
    print(x$distance)
plot.particle <- function(x, ...)
    plot(startTime(x) + seq_along(x$distance) - 1, x$distance,
         xlab = "Time (s)", ylab = "Distance Traveled (m)",
         type = "l", ...)
lines.particle <- function(x, ...)
    lines(startTime(x) + seq_along(x$distance) - 1, x$distance, ...)
size <- function(x) length(x$distance)
startTime <- function(x) x$t0

collect <- function(...)
    structure(list(...), class = "particle.list")
fleet <- function(N, ...) {
    x <- do.call(collect, lapply(1:N, function(x) particle(...)))
    attr(x, "Rd") <- x[[1]]$Rd
    attr(x, "Sd") <- x[[1]]$Sd
    x
}
print.particle.list <- function(x, ...)
    cat("A collection of", length(x), "particles.\n")
plot.particle.list <- function(x, ...,
                               xlim = c(min(sapply(x, startTime)),
                                        max(sapply(x, size))),
                               ylim = c(0, max(sapply(x, function(y) max(y$d))))) {
    plot(NA, xlim = xlim, ylim = ylim,
         xlab = "Time (min)", ylab = "Distance Traveled (km)",
         xaxt = "n", yaxt = "n")
    axis(1, at = pretty(xlim / 60) * 60, labels = pretty(xlim / 60))
    axis(2, at = pretty(ylim / 1000) * 1000, labels = pretty(ylim / 1000),
         las = 1)
    if (!is.null(attr(x, "Rd")))
        abline(h = attr(x, "Rd"), lty = 2, col = "orangered")
    if (!is.null(attr(x, "Sd")))
        abline(h = attr(x, "Sd"), lty = 3, col = "cyan")
    sapply(x, lines, ...)
    invisible(NULL)
}
lines.particle.list <- function(x, ...)
    sapply(x, lines, ...)

initializeParticle <- function(dmax, Sd, Rd, ...) {
    if (missing(dmax)) dmax = max(Sd)
    d <- 0
    v <- 0
    x <- d
    structure(mutate(x, Sd, Rd, D = dmax, ...),
              "t0" = 0, "Sd" = Sd, "Rd" = Rd)
}

mutate <- function(x, sd, rd, Dmax,
                   amin = -5, Vmax = 30, vmin = 0, sigv = 2,
                   pi = 0.5, rho = 0.5, gamma = 3, tau = 6, theta = 20, ...) {
    d <- x[length(x)]
    v <- 0
    if (length(x) > 1)
        v <- diff(x)[length(x)-1]
    j <- 1; J <- 1
    if (!missing(sd)) {
        J <- length(sd)
        j <- which(sd > d)[1] - 1
    }
    l <- 1; L <- 1
    if (!missing(rd)) {
        L <- length(rd)
        l <- tail(which(rd <= d), 1)
    }
    pstops <- -1
    while (d < Dmax) {
        if (v == 0) while(runif(1) < 0.9) x <- c(x, d)
        if (l < L && rd[l+1] < sd[j+1]) {
            dmax <- rd[l+1]
            if (pstops == -1) pstops <- rbinom(1, 1, rho)
        } else {
            dmax <- sd[j+1]
            if (pstops == -1) pstops <- rbinom(1, 1, pi)
        }
        vmax <- (dmax - d) / sqrt((dmax - d) / -amin)
        if (pstops == 1 && vmax < Vmax) v <- runif(1, vmin, vmax)
        else v <- rtruncnorm(1, vmin, Vmax, v, sigv)
        d <- d + v
        if (d >= dmax) {
            d <- dmax
            if (pstops == 1) v <- 0
            if (dmax == sd[j+1]) {
                j <- j + 1
                wait <- round(gamma * rexp(1, 1 / tau))
            } else {
                l <- l + 1
                wait <- round(rexp(1, 1 / theta))
            }
            x <- c(x, rep(d, wait))
            pstops <- -1
        }
        x <- c(x, d)
    }
    x
}
loglh <- function(p, t, pos, shape, sigma = 10, R = 6371e3) {
    ## get particle distance at time t
    if (length(p$distance) >= t+1) {
        d <- p$distance[t+1] ## because t is 0-based
        y <- h(d, shape)
    } else y <- shape[nrow(shape), c("lat", "lng")]#h(max(p$distance), shape)
    ## c(x, y):
    z <- c((y$lng - pos$lng) * pi / 180 * sin(pos$lat * pi / 180),
           (y$lat - pos$lat) * pi / 180) * R
    - log(2 * pi * sigma) - sum(z^2) / (2 * sigma^2)
}
h <- function(d, shp) {
    LL <- shp[, c("lat", "lng")]
    if (is.na(d)) return(LL[nrow(LL), ])
    dt <- shp$dist_traveled
    if (d == 0) return(LL[1,])
    wl <- dt <= d
    if (all(wl)) return(LL[nrow(LL),])
    i <- max(which(wl))    
    ## distance along line here ...
    if (d == dt[i]) return(LL[i, ])
    dv <- d - dt[i]
    th <- bearing(LL[i, ], LL[i+1, ])
    dp <- destPt(LL[i, ], th, dv)
    return(data.frame(lat = dp[1], lng = dp[2]))
}
bearing <- function(a, b) {
    ## compute the bearing between two points
    a <- as.numeric(a)
    b <- as.numeric(b)
    ## convert to radians!!
    phi.a <- a[1] * pi / 180
    phi.b <- b[1] * pi / 180
    lam.a <- a[2] * pi / 180
    lam.b <- b[2] * pi / 180
    th.rad <- atan2(sin(lam.b - lam.a) * cos(phi.b),
                    cos(phi.a) * sin(phi.b) - sin(phi.a) * cos(phi.b) * cos(lam.b - lam.a))
    (th.rad * 180 / pi + 360) %% 360
}
destPt <- function(a, theta, d, R = 6371e3) {
    a <- as.numeric(a)
    phi1 <- a[1] * pi / 180
    lam1 <- a[2] * pi / 180
    theta <- theta * pi / 180
    phi2 <- asin(sin(phi1) * cos(d / R) +
                 cos(phi1) * sin(d / R) * cos(theta))
    lam2 <- lam1 + atan2(sin(theta) * sin(d / R) * cos(phi1),
                         cos(d / R) - sin(phi1) * sin(phi2))
    lat <- phi2 * 180 / pi
    lng <- lam2 * 180 / pi
    lng <- (lng + 540) %% 360 - 180
    c(lat, lng)
}
