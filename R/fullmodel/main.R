library(RProtoBuf)
library(RSQLite)
library(ggmap)
library(tidyverse)
library(viridis)
library(truncnorm)

source("fns.R")

## 0. settings etc.
DATE <- "2018-01-08"
HOME <- path.expand("~/Documents/uni/TransitNetworkModel")
PI.IP <- "tom@130.216.51.230"
PI.DIR <- "/mnt/storage/history"


## 1. read protofiles from a day into a database; return connection
readProtoFiles(dir = file.path(HOME, "proto"))

#makeData(DATE, PI.IP, PI.DIR)


## 2. connect to database and, for a given route, fetch GTFS information

dd <- getRouteData("274", "busdata.db", file.path(HOME, "gtfs.db"))
vstart <- tapply(1:nrow(dd), dd$vehicle_id, function(x) x[1])

vi <- vstart[2]
p <- plot(getShape(dd, id = dd$shape_id[vi]), zoom = 14)
p <- p +
    geom_point(aes(lng, lat), data = getStops(dd, id = dd$trip_id[vi]),
               size = 2, colour = "orangered", pch = 21, fill = "black",
               stroke = 1.5)
p

R <- 6371e3
dd1 <- dd %>% filter(vehicle_id == vehicle_id[vi], trip_id == trip_id[vi]) %>%
    arrange(timestamp) %>%
    ## compute times ...
    dplyr::mutate(x = (lng * pi / 180 - mean(lng * pi / 180)) * cos(lat * pi / 180) * R,
                  y = (lat - mean(lat)) * pi / 180 * R) %>%
    dplyr::mutate(dist = c(sqrt(diff(x)^2 + diff(y)^2), 0),
                  tdiff = c(diff(timestamp), 0)) %>%
    dplyr::mutate(speed = dist / tdiff)
class(dd1) <- c("gtfs.data", class(dd1))

p + geom_path(aes(lng, lat, colour = speed), data = dd1, lwd = 2) +
    scale_colour_viridis()



## 3a. implement particle filter EXACTLY as it is in C++ ... (some how)

Sd <- getStops(dd, id = dd$trip_id[vi])$shape_dist_traveled
Rd <- getSegments(dd, id = dd$shape_id[vi])$shape_dist_traveled
p1 <- particle(Sd = Sd, Rd = Rd)

source("fns.R")
rho = 0.4; Pi = 0.5; theta = 20; tau = 6; gamma = 3; sigv = 5;
ps <- fleet(500, Sd = Sd, Rd = Rd,
            rho = rho, pi = Pi, theta = theta, tau = tau, gamma = gamma,
            sigv = sigv)
dhat <- rep(0, nrow(dd1))
#plot(ps, col = "#33333330", xlim = c(0, 40 * 60))
#points(dd1$timestamp - min(dd1$timestamp), dhat,
#       col = "magenta", pch = 19, cex = 0.5)

ts <- dd1$timestamp - min(dd1$timestamp)
Y <- dd1[, c("lat", "lng")]
shp <- getShape(dd, dd$shape_id[vi])[, 3:5]
#jpeg("journeyRT%03d.jpeg", width = 1600, height = 900)
for (i in seq_along(ts)) {
    lh <- 0
    li <- 1
    while (sum(lh) == 0) {
        llh <- sapply(ps, loglh, t = ts[i], pos = Y[i, ], shape = shp, sigma = li * 5)
        lh <- exp(llh)
        wt <- lh / sum(lh)
        li <- li + 1
    }
    if (li > 2) print(paste("li =", li-1))
    ii <- sample(length(wt), replace = TRUE, prob = wt)
    dev.hold()
    layout(rbind(c(1, 1, 1, 2)))
    plot(ps, col = "#cccccc30", xlim = c(0, 60*40))
    pold <- ps
    ps <- do.call(collect, lapply(ii, function(j) {
        particle(ps[[j]]$distance[1:(min(ts[i]+1, length(ps[[j]]$distance)))],
                 Sd = Sd, Rd = Rd,
                 rho = rho, pi = Pi, theta = theta, tau = tau, gamma = gamma,
                 sigv = sigv)
    }))
    attr(ps, "Rd") <- Rd
    attr(ps, "Sd") <- Sd
    lines(ps, col = "#99000030")
    dhat[i] <- mean(sapply(ps, function(p) p$distance[ts[i]+1]))
    points(dd1$timestamp - min(dd1$timestamp), dhat,
           col = "magenta", pch = 19, cex = 0.5)
    drx <- diff(range(shp$lng))
    dry <- diff(range(shp$lat))
    with(shp, plot(lng, lat, asp = 1.6, type = "l", lwd = 2,
                   xlim = dd1[i, "lng"] + c(-0.1, 0.1) * drx,
                   ylim = dd1[i, "lat"] + c(-0.1, 0.1) * dry))
    with(dd1[i, ], points(lng, lat, pch = 19, cex = 4, col = "#ef560040"))
    points(t(sapply(pold, function(p) {
        if (size(p) > ts[i]) h(p$distance[ts[i]+1], shp = shp) else shp[nrow(shp), 1:2]
    }))[, 2:1], cex = 2, col = "#cccccc30", pch = 19)
    points(t(sapply(ps, function(p) {
        if (size(p) > ts[i]) h(p$distance[ts[i]+1], shp = shp) else shp[nrow(shp), 1:2]
    }))[, 2:1], cex = 1.5, col = "#990000", pch = 19)
    dev.flush()
    #locator(1)
}

dev.off()
## make movie
system("rm journey274.gif && convert -delay 50 -loop 0 journey*.jpeg ~/Dropbox/journey274.gif && rm journey*jpeg")






## 3b. implement a STAN model for n = 0, ..., N observations (estimating segment travel times!)

flatX <- function(x, z) (x - z) * pi / 180 * sin(z * pi / 180 * R)
flatY <- function(y, z) (y - z) * pi / 180 * R

sh <- getShape(dd, id = dd$shape_id[vi])
z <- c(mean(sh$lng), mean(sh$lat))
dat <- list(N = nrow(dd1),
            lng = flatX(dd1$lng, z[1]),
            lat = flatY(dd1$lat, z[2]),
            t = dd1$timestamp - min(dd1$timestamp) + 1,
            Q = nrow(sh),
            sx = flatX(sh$lng, z[1]),
            sy = flatY(sh$lat, z[2]),
            sdist = round(sh$dist_traveled),
            pi = pi)
dat$Nt = max(dat$t)

fit <- stan(file = "model.stan", data = dat,
            iter = 1000, chains = 4)

print(fit)

plot(fit, pars = "d")
