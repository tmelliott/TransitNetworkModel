colf <- function(x)
    viridis::viridis(length(levels(x)))[as.numeric(x)]


while (TRUE) {
particles <- read.csv("../build/PARTICLES.csv",
                      colClasses = c("factor", "factor", "integer", "factor",
                                     "numeric", "numeric", "integer", "numeric", "numeric", "numeric"))
particles$timestamp <- as.POSIXct(particles$timestamp, origin = "1970-01-01")
#etas <- read.csv("../build/ETAS.csv",
#                 colClasses = c("factor", "factor", "integer", "integer"))
#etas$eta <- as.POSIXct(etas$eta, origin = "1970-01-01")
with(particles[particles$timestamp > 0, ], {
    plot(timestamp, distance, col = colf(vehicle_id), xaxt = "n")
    axis(1, pretty(timestamp), format(pretty(timestamp), "%T"))
})
locator(1)
}
