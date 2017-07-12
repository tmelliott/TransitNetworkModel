library(RSQLite)
con <- dbConnect(SQLite(), "../gtfs.db")

## 1. read particles (+ ETAs)
particles <- read.csv("../build/PARTICLES.csv",
                      colClasses = c("factor", "factor", "integer", "factor",
                                     "numeric", "numeric", "integer", "numeric", "numeric"))
particles$timestamp <- as.POSIXct(particles$timestamp, origin = "1970-01-01")
etas <- read.csv("../build/ETAS.csv",
                 colClasses = c("factor", "factor", "integer", "integer"))
etas$eta <- as.POSIXct(etas$eta, origin = "1970-01-01")

routes <- dbGetQuery(con, sprintf("SELECT DISTINCT route_id FROM trips WHERE trip_id IN ('%s') ORDER BY route_id",
                                  paste(unique(particles$trip_id), collapse = "','")))$route_id
shapes <- dbGetQuery(con,
                     sprintf("SELECT DISTINCT shape_id FROM routes WHERE route_id IN ('%s')",
                             paste(routes, collapse = "','")))$shape_id

## 2. read their shapes (including segments)
shapeseg <- lapply(shapes, function(sid) {
    dbGetQuery(con, sprintf("SELECT segment_id, leg, shape_dist_traveled FROM shape_segments WHERE shape_id='%s' ORDER BY leg", sid))
})
names(shapeseg) <- shapes

## 3. read their stop_times (for shape_dist_traveled)
stops <- lapply(routes, function(rid) {
    dbGetQuery(con,
               sprintf("SELECT stop_sequence, shape_dist_traveled FROM stop_times
WHERE trip_id IN (SELECT trip_id FROM trips WHERE route_id='%s' LIMIT 1) ORDER BY stop_sequence",
                       rid))
})
names(stops) <- routes

## Picture it:
for (i in seq_along(routes)) {
    o <- par(mar = c(5.1, 2.1, 2.1, 2.1))
    with(stops[[i]], {
        plot(shape_dist_traveled, rep(0, length(shape_dist_traveled)), type = "l",
             lwd = 2, xaxs = "i", ylab = "", yaxt = "n", xlab = "Distance (m)", ylim = c(-1, 1))
        points(shape_dist_traveled, rep(0, length(shape_dist_traveled)),
               pch = 21, bg = "white", lwd = 2)
    })
    with(shapeseg[[i]][-1,], {
        arrows(shape_dist_traveled, -0.2, y1=0.2, code = 0, col = "#990000", lwd = 2)
    })
    locator(1)
    par(o)
}


particles
