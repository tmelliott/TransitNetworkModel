library(ggmap)
library(RSQLite)
con = dbConnect(SQLite(), "../gtfs.db")

shapeid <- "935-20170602141618_v54.27"
intq <- dbSendQuery(con, "SELECT * FROM intersections WHERE intersection_id IN (SELECT from_id FROM segments WHERE segment_id IN (SELECT segment_id FROM shape_segments WHERE shape_id=?))")
dbBind(intq, list(shapeid))
ints <- dbFetch(intq)
dbClearResult(intq)

shapeq <- dbSendQuery(con, "SELECT * FROM shapes WHERE shape_id=?")
dbBind(shapeq, list(shapeid))
shape <- dbFetch(shapeq)
dbClearResult(shapeq)

segq <- dbSendQuery(con, "SELECT shape_segments.*, segments.from_id FROM shape_segments, segments WHERE shape_segments.segment_id=segments.segment_id AND shape_id=? ORDER BY leg")
dbBind(segq, list(shapeid))
segs <- dbFetch(segq)
dbClearResult(segq)

splits <- read.csv(textConnection("lat,lng
-36.92530526,174.7834191
-36.92514453,174.7856664
-36.92645396,174.7858481
-36.92665524,174.7835868
-36.9267484,174.7825668
-36.94114,174.78694
-36.94281692,174.7874025
-36.9525788,174.7915994
-36.95489354,174.7906727
-36.96836948,174.7985384
-36.96854673,174.7984435
-36.97216065,174.7868239
-36.97208,174.78488
-36.9810653,174.78209
-36.99083247,174.7835257
-36.99119059,174.7863485
-36.9982969,174.7875399
-36.9979941,174.7889481
-37.00179349,174.7893919
-37.00263,174.78679
-37.0028958,174.7850654
-37.00242535,174.7866618
-37.00171,174.78933
-37.00428,174.79048
-37.00585796,174.7914677
-37.00585,174.79116
-37.0043629,174.79041
-37.00210466,174.7947853
-37.00144489,174.8018347
-36.99347223,174.8429002
-36.99314317,174.8438305
-36.97986666,174.8501433
-36.97976,174.85121
-36.97961867,174.8529978
-36.97876585,174.8602656
-36.98416699,174.8700372
-36.98931048,174.8735268
-36.99143357,174.8736855
-36.99339303,174.8777478
-36.99437,174.87811
-36.99392619,174.8799821
-36.99309488,174.8824848"))

with(shape, plot(lng, lat, type = "l", asp=1.2))
with(shape[1, ], points(lng, lat, pch = 19, cex = 0.5))

 with(ints, points(lng, lat, col = "#990000", pch = 19, cex = 0.5))
points(splits[, 2], splits[, 1], col = "#000099", pch = 1:4)

bbox <- locator(2)
with(shape, plot(lng, lat, type = "l", asp=1.6, xlim = bbox$x, ylim = bbox$y))
with(ints, points(lng, lat, col = "#990000", pch = 19, cex = 0.5))
points(splits[, 2], splits[, 1], col = "#000099", pch = 1:4)


xr = extendrange(shape$lng)
yr = extendrange(shape$lat)
bbox = c(xr[1], yr[1], xr[2], yr[2])
akl = get_stamenmap(bbox, zoom = 14, maptype = "toner-lite")

ggmap(akl) +
    geom_path(aes(lng, lat), data = shape, color = "steelblue", lwd = 2) +
    geom_point(aes(lng, lat), data = ints, color = "steelblue", size = 2.5, pch = 21, fill = "white", stroke = 2)



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
akl = get_stamenmap(bbox, zoom = 12, maptype = "toner-lite")

stops = dbGetQuery(con, sprintf("SELECT stops.stop_id, lat, lng, shape_dist_traveled FROM stops, stop_times WHERE stops.stop_id=stop_times.stop_id AND trip_id=(SELECT trip_id FROM trips WHERE route_id='%s' LIMIT 1) ORDER BY stop_sequence", routeid))


shape$d = shape$shape_dist_traveled + shape$seg_dist_traveled
stop.est = cbind(stop_id = stops$stop_id, do.call(rbind, lapply(stops$shape_dist_traveled, function(x) {
    wi <- which(shape$d >= x)[1]
    if (wi == 1 || wi == nrow(shape)) return(shape[wi, c("lat", "lng")])
    p1 <- shape[wi-1, c("lat", "lng")]
    p2 <- shape[wi, c("lat", "lng")]
    phi1 <- (p1[1] * pi / 180)[[1]]
    phi2 <- (p2[1] * pi / 180)[[1]]
    lam1 <- (p1[2] * pi / 180)[[1]]
    lam2 <- (p2[2] * pi / 180)[[1]]
    R <- 6371000
    dx <- x - shape$d[wi-1]
    dx <- acos(sin(phi1) * sin(phi2) + cos(phi1) * cos(phi2) * cos(lam2 - lam1)) * R
    br <- atan2(sin(lam2 - lam1) * cos(phi2),
                cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(lam2 - lam1)) * 180 / pi
    th <- (br + 360) %% 360
    phi3 <- asin(sin(phi1) * cos(dx / R) + cos(phi1) * sin(dx / R) * cos(th))
    lam3 <- lam1 + atan2(sin(th) * sin(dx / R) * cos(phi1),
                         cos(dx / R) - sin(phi1) * sin(phi3))
    p3 <- c(phi3, lam3) * 180 / pi
    p3[2] <- (p3[2] + 540) %% 360 - 180
    p3
})))

plot(stops$shape_dist_traveled, type="b")
abline(h = c(unique(shape$shape_dist_traveled), max(shape$d)), lty = 3)

dev.set()

ggmap(akl) +
    geom_path(aes(x = lng, y = lat, group = segment_id, color = as.factor(segment_id)),
              data = shape, lwd = 2) +
    geom_point(aes(lng, lat), data = stops, size = 3) +
    geom_point(aes(lng, lat), data = stop.est, color = "white") +
    geom_text(aes(lng, lat, label = stop_id), data = stop.est, color = "blue")
