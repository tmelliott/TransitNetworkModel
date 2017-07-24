library(viridis)
library(RProtoBuf)
readProtoFiles(dir="../protobuf")

delayHist <- function() {
    delays <- read(transit_realtime.FeedMessage, "~/Dropbox/gtfs/trip_updates.pb")$entity
    delays <- do.call(rbind, lapply(delays, function(x) {
        tu <- x$trip_update
        stu <- tu$stop_time_update[[1]]
        data.frame(vehicle_id = tu$vehicle$id,
                   trip_id    = tu$trip$trip_id,
                   route_id   = tu$trip$route_id,
                   delay      = ifelse(stu$arrival$time == 0, stu$departure$delay, stu$arrival$delay),
                   type       = ifelse(stu$arrival$time == 0, "Departure", "Arrival"))
    }))
    delays$delay[delays$delay > 30*60] <- 30*60
    delays$delay[delays$delay < -30*60] <- -30*60
    vc <- c(viridis(29, end = 0.6), rep(viridis(1, begin=0.7), 6),
            magma(10, begin=0.6, direction=-1),
            rep(magma(1, begin=0.6), 15))
    opar <- par(mfrow = c(2, 1))
    on.exit(par(opar))
    for (type in c("Departure", "Arrival")) {
        h <- hist(delays[delays$type == type, ]$delay/60, breaks=-30:30, col=vc,
                  xlab = "Delay (min)", main = paste(type, "delays"))
        abline(v = c(-1, 5), lty = 2)
        text(x = c(-2, 6), y = max(h$counts) * 0.8, c("early", "late"),
             pos = c(2, 4))
        arrows(c(-1, 5), max(h$counts) * 0.75, c(-6, 10), code = 2, length = 0.1)
    }    
}

while(TRUE) {
    #jpeg("~/Dropbox/gtfs/delays.jpg", width = 900, height = 500)
    delayHist()
    #dev.off()
    Sys.sleep(30)
}

