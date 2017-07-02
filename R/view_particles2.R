library(RSQLite)
library(ggmap)
if (grepl ("/R", getwd())) setwd("..")
con = dbConnect(SQLite(), "gtfs.db")

routes = dbGetQuery(con, "SELECT routes.route_id, routes.route_short_name as number, MAX(shapes.shape_dist_traveled + segments.length) AS len FROM routes, shapes, segments WHERE routes.shape_id=shapes.shape_id AND shapes.segment_id=segments.segment_id AND route_id LIKE '%v54.27' GROUP BY shapes.shape_id")
stops = dbGetQuery(con, "SELECT MIN(routes.route_id) AS route_id, MIN(stop_times.stop_sequence) AS stop_sequence, MIN(stop_times.shape_dist_traveled) AS distance, lat, lng FROM routes, trips, stop_times, stops WHERE stops.stop_id=stop_times.stop_id AND routes.route_id=trips.route_id AND trips.trip_id=stop_times.trip_id AND routes.route_id LIKE '%54.27' AND route_short_name IN ('274','277','258','NEX','881') GROUP BY routes.route_id, stop_times.stop_sequence ORDER BY routes.route_id, stop_times.stop_sequence")
lens <- data.frame(len = routes[, "len"], num = routes[, "number"])
rownames(lens) <- routes$route_id
stops$progress <- stops$distance / lens[as.character(stops$route_id), ]$len * 100
shapes = dbGetQuery(con, "SELECT * FROM segment_pt WHERE segment_id IN (SELECT DISTINCT segment_id FROM shapes WHERE shape_id IN (SELECT DISTINCT shape_id FROM routes WHERE route_short_name IN ('274','277','258','NEX','881'))) ORDER BY segment_id, seg_pt_sequence")
xr = extendrange(shapes$lng)
yr = extendrange(shapes$lat)
bbox = c(xr[1], yr[1], xr[2], yr[2])
akl = get_stamenmap(bbox, zoom = 13, maptype = "toner-lite")

try(while(dev.off()) TRUE, TRUE)
dev.new(); dev.new()

while (TRUE) {
    t0 = as.numeric(Sys.time()) - 5 * 60 ## last 5 mins
    try({
        particles = dbGetQuery(con, sprintf("SELECT vehicle_id, (particles.trip_id) as trip_id, route_id, (distance) as distance, log_likelihood, initialized, etas, lat, lng FROM particles, trips WHERE particles.trip_id=trips.trip_id AND timestamp > %s", t0))
    })
    particles$vehicle <- as.factor(particles$vehicle_id)
    particles$progress <- particles$distance / lens[particles$route_id, "len"] * 100
    ##jpeg(sprintf("~/Dropbox/PhD/routeprogress/routeprogress_%s.jpg", as.numeric(Sys.time())), width = 900, height = 700)
    with(particles, {#[particles$progress > 1 & particles$progress < 99, ], {
        vehicle <- as.factor(as.character(vehicle))
        par(mar = c(5.1, 2.1, 1.1, 2.1))
        lh = exp(log_likelihood)
        wt = 2 * lh / max(lh)
        dev.hold()
        plot(progress, as.numeric(vehicle), xlim = c(-2, 102), type = "n",
             yaxt = "n", ylab = "", xaxs = "i", xlab = "Progress (%)")
        sapply(seq_along(levels(vehicle)), function(i) {
            lines(c(0, 100), c(i, i), col = "#666666")
        })
        sapply(levels(vehicle), function(v) {
            with(stops[stops$route_id %in% unique(route_id[vehicle == v]), ],
                 points(progress, rep(which(levels(vehicle) == v), length(progress)),
                        pch = 21, col = "#666666", bg = "white", lwd = 1))
            text(0, which(levels(vehicle) == v), v, pos = 3)
            invisible(NULL)
        })
        #points(progress, as.numeric(vehicle), pch = 3, col = ifelse(initialized==1, "#00990020", "#cccccc30"))
        points(progress, as.numeric(vehicle), cex = wt, col = "#99000040", pch = 3)
        vr <- tapply(route_id, vehicle, function(x) x[1])
        axis(2, at = 1:length(levels(vehicle)), lens[vr, "num"], las = 3, cex.axis = 0.8)
        axis(4, at = 1:length(levels(vehicle)), lens[vr, "num"], las = 3, cex.axis = 0.8)
        ## ETAs
        try({
            sapply(levels(vehicle), function(v) {
                ETAs <- do.call(rbind, strsplit(etas[vehicle == v], ","))
                mode(ETAs) <- "integer"
                ETAs[is.na(ETAs)] <- 0
                tr <- apply(ETAs, 2, quantile, probs = c(0.05, 0.95))
                xx <- stops[stops$route_id %in% unique(route_id[vehicle == v]), "progress"]
                eta <- tr - as.numeric(Sys.time())
                eta[1, ] <- eta[1, ] / 60
                eta[2, ] <- eta[2, ] / 60
                etaMins <- ifelse(eta[2, ] <= 0, "",
                           ifelse(eta[2, ] < 1, "DUE",
                           ifelse(eta[1, ] < 1, sprintf("<%dmin", ceiling(eta[2, ])),
                                  sprintf("%d-%dmin", floor(eta[1, ]), ceiling(eta[2, ])))))
                text(xx, which(levels(vehicle) == v), etaMins, cex = 0.6, offset = 0.5,
                     pos = ifelse(seq_along(eta) %% 2 == 0, 1, 3))
            })
        }, TRUE)
        dev.flush()
        ## and on a map
        dev.set()
        pl <- ggmap(akl) +
            geom_path(aes(lng, lat, group = segment_id), data = shapes, color = "steelblue", lwd = 2) +
            geom_point(aes(lng, lat), data = stops, color = "black", size = 0.5) +
            geom_point(aes(lng, lat, colour = vehicle), data = particles, pch = 4)
        dev.flush()
        print(pl)
        dev.set()
    })
    ## dev.off()
    Sys.sleep(10)
}




