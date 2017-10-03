library(RSQLite)
#library(ggmap)
library(RProtoBuf)
library(viridis)

print.gtfs.segment <- function(x, ...) {
    cat(sprintf("Segment %d: Current travel time = %.2f (%.2f) [updated %s]\n",
                x$id, x$mean, x$var, format(x$ts, "%T")))
}

readProtoFiles(dir = "../proto")

plotRoute <- function(routeid, maxSpeed = 100, .max = maxSpeed * 1000 / 60 / 60) {
    tt <- function(x) as.POSIXct(x, origin="1970-01-01")
    segments <- read(transit_network.Feed, "../build/gtfs_network.pb")$segments
    segments <- segments[sapply(segments, function(x) x$timestamp > 0)]
    segments <- lapply(segments, function(x) {
        structure(list(id = x$segment_id,
                       mean = x$travel_time, var = x$travel_time_var,
                       ts   = tt(x$timestamp)), class = "gtfs.segment")  
    }); names(segments) <- sapply(segments, function(x) x$id)

    ## vs <- read(transit_etas.Feed, "../build/gtfs_etas.pb")$trips
    ## vs <- do.call(rbind, lapply(vs, function(x) {
    ##     data.frame(vehicle_id = x$vehicle_id, trip_id = x$trip_id,
    ##                route_id = x$route_id,
    ##                distance = x$distance_into_trip,
    ##                velocity = x$velocity)
    ## }))


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
             type = "l", lwd = 4,
             yaxt = "n", xlab = "Distance (m)", ylab = "")
        points(c(shape_dist_traveled, max(shape$dist_traveled)), rep(0, length(tt)+1), pch = 19)
        mid <- shape_dist_traveled + diff(c(shape_dist_traveled, max(shape$dist_traveled))) / 2
        lab <- ifelse(is.na(tt), "", sprintf("%.1f (%.1f)", tt, sqrt(ttvar)))
        text(mid, rep(0, length(mid)), lab, pos = c(1, 3), cex = 0.7)
        l2 <- ifelse(is.na(state), "", sprintf("%.0f", state * 60 * 60 / 1000))
        text(mid, rep(0, length(mid)), l2, pos = c(1, 3), offset = 1.5, cex = 0.7)
    })
    #with(vs[vs$route_id == routeid, ],
    #     abline(v = distance, col = "#990000", lty = 2))
    
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
    ## ps <- read.csv("../build/PARTICLES.csv",
    ##                colClasses = c("factor", "factor", "integer", "factor", "factor",
    ##                               "numeric", "numeric", "integer", "numeric", "numeric", "numeric", "integer"))
    o <- par(mfrow = c(2, 1), mar = c(5.1, 2.1, 2.1, 2.1))
    dev.hold()
    plotRoute(ri <- r274$route_id[2])
    ## pp <- with(ps[ps$route_id == ri, ], tapply(distance, factor(vehicle_id), mean))
    ## px <- with(ps[ps$route_id == ri, ], tapply(parent_id, factor(vehicle_id), mean))
    ## points(pp, rep(0, length(pp)), col = ifelse(px == 0, "green", "magenta"), cex = 1, pch = 4)
    plotRoute(ri <- r274$route_id[4])
    ## pp <- with(ps[ps$route_id == ri, ], tapply(distance, factor(vehicle_id), mean))
    ## px <- with(ps[ps$route_id == ri, ], tapply(parent_id, factor(vehicle_id), mean))
    ## points(pp, rep(0, length(pp)), col = ifelse(px == 0, "green", "magenta"), cex = 1, pch = 4)
    dev.flush()
    par(o)
    Sys.sleep(2)
}




## Plot the network map ...
library(sp)
library(magrittr)
library(ggplot2)
library(dplyr)
library(ggmap)

graph <- function() {
    nw <- read(transit_network.Feed, "~/Dropbox/gtfs/nws.pb")
    ## lines <- lapply(nw$segments, function(x) {
    ##     line <- Line(cbind(c(x$start$lng, x$end$lng),
    ##                        c(x$start$lat, x$end$lat)))
    ##     lines <- Lines(list(line), ID = x$segment_id)
    ##     lines
    ## })
    ## sl <- SpatialLines(lines, proj4string = CRS("+init=epsg:4326"))
    ## lens <- sapply(lines, LinesLength, longlat = TRUE)
    segs <- do.call(rbind, lapply(nw$segments, function(x) {
        line <- Line(cbind(c(x$start$lng, x$end$lng),
                           c(x$start$lat, x$end$lat)))
        lines <- Lines(list(line), ID = x$segment_id)
        len <- LinesLength(lines, TRUE)
        data.frame(id = as.character(x$segment_id),
                   travel.time = ifelse(x$travel_time == 0, NA, x$travel_time),
                   var = ifelse(x$travel_time_var == 0, NA, x$travel_time_var),
                   timestamp = x$timestamp,
                   x.start = x$start$lng, x.end = x$end$lng,
                   y.start = x$start$lat, y.end = x$end$lat,
                   length = len)
    }))
    segs$speed <- pmin(100, segs$length / segs$travel.time * 60 * 60)
    
    ## spdf <- SpatialLinesDataFrame(sl, segs)
    ## segd <- fortify(spdf) %>%
    ##     left_join(segs, by = "id") %>%
    ##     filter(!is.na(travel.time))
    segd <- segs %>%
        filter(!is.na(travel.time)) %>%
        filter(x.start != x.end) %>% filter(y.start != y.end)
    ## xr <- extendrange(range(segd$x.start, segd$x.end))
    ## yr <- extendrange(range(segd$y.start, segd$y.end))
    ## bbox <- c(xr[1], yr[1]+0.2, xr[2], yr[2]-0.1)
    ## akl <- get_stamenmap(bbox, zoom = 10, maptype = "toner-background")
    
    p <- ggmap(akl, darken = 0.85) +
        coord_cartesian() +
        geom_curve(aes(x.start, y.start, xend = x.end, yend = y.end,
                       colour = speed),
                   curvature = -0.1,
                   data = segd, alpha = 0.5, lineend = "round") +
        scale_colour_viridis(option = "plasma", begin = 0.2)
    pdf("~/Dropbox/gtfs/nws.pdf", width = 10, height = 12)
    print(p)
    dev.off()
    invisible(p)
}

while (TRUE) { try(graph()); Sys.sleep(1) }
