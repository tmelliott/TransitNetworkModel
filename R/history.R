plotHist <- function(f) {
    require(ggplot2)
    p <- read.csv(f, colClasses = c("integer", "factor", "integer", "numeric", "factor", "integer", "numeric", "numeric"))
    p$timestamp <- as.POSIXct(p$timestamp, origin = "1970-01-01")
    p$survive <- factor(p$particle_id %in% unique(p$parent),
                        levels = c(FALSE, TRUE), labels = c("no", "yes"))

    x <- ggplot(p) +
        geom_line(aes(timestamp, distance, group = particle_id, color = event)) +
        geom_point(aes(timestamp, distance, color = event, shape = survive)) +
        scale_color_manual(values = c("#bb0000", "orange", "#00bb00", "#0000bb", "yellow", "black")) +
        scale_shape_manual(values = c(4, 1))
    print(x)
    invisible(p)
}

fs <- list.files("../build/HISTORY", full.names = TRUE)
for (f in list.files("../build/HISTORY", full.names = TRUE)) {
    plotHist(f)#"../build/HISTORY/3A9A.csv")
    grid::grid.locator()
}
