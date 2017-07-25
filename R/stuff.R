colf <- function(x)
    viridis::viridis(length(levels(x)))[as.numeric(x)]

while (TRUE) {
    try({
    particles <- read.csv("../build/PARTICLES.csv",
                          colClasses = c("factor", "factor", "integer", "factor",
                                         "numeric", "numeric", "integer", "numeric", "numeric", "numeric", "integer"))
    particles$timestamp <- as.POSIXct(particles$timestamp, origin = "1970-01-01")
    ## etas <- read.csv("../build/ETAS.csv",
    ##                  colClasses = c("factor", "factor", "integer", "integer"))
    ## etas$eta <- as.POSIXct(etas$eta, origin = "1970-01-01")
    dev.hold()
    with(particles[particles$timestamp > 0, ], {
        plot(timestamp, distance, col = colf(vehicle_id), xaxt = "n",
             pch = ifelse(parent_id > 0 & init == 1, 1, 4))
        ##abline(h = segs, lty = 2, col = "gray50")
        axis(1, pretty(timestamp), format(pretty(timestamp), "%T"))
        tapply(seq_along(vehicle_id), as.factor(paste(vehicle_id, trip_id)),
               function(i) lines(unique(timestamp[i]),
                                 tapply(distance[i], as.factor(timestamp[i]), mean),
                                 col = colf(vehicle_id[i[1]])))
    })
    },TRUE)
    dev.flush()
    Sys.sleep(1)
}
