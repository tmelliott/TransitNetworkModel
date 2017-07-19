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
               type       = ifelse(stu$arrival$time == 0, "departure", "arrival"))
}))
delays$delay[delays$delay > 30*60] <- 30*60
delays$delay[delays$delay < -30*60] <- -30*60
vc <- c(viridis(29, end = 0.6), rep(viridis(1, begin=0.7), 6),
        magma(10, begin=0.6, direction=-1),
        rep(magma(1, begin=0.6), 15))
#layout(rbind(c(1, 1, 1, 2)))
hist(delays$delay/60, breaks=-30:30, col=vc)
## dfact <- factor(ifelse(delays$delay/60 < -1, "early",
##                 ifelse(delays$delay/60 <= 5, "ontime", "late")),
##                 levels = c("early", "ontime", "late"))
}

while(TRUE) {
    delayHist()
    Sys.sleep(1)
}

colf <- function(x) {
    ifelse(x < -1, "purple",
    ifelse(x <= 5, "green4",
    ifelse(x <= 10, "orange",
           "red")))
}




vc <- viridis(1, begin=0.7)
barplot(rep(1, length(vc)), col = vc, names=(-30:30)[1:length(vc)])
