colf <- function(x)
    viridis::viridis(length(levels(x)))[as.numeric(x)]

while (TRUE) {
    dev.hold()
    try({
    particles <- read.csv("../build/PARTICLES.csv",
                          colClasses = c("factor", "factor", "integer", "factor", "factor",
                                         "numeric", "numeric", "integer", "numeric", "numeric", "numeric", "numeric", "integer"))
    particles$timestamp <- as.POSIXct(particles$timestamp, origin = "1970-01-01")
    ## etas <- read.csv("../build/ETAS.csv",
    ##                  colClasses = c("factor", "factor", "integer", "integer"))
    ## etas$eta <- as.POSIXct(etas$eta, origin = "1970-01-01")
    #par(mfrow = c(length(unique(particles$route_id)), 1), mar = c(5.1, 2.1, 2.1, 2.1))
    #for (routeid in unique(particles$route_id)) {
        with(particles[particles$timestamp > 0, ], {## && particles$route_id == routeid, ], {
            plot(timestamp, distance, col = colf(vehicle_id), xaxt = "n",
                 pch = ifelse(parent_id > 0 & init == 1, 1, 4), cex = wt * 3)
            axis(1, pretty(timestamp), format(pretty(timestamp), "%T"))
            tapply(seq_along(vehicle_id), as.factor(paste(vehicle_id, trip_id)),
                   function(i) {
                       lines(unique(timestamp[i]),
                             tapply(distance[i], as.factor(timestamp[i]), mean),
                             col = colf(vehicle_id[i[1]]))
                   })
        })
    #}
    },TRUE)
    dev.flush()
    Sys.sleep(1)
}
