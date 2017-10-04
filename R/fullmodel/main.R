library(RProtoBuf)
library(RSQLite)
library(ggmap)

## 0. settings etc.
DATE <- "2017-10-04"
HOME <- path.expand("~/Documents/uni/TransitNetworkModel")
PI.IP <- "tom@130.216.51.230"
PI.DIR <- "/mnt/storage/historical_data"


## 1. read protofiles from a day into a database; return connection
readProtoFiles(dir = file.path(HOME, "proto"))

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
    datef <- gsub("-", "", date)
    cmd <- sprintf("ssh %s find %s -type f -name 'vehicle_locations_%s*.pb'", ip, dir, datef)
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

#makeData(DATE, PI.IP, PI.DIR)


## 2. connect to database and, for a given route, fetch GTFS information
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

    dat <- dat %>% mutate(vehicle_id = as.factor(vehicle_id)) %>%
        merge(rid, by = "route_id")

    class(shapes) <- c("gtfs.shape", class(shapes))
    attr(dat, "shapes") <- shapes
    class(segments) <- c("gtfs.segments", class(segments))
    attr(dat, "segments") <- segments
    class(stops) <- c("gtfs.stops", class(stops))
    attr(dat, "stops") <- stops
    class(dat) <- c("gtfs.data", class(dat))

    

    return(dat)
}

dd <- getRouteData("274", "busdata.db", file.path(HOME, "gtfs.db"))
vstart <- tapply(1:nrow(dd), dd$vehicle_id, function(x) x[1])

vi <- vstart[2]
p <- plot(getShape(dd, id = dd$shape_id[vi]), zoom = 14)
p <- p + geom_point(aes(lng, lat), data = getStops(dd, id = dd$trip_id[vi]),
                    size = 2, colour = "orangered", pch = 21, fill = "black",
                    stroke = 1.5)
p

R <- 6371e3
dd1 <- dd %>% filter(vehicle_id == vehicle_id[vi], trip_id == trip_id[vi]) %>%
    arrange(timestamp) %>%
    ## compute times ...
    mutate(x = (lng * pi / 180 - mean(lng * pi / 180)) * cos(lat * pi / 180) * R,
           y = (lat - mean(lat)) * pi / 180 * R) %>%
    mutate(dist = c(sqrt(diff(x)^2 + diff(y)^2), 0),
           tdiff = c(diff(timestamp), 0)) %>%
    mutate(speed = dist / tdiff)
class(dd1) <- c("gtfs.data", class(dd1))

p + geom_path(aes(lng, lat, colour = speed), data = dd1, lwd = 2) +
    scale_colour_viridis()



## 3. implement a STAN model for n = 0, ..., N observations (estimating segment travel times!)

flatX <- function(x, z) (x - z) * pi / 180 * sin(z * pi / 180 * R)
flatY <- function(y, z) (y - z) * pi / 180 * R

sh <- getShape(dd, id = dd$shape_id[vi])
z <- c(mean(sh$lng), mean(sh$lat))
dat <- list(N = nrow(dd1),
            lng = flatX(dd1$lng, z[1]),
            lat = flatY(dd1$lat, z[2]),
            t = dd1$timestamp - min(dd1$timestamp) + 1,
            Q = nrow(sh),
            sx = flatX(sh$lng, z[1]),
            sy = flatY(sh$lat, z[2]),
            sdist = round(sh$dist_traveled),
            pi = pi)
dat$Nt = max(dat$t)

fit <- stan(file = "model.stan", data = dat,
            iter = 1000, chains = 4)

print(fit)

plot(fit, pars = "d")