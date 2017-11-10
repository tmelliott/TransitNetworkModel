library(ggplot2)
file <- "../build/segment_data.csv"

plottimes <- function(file) {
    times <- read.csv(file, colClasses = c("factor", "factor", "integer", "integer", "numeric"))
    times$timestamp <- as.POSIXct(times$timestamp, origin = "1970-01-01")
    times$speed <- with(times, length / 1000 / travel_time * 60 * 60)

    p <- ggplot(times, aes(x = timestamp, y = speed)) +
        geom_point() +
        geom_smooth(method = "loess") +
        facet_wrap(~segment_id) +
        xlab("Time") + ylab("Speed (m/s)") + ylim(c(0, 110))
    dev.hold()
    print(p)
    dev.flush()
}

while(TRUE) {
    pdf("~/Dropbox/gtfs/segment_speeds.pdf", width = 18, height = 15)
    try(plottimes(file), TRUE)
    dev.off()
    Sys.sleep(60)
}
