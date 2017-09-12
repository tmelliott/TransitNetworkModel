library(RSQLite)
library(ggplot2)

csv2db <- function(f, db = tempfile(fileext = ".db", tmpdir = "~/tmp")) {
    ## write the csv file f to a database, and return connection
    t <- tempfile(fileext = ".sql", tmpdir = "~/tmp")
    cat(file = t, sep = "",
        ".mode csv\n",
        ".import ", f, " particles")
    system(sprintf("cat %s | sqlite3 %s", t, db))
    unlink(t)

    dbConnect(SQLite(), db)
}

plotVehicle <- function(vid, db, obs, wait = FALSE, ...) {
    pq <- dbSendQuery(db, "SELECT particle_id, trip_id, timestamp, t, CAST(d AS REAL) AS d FROM particles WHERE vehicle_id=? ORDER BY particle_id, t")
    dbBind(pq, list(vid))
    p <- dbFetch(pq)
    dbClearResult(pq)
    mode(p$particle_id) <- "factor"
    p$timestamp <- as.POSIXct(as.numeric(p$timestamp), origin = "1970-01-01")
    p$t <- as.POSIXct(as.numeric(p$t), origin = "1970-01-01")
    c2 <- dbConnect(SQLite(), "../gtfs.db")
    stopq <- dbSendQuery(c2, "SELECT arrival_time, shape_dist_traveled FROM stop_times WHERE trip_id=? ORDER BY stop_sequence")
    dbBind(stopq, list(p$trip_id[nrow(p)]))
    stops <- dbFetch(stopq)
    dbClearResult(stopq)
    stops$arrival_time <- as.POSIXct(paste(Sys.Date(), stops$arrival_time), origin = "1970-01-01")
    tripq <- dbSendQuery(c2, "SELECT route_short_name, route_long_name FROM routes, trips WHERE routes.route_id=trips.route_id AND trip_id=?")
    dbBind(tripq, list(p$trip_id[nrow(p)]))
    tripr <- dbFetch(tripq)
    dbClearResult(tripq)
    ps <- p[p$t <= p$timestamp | p$timestamp == max(p$timestamp), ]
    xr <- range(stops$arrival_time, ps$t)
    x <- ggplot(ps) +
        geom_line(aes(t, d, group = particle_id), alpha=0.3) +
        labs(x = "Time", y = "Distance (m)") +
        xlim(xr) + ylim(c(0, max(stops$shape_dist_traveled))) + 
        geom_hline(yintercept = max(stops$shape_dist_traveled), linetype = 2, colour = "#cccccc") + 
        ggtitle(sprintf("%s: %s (vehicle %s)", tripr$route_short_name, tripr$route_long_name, vid)) +
        geom_point(aes(arrival_time, shape_dist_traveled), data = stops)
    if (!missing(obs)) {
        x <- x + geom_point(aes(obs$t, obs$d), col = "#bb0000", size = 0.5)
    }
    x + geom_vline(xintercept = Sys.time(), colour = "#aa0000", linetype = 2)
    if (wait) grid::grid.locator()
    dev.hold()
    on.exit(dev.flush())
    print(x) 
    dev.flush()
    invisible(p)
}

pdb <- csv2db("../build/particles.csv")
vids <- dbGetQuery(pdb, "SELECT DISTINCT vehicle_id FROM particles")$vehicle_id
invisible(sapply(vids, plotVehicle, db = pdb, wait = TRUE))

## p <- plotVehicle(vids[3], pdb)





#
# x + geom_point(aes(obs$t, obs$d), col = "#bb0000")
#
# p1$tadj <- numeric(nrow(p))
# n <- tapply(seq_along(p1$id), p1$id, function(i) {
#     di <- runif(1, max(0, obs$d - 50), min(max(p1$d), obs$d + 50))
#     p1$tadj[i] <<- p1$t[i] - p1$t[which.min(abs(p1$d[i] - di))] + obs$t
#     NULL
# })
#
#
# ggplot(p) +
#     geom_line(aes(t, d, group = particle_id)) +
#     labs(x = "Time (s)", y = "Distance (m)") +
#     geom_hline(yintercept = max(p$d), linetype = 2, colour = "#cccccc") +
#     geom_point(aes(obs$t, obs$d), col = "#bb0000", size = 0.5)
