library(ggplot2)

p <- read.csv("../build/particles.csv")
p$id <- as.factor(paste(p$vehicle_id, p$particle_id, sep = ":"))

vs = levels(p$vehicle_id)

p1 = p[p$vehicle_id == vs[1], ]

x <- ggplot(p1) +
    geom_line(aes(t, d, group = id)) +
    labs(x = "Time (s)", y = "Distance (m)") +
    geom_hline(yintercept = max(p1$d), linetype = 2, colour = "#cccccc")
x

obs = list(t = 120, d = 1000)
x + geom_point(aes(obs$t, obs$d), col = "#bb0000")

p1$tadj <- numeric(nrow(p1))
n <- tapply(seq_along(p1$id), p1$id, function(i) {
    di <- runif(1, max(0, obs$d - 50), min(max(p1$d), obs$d + 50))
    p1$tadj[i] <<- p1$t[i] - p1$t[which.min(abs(p1$d[i] - di))] + obs$t
    NULL
})


ggplot(p1) +
    geom_line(aes(tadj, d, group = id)) +
    labs(x = "Time (s)", y = "Distance (m)") +
    geom_hline(yintercept = max(p1$d), linetype = 2, colour = "#cccccc") +
    geom_point(aes(obs$t, obs$d), col = "#bb0000", size = 0.5)
