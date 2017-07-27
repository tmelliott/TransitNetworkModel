library(ggplot2)

plotHist <- function(f) {
    p <- read.csv(f, colClasses = c("integer", "factor", "integer", "numeric", "factor", "integer", "numeric", "numeric"))
    p$timestamp <- as.POSIXct(p$timestamp, origin = "1970-01-01")

    x <- ggplot(p) +
        geom_line(aes(timestamp, distance, group = particle_id, color = event)) +
        geom_point(aes(timestamp, distance, color = event)) +
        scale_color_manual(values = c("#bb0000", "orange", "#00bb00", "#0000bb", "yellow", "black"))
    print(x)
    invisible(p)
}

for (f in list.files("../build/HISTORY", full.names = TRUE)) {
    plotHist(f)#"../build/HISTORY/3A9A.csv")
    locator(1)
}
