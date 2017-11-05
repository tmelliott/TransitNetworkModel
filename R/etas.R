tt <- function(x) as.POSIXct(x, origin="1970-01-01")
library(RProtoBuf)

readProtoFiles(dir="../proto")

etas <- read(transit_etas.Feed, "../build/gtfs_etas.pb")$trips
etas <- do.call(rbind, lapply(etas, function(x) {
    x <- try({
        data.frame(vehicle_id = x$vehicle_id, trip_id = x$trip_id, route_id = x$route_id,
                   do.call(rbind, lapply(x$etas, function(y) as.data.frame(as.list(y)))))
    })
    if (!inherits(x, "try-error")) return(x) else return(NULL)
}))
#o <- par(mfrow = c(length(levels(etas$route_id)), 1))
xr <- tt(range(etas$arrival_min, etas$arrival_max))
for (rid in levels(etas$vehicle_id)) {
    ##with(etas[etas$route_id == rid, ], {
    with(etas[etas$vehicle_id == rid, ], {
        #clrs <- as.numeric(droplevels(vehicle_id))
        clrs <- "black"#viridis::viridis(length(unique(clrs)))[clrs]
        plot(tt(arrival_eta), stop_sequence, ylim = c(0, max(stop_sequence)),
             xlim = xr, xaxt = "n",
             col = clrs, pch = 19, main = rid)
        abline(v = pretty(xr), col = "#cccccc")
        axis(1, pretty(xr), format(pretty(xr), "%T"))
        arrows(tt(arrival_min), stop_sequence, tt(arrival_max), code = 0,
               col = clrs)
    })
    locator(1)
}
#par(o)
