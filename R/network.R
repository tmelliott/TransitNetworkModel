library(RSQLite)
#library(ggmap)
library(RProtoBuf)
library(viridis)

print.gtfs.segment <- function(x, ...) {
    cat(sprintf("Segment %d: Current travel time = %.2f (%.2f) [updated %s]\n",
                x$id, x$mean, x$var, format(x$ts, "%T")))
}

readProtoFiles(dir="../protobuf")

plotRoute <- function(routeid, maxSpeed = 100, .max = maxSpeed * 1000 / 60 / 60) {
    tt <- function(x) as.POSIXct(x, origin="1970-01-01")
    segments <- read(transit_network.Feed, "../build/gtfs_network.pb")$segments
    segments <- segments[sapply(segments, function(x) x$timestamp > 0)]
    segments <- lapply(segments, function(x) {
        structure(list(id = x$segment_id,
                       mean = x$travel_time, var = x$travel_time_var,
                       ts   = tt(x$timestamp)), class = "gtfs.segment")  
    }); names(segments) <- sapply(segments, function(x) x$id)


    ## Choose a route, fetch its shape and segments, and draw what's going on ...
    con <- dbConnect(SQLite(), "../gtfs.db")
    
    routeq <- dbSendQuery(con, "SELECT * FROM routes WHERE route_id=?")
    dbBind(routeq, list(routeid))
    route <- dbFetch(routeq)
    dbClearResult(routeq)
    
    shapeq <- dbSendQuery(con, "SELECT * FROM shapes WHERE shape_id=? ORDER BY seq")
    dbBind(shapeq, list(route$shape_id))
    shape <- dbFetch(shapeq)
    dbClearResult(shapeq)
    
    segq <- dbSendQuery(con, "SELECT segments.segment_id, shape_segments.leg, shape_segments.shape_dist_traveled FROM segments, shape_segments WHERE segments.segment_id=shape_segments.segment_id AND shape_segments.shape_id=? ORDER BY leg")
    dbBind(segq, list(route$shape_id))
    segs <- dbFetch(segq)
    dbClearResult(segq)
    dbDisconnect(con)
    
    segs$tt <- sapply(as.character(segs$segment_id), function(id)
        ifelse(is.null(segments[[id]]$mean), NA, segments[[id]]$mean))
    segs$ttvar <- sapply(as.character(segs$segment_id), function(id)
        ifelse(is.null(segments[[id]]$var), NA, segments[[id]]$var))
    shape$seg <- sapply(shape$dist_traveled, function(x) {
        as.character(segs$segment_id[sum(segs$shape_dist_traveled <= x)])
    })
    segs$length <- diff(c(segs$shape_dist_traveled, max(shape$dist_traveled)))
    segs$state <- with(segs, length / tt)
    rownames(segs) <- segs$segment_id
    shape$speed <- pmin(segs[shape$seg, "state"], .max) * 60 * 60 / 1000
    with(segs, {
        plot(c(shape_dist_traveled, max(shape$dist_traveled)), rep(0, length(tt)+1),
             type = "l", lwd = 2,
             yaxt = "n", xlab = "Distance (m)", ylab = "")
        points(c(shape_dist_traveled, max(shape$dist_traveled)), rep(0, length(tt)+1), pch = 19)
        mid <- shape_dist_traveled + diff(c(shape_dist_traveled, max(shape$dist_traveled))) / 2
        lab <- ifelse(is.na(tt), "", sprintf("%.1f (%.1f)", tt, sqrt(ttvar)))
        text(mid, rep(0, length(mid)), lab, pos = c(1, 3), cex = 0.7)
        l2 <- ifelse(is.na(state), "", sprintf("%.0f", state * 60 * 60 / 1000))
        text(mid, rep(0, length(mid)), l2, pos = c(1, 3), offset = 1.5, cex = 0.7)
    })
    
    ## xr = extendrange(shape$lng)
    ## yr = extendrange(shape$lat)
    ## bbox = c(xr[1], yr[1], xr[2], yr[2])
    ## akl = get_stamenmap(bbox, zoom = 14, maptype = "toner-lite")
    
    ## pl <- ggmap(akl) +
    ##     geom_path(aes(lng, lat, color = speed), data = shape, lwd = 2) +
    ##     ggtitle(sprintf("Updated %s", format(Sys.time(), "%T"))) +
    ##     scale_color_viridis(option = "magma")
    ## print(pl)

    invisible(segs)
}

con <- dbConnect(SQLite(), "../gtfs.db")
NEX <- dbGetQuery(con, "SELECT * FROM routes WHERE route_short_name='NEX'")
r274 <- dbGetQuery(con, "SELECT * FROM routes WHERE route_short_name='274'")
dbDisconnect(con)

while (TRUE) {
    o <- par(mfrow = c(2, 1))
    plotRoute(r274$route_id[2])
    plotRoute(r274$route_id[4])
    par(o)
    Sys.sleep(10)
}
