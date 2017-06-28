library(ggmap)
library(RSQLite)
con = dbConnect(SQLite(), "../gtfs.db")

routes = dbGetQuery(con, "SELECT route_id, shape_id FROM routes WHERE route_short_name='NEX'")
shape = dbGetQuery(con, sprintf("SELECT segment_pt.lat, segment_pt.lng FROM segment_pt, shapes WHERE segment_pt.segment_id=shapes.segment_id AND shape_id='%s' ORDER BY leg, seg_pt_sequence", routes$shape_id[6]))
stops = dbGetQuery(con, sprintf("SELECT * FROM stops, stop_times WHERE stops.stop_id=stop_times.stop_id AND trip_id IN (SELECT trip_id FROM trips WHERE route_id='%s' LIMIT 1) ORDER BY stop_sequence", routes$route_id[6]))

xr = extendrange(shape$lng)
yr = extendrange(shape$lat)
bbox = c(xr[1], yr[1], xr[2], yr[2])
akl = get_stamenmap(bbox = bbox, zoom = 14,  maptype = "toner-lite")

ggmap(akl) +
    geom_path(aes(lng, lat), data = shape, col = "#000099") +
    geom_point(aes(lng, lat), data = stops, col = "#990000")
