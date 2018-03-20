suppressPackageStartupMessages({
    library(tidyverse)
    library(ggmap)
    library(viridis)
    library(RSQLite)
    library(Rcpp)
    library(rgl)
    library(sf)
})

source("common.R")

as.time <- function(x) {
    y <- as.POSIXct(x, origin = "1970-01-01") %>%
        format("%Y-%m-%d") %>%
        as.POSIXct %>% as.numeric
    (x - y) / 60 / 60
}

## Load all of the data for exploring
data <- dbGetQuery(dbConnect(SQLite(), "history.db"),
                   "SELECT * FROM vps WHERE segment_id IS NOT NULL") %>%
    as.tibble %>%
    mutate(speed = speed / 1000 * 60 * 60,
           time = as.time(timestamp),
           dow = format(as.POSIXct(timestamp, origin = "1970-01-01"), "%a"),
           weekend = dow %in% c("Sat", "Sun"))


bbox <- c(174.7, -37, 174.9, -36.8)
aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

##' @param data the data to plot
##' @param t time (in hours) to plot
##' @param span range of averaging (in hours)
doaplot <- function(data, segments, t, span = 0.25) {
    segsmry <- data %>%
        filter(!is.na(speed)) %>%
        filter(time >= t - span & time <= t + span) %>%
        group_by(segment_id) %>%
        summarize(speed = mean(speed, na.rm = TRUE),
                  speed.sd = sd(speed, na.rm = TRUE),
                  n = n()) %>%
        arrange(desc(n))
    
    segments <- segments %>%
        rename(segment_id = "id") %>%
        inner_join(segsmry, by = "segment_id")

    p <- ggmap(aklmap) +
        geom_path(aes(x = lng, y = lat, group = segment_id, colour = speed),
                  data = segments,
                  lwd = 2) +
        scale_colour_viridis(limits = c(0, 50)) +
        xlab("") + ylab("") +
        ggtitle(sprintf("Network State at %d:%02d",
                        floor(t), round((t %% 1) * 60)))
    dev.hold()
    print(p)
    dev.flush()
    
    invisible(p)
}

segments <- getsegments()
## for (t in seq(5, 21, by = 0.25)) {
##     doaplot(data, segments, t)
## }

for (si in unique(data$segment_id)) {
    p <- ggplot(data %>% filter(segment_id == si), aes(time, speed)) +
        geom_point() +
        xlab("") + ylab("Speed (km/h)") + ylim(c(0, 100)) +
        geom_smooth() + facet_wrap(~weekend, ncol = 1)
    print(p)
    grid::grid.locator()
}

si <- "5262"
si <- "5073"
dsi <- data %>% filter(segment_id == si) %>%
    mutate(route = substr(route_id, 1, 3))
ggplot(dsi %>% filter(!weekend), aes(time, speed)) +
    geom_point() +
    xlab("") + ylab("Speed (km/h)") + ylim(c(0, 100)) +
    geom_smooth() #+ facet_wrap(~route)

ssi <- segments %>% filter(id == si)
bbox <- with(ssi, c(min(lng) - 0.05, min(lat) - 0.05,
                    max(lng) + 0.05, max(lat) + 0.05))
aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

ggmap(aklmap) +
    geom_path(aes(lng, lat), data = ssi, lwd = 2, col = "red")


#col <- viridis(101)[round(dsi$speed)]
#with(dsi,
#     plot3d(lng, lat, speed))


distIntoShape <- function(p, shape) {
    P <- p %>% select(lng, lat) %>% as.matrix %>%
        st_multipoint %>% st_sfc %>%
        st_cast("POINT")
    sh <- shape %>% select(lng, lat) %>% as.matrix %>%
        st_linestring %>% st_line_sample(nrow(shape) * 10) %>%
        st_cast("POINT")
    co <- sh %>% st_coordinates
    cd <- c(0, cumsum(geosphere::distHaversine(co[-nrow(co),], co[-1, ])))
    pbapply::pbsapply(P, function(p) cd[which.min(st_distance(sh, p))] )
}


dsi$dist <- distIntoShape(dsi, ssi)

ggplot(dsi %>% filter(!weekend), aes(dist, speed, color = time)) +
    geom_point() +
    xlab("Distance (m)") + ylab("Speed (km/h)") + ylim(c(0, 100)) +
    geom_smooth() +
    scale_colour_viridis(option = "D")

fit <- gam(speed ~ s(dist) + s(time), data = dsi,
           sp = c(-1, 0.001))
summary(fit)
xdist <- seq(min(dsi$dist), max(dsi$dist), length.out = 101)
xtime <- seq(min(dsi$time), max(dsi$time), length.out = 101)
pr <- expand.grid(xdist, xtime)
names(pr) <- c("dist", "time")
yspeed <- predict(fit, pr)

pred <- outer(xdist, xtime, function(d, t)
    predict(fit, data.frame(dist = d, time = t)))

with(dsi %>% filter(!weekend),
     plot3d(dist, time, speed, aspect = c(3, 5, 1)))
surface3d(xdist, xtime, pred, grid=FALSE, color = "red")


