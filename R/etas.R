tt <- function(x) as.POSIXct(x, origin="1970-01-01")
library(RProtoBuf)
library(RSQLite)

cummean <- function(x) sapply(seq_along(x), function(i) mean(x[1:i]))
stopTimes <- function(trip, con) {
    qq <- dbSendQuery(con, "SELECT stop_sequence, arrival_time, departure_time, shape_dist_traveled FROM stop_times where trip_id=? ORDER BY stop_sequence")
    dbBind(qq, list(trip))
    res <- dbFetch(qq)
    dbClearResult(qq)
    res
}

readProtoFiles(dir="../proto")

etas.raw <- read(transit_etas.Feed, "~/Dropbox/gtfs/etas.pb")$trips
all <- do.call(rbind, lapply(etas.raw, function(x)
    data.frame(v=x$vehicle_id, r=x$route_id, delay=x$delay, d=x$distance_into_trip, n=length(x$etas))))
#all <- all[order(all$d), ]
#all <- all[all$d >= 0 & all$n > 0, ]
#head(all, 20)
iNZightPlots::iNZightPlot(all$delay/60, cex.dotpt = 0.3)

ALL <- read(transit_realtime.FeedMessage, "~/Dropbox/gtfs/trip_updates.pb")$entity
delays <- do.call(rbind, lapply(ALL, function(x) {
    tu <- x$trip_update$stop_time_update[[1]]
    return(data.frame(v = x$trip_update$vehicle$id, delay = ifelse(is.null(tu$arrival), tu$departure$delay, tu$arrival$delay)))
}))
delays <- delays[delays$delay > min(all$delay) & delays$delay < max(all$delay), ]
iNZightPlots::iNZightPlot(delays$delay/60, cex.dotpt = 0.3, xlim = range(all$delay/60))

vid <- "2F60"
xr <- NULL

delays[delays$v == vid, ]

while (TRUE) {
    etas.raw <- read(transit_etas.Feed, "~/Dropbox/gtfs/etas.pb")$trips
    etas <- do.call(rbind, lapply(etas.raw, function(x) {
        if (x$vehicle_id != vid) return(NULL)
        x1 <- data.frame(vehicle_id = x$vehicle_id,
                        trip_id = x$trip_id,
                        route_id = x$route_id,
                        dist = x$distance_into_trip,
                        delay = x$delay)
        x2 <- do.call(rbind, lapply(x$etas, function(y) as.data.frame(as.list(y))))
        if (!is.null(x2)) return(data.frame(x1, x2))
        return(x2)
    }))
    print(etas$delay[1])
    if (is.null(etas) || length(dim(etas)) != 2 || nrow(etas) == 0) {
        Sys.sleep(20)
        next()
    }
    if (is.null(xr))
        xr <- tt(range(etas$arrival_min, etas$arrival_max))
    con <- dbConnect(SQLite(), "../gtfs.db")
    ## for (vid in levels(etas$vehicle_id)) {
    ## with(etas[etas$route_id == rid, ], {
    jpeg(sprintf("~/Desktop/eg/etas_%s_%s.jpg", vid,
                 format(file.info("~/Dropbox/gtfs/etas.pb")$mtime, "%H:%M:%S")),
        width = 1000, height = 700)
    with(etas, {##etas$vehicle_id == vid, ], {
        ##vint <- as.numeric(droplevels(vehicle_id))
        clrs <- "black" ##viridis::viridis(length(unique(vint)))[vint]
        tts <- stopTimes(trip_id[1], con)
        tts$time <- as.POSIXct(paste(Sys.Date(), tts$arrival_time))
        tti <- tts[tts$stop_sequence >= min(stop_sequence), ]
        plot(tt(arrival_eta), tts$shape_dist_traveled[stop_sequence],
             ylim = c(0, max(tts$shape_dist_traveled)),
             xlim = xr, xaxt = "n", type = "n",
             col = clrs, pch = 19,
             xlab = "ETA", ylab = "Distance into Trip (m)",
             main = sprintf("Route: %s; Trip: %s; Vehicle: %s",
                            route_id[1], trip_id[1], vid))
        abline(v = pretty(xr), col = "#cccccc")
        points(tti$time, tti$shape_dist_traveled, pch = 19, cex = 0.5, col = "orangered")
        arrows(tti$time, tti$shape_dist_traveled, tti$time + delay[1], code = 0, col = "orangered")
        points(tti$time + delay[1], tti$shape_dist_traveled, pch = 4, col = "orangered")
        axis(1, pretty(xr), format(pretty(xr), "%T"))
        arrows(tt(arrival_min), tts$shape_dist_traveled[stop_sequence], tt(arrival_max),
               code = 0, col = clrs)
        points(tt(arrival_eta), tts$shape_dist_traveled[stop_sequence],
               pch = 21, col = clrs, lwd = 2,
               bg = ifelse(cummin(certainty == 1), "black",
                    ifelse(certainty == 1, "yellow", "white")))
        abline(h = dist[1], col = "magenta", lty = 2)
    })
    dbDisconnect(con)
    dev.off()
    Sys.sleep(10)
}

