library(ggplot2)
file <- "../build/segment_data.csv"

plottimes <- function(file, which = c("segments", "combined")) {
    times <- read.csv(file, colClasses = c("factor", "factor", "integer", "integer", "numeric"))
    times$timestamp <- as.POSIXct(times$timestamp, origin = "1970-01-01")
    times$speed <- with(times, length / 1000 / travel_time * 60 * 60)
    date <- format(times$timestamp[1], "%Y-%m-%d")

    which <- match.arg(which)
    p <- ggplot(times, aes(x = timestamp, y = speed)) +
            geom_vline(xintercept = as.POSIXct(paste(date, c("07:00", "09:00", "17:00", "19:00"))),
                       color = "orangered", lty = 2) +
            geom_point() +
            geom_smooth(method = "loess")
    switch(which,
            "segments" = {
                p <- p + facet_wrap(~segment_id)
            },
            "combined" = {
                
            })
    p <- p + xlab("Time") + ylab("Speed (m/s)") + ylim(c(0, 110))
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
