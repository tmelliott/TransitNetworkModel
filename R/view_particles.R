library(RSQLite)

con = dbConnect(SQLite(), "../gtfs.db")

routes = dbGetQuery(con, "SELECT routes.route_id, routes.route_short_name as number, MAX(shapes.shape_dist_traveled + segments.length) AS len FROM routes, shapes, segments WHERE routes.shape_id=shapes.shape_id AND shapes.segment_id=segments.segment_id AND route_id LIKE '%v54.27' GROUP BY shapes.shape_id")
stops = dbGetQuery(con, "SELECT MIN(routes.route_id) AS route_id, MIN(stop_times.stop_sequence) AS stop_sequence, MIN(stop_times.shape_dist_traveled) AS distance FROM routes, trips, stop_times WHERE routes.route_id=trips.route_id AND trips.trip_id=stop_times.trip_id AND routes.route_id LIKE '%54.27' GROUP BY routes.route_id, stop_times.stop_sequence ORDER BY routes.route_id, stop_times.stop_sequence")
lens <- data.frame(len = routes[, "len"], num = routes[, "number"])
rownames(lens) <- routes$route_id
stops$progress <- stops$distance / lens[as.character(stops$route_id), ]$len * 100

while (TRUE) {
    t0 = as.numeric(Sys.time()) - 5 * 60 ## last 5 mins
    try({
        particles = dbGetQuery(con, sprintf("SELECT vehicle_id, (particles.trip_id) as trip_id, route_id, (distance) as distance, log_likelihood, initialized FROM particles, trips WHERE particles.trip_id=trips.trip_id AND timestamp > %s", t0))
        #AND route_id IN (SELECT route_id FROM routes WHERE route_short_name IN ('274', '224', '277', '258'))")
    })
    particles$vehicle <- as.factor(particles$vehicle_id)
    particles$progress <- particles$distance / lens[particles$route_id, "len"] * 100
    with(particles[particles$progress > 1 & particles$progress < 99, ], {
        route = as.factor(as.character(route_id))
        par(mar = c(5.1, 2.1, 1.1, 2.1))
        lh = exp(log_likelihood)
        wt = lh / sum(lh)
        plot(progress, as.numeric(route), xlim = c(0, 100), pch = 19, cex = 1,
             col = ifelse(initialized==1, "#00990040", "#cccccc40"),
             yaxt = "n", ylab = "", xaxs = "i", xlab = "Progress (%)")
        abline(h = 1:length(levels(route)), lty = 1, lwd = 2, col = "#666666")
        with(stops[stops$route_id %in% as.character(route), ],
             points(progress, as.factor(as.character(route_id)), pch = 21, col = "#666666", bg = "white", lwd = 2))
        points(progress, as.numeric(route), cex = 3 * wt, col = "#990000", pch = 19)
        axis(2, at = 1:length(levels(route)), lens[levels(route), "num"], las = 2, cex.axis = 0.4)
        axis(4, at = 1:length(levels(route)), lens[levels(route), "num"], las = 2, cex.axis = 0.4)
    })
    Sys.sleep(10)
}
