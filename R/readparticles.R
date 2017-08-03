library(ggplot2)

p <- read.csv("../build/particles.csv")
p$id <- as.factor(paste(p$vehicle_id, p$particle_id, sep = ":"))

vs = levels(p$vehicle_id)

ggplot(p[p$vehicle_id == vs[1], ]) +
    geom_line(aes(t/60, d, group = id)) +
    labs(x = "Time (minutes)", y = "Distance (m)") +
    geom_hline(yintercept = max(p[p$vehicle_id == vs[1], "d"]), linetype = 2, colour = "#cccccc")
