library(RSQLite)

con = dbConnect(SQLite(), "../gtfs.db")

routes = dbGetQuery(con, "SELECT route_id, MAX(shape_dist_traveled + length) AS len FROM routes, shapes, segments WHERE shapes.shape_id IN (SELECT shape_id FROM routes WHERE routes.shape_id = shapes.shape_id AND route_id LIKE '%v54.17') AND shapes.segment_id = segments.segment_id GROUP BY route_id ORDER BY leg")
lens <- data.frame(len = routes[, "len"])
rownames(lens) <- routes$route_id

while (TRUE) {
    try({
        particles = dbGetQuery(con, "SELECT vehicle_id, particles.trip_id, route_id, distance FROM particles, trips WHERE particles.trip_id=trips.trip_id AND route_id IN (SELECT route_id FROM routes WHERE route_short_name IN ('274', '224', '277', '258'))")
    })
    particles$vehicle <- as.factor(particles$vehicle_id)
    particles$progress <- particles$distance / lens[particles$route_id, "len"] * 100
    with(particles, {
        plot(progress, as.numeric(vehicle), xlim = c(0, 100), pch = 3, cex = 0.5,
             yaxt = "n", ylab = "", xaxs = "i", xlab = "Progress (%)")
        axis(2, at = 1:length(levels(vehicle)), levels(vehicle), las = 2)
        abline(h = 1:length(levels(vehicle)), lty = 3, col = "#cccccc")
    })
    Sys.sleep(10)
}
