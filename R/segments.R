library(ggmap)
library(RSQLite)
con = dbConnect(SQLite(), "../gtfs.db")

segs = dbGetQuery(con, "SELECT * FROM segment_pt ORDER BY segment_id, seg_pt_sequence")
## segs2 = do.call(rbind, tapply(1:nrow(segs), segs$segment_id, function(i) {
##     segs[c(min(i), max(i)), ]
## }))

xr = extendrange(segs$lng)
yr = extendrange(segs$lat)
bbox = c(xr[1], yr[1], xr[2], yr[2])
akl = get_stamenmap(bbox, zoom = 11, maptype = "toner-lite")

ggmap(akl) +
    geom_path(aes(x = lng, y = lat, group = segment_id),
              data = segs, color = "#00000020")


## ggplot() +
##     geom_path(aes(x = lng, y = lat, group = segment_id),
##               data = segs2[segs2$seg_dist_traveled < 2000, ], color = "#00000020")



routeid = "10009-20170602141618_v54.27"
shape = dbGetQuery(con, sprintf("SELECT * FROM shapes, segment_pt WHERE shapes.segment_id=segment_pt.segment_id AND shape_id=(SELECT shape_id FROM routes WHERE route_id='%s') ORDER BY leg, seg_pt_sequence", routeid))
xr = extendrange(shape$lng)
yr = extendrange(shape$lat)
bbox = c(xr[1], yr[1], xr[2], yr[2])
akl = get_stamenmap(bbox, zoom = 14, maptype = "toner-lite")

stops = dbGetQuery(con, sprintf("SELECT stops.stop_id, lat, lng, shape_dist_traveled FROM stops, stop_times WHERE stops.stop_id=stop_times.stop_id AND trip_id=(SELECT trip_id FROM trips WHERE route_id='%s' LIMIT 1) ORDER BY stop_sequence", routeid))

plot(stops$shape_dist_traveled, type="b")

shape$d = shape$shape_dist_traveled + shape$seg_dist_traveled
stop.est = cbind(stop_id = stops$stop_id, do.call(rbind, lapply(stops$shape_dist_traveled, function(x) {
    shape[which(shape$d >= x)[1], c("lat", "lng")]
})))

ggmap(akl) +
    geom_path(aes(x = lng, y = lat, group = segment_id, color = as.factor(segment_id)),
              data = shape, lwd = 2) +
    geom_point(aes(lng, lat), data = stops, size = 3) +
    geom_point(aes(lng, lat), data = stop.est, color = "red") +
    geom_text(aes(lng, lat, label = stop_id), data = stop.est, color = "blue")
