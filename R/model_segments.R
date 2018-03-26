message(" * loading packages")
suppressPackageStartupMessages({
    library(tidyverse)
    library(ggmap)
    library(viridis)
    library(RSQLite)
    library(Rcpp)
    ## library(rgl)
    library(sf)
    library(mgcv)
    library(splines)
    library(R2jags)
})

source("common.R")

as.time <- function(x) {
    y <- as.POSIXct(x, origin = "1970-01-01") %>%
        format("%Y-%m-%d") %>%
        as.POSIXct %>% as.numeric
    (x - y) / 60 / 60
}

## Load all of the data for exploring
message(" * loading data")
con <- dbConnect(SQLite(), "history.db")
data <- dbGetQuery(con,
                   "SELECT * FROM vps WHERE segment_id IS NOT NULL") %>%
    as.tibble %>%
    mutate(speed = speed / 1000 * 60 * 60,
           time = as.time(timestamp),
           dow = format(as.POSIXct(timestamp, origin = "1970-01-01"), "%a"),
           weekend = dow %in% c("Sat", "Sun"))
dbDisconnect(con)

## bbox <- c(174.7, -37, 174.9, -36.8)
## aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

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

message(" * loading segments")
segments <- getsegments()
## for (t in seq(5, 21, by = 0.25)) {
##     doaplot(data, segments, t)
## }

## for (si in unique(data$segment_id)) {
##     p <- ggplot(data %>% filter(segment_id == si), aes(time, speed)) +
##         geom_point() +
##         xlab("") + ylab("Speed (km/h)") + ylim(c(0, 100)) +
##         geom_smooth() + facet_wrap(~weekend, ncol = 1)
##     print(p)
##     grid::grid.locator()
## }

## si <- "5262"
## si <- "5073"
## si <- "2951"

## dsi <- data %>% filter(segment_id == si) %>%
##     mutate(route = substr(route_id, 1, 3))
## ggplot(dsi %>% filter(!weekend), aes(time, speed)) +
##     geom_point() +
##     xlab("") + ylab("Speed (km/h)") + ylim(c(0, 100)) +
##     geom_smooth()

## ssi <- segments %>% filter(id == si)
## bbox <- with(ssi, c(min(lng) - 0.05, min(lat) - 0.05,
##                     max(lng) + 0.05, max(lat) + 0.05))
## aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

## ggmap(aklmap) +
##     geom_path(aes(lng, lat), data = ssi, lwd = 2, col = "red")


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


## dsi$dist <- distIntoShape(dsi, ssi)

## ggplot(dsi %>% filter(!weekend), aes(dist, speed, color = time)) +
##     geom_point() +
##     xlab("Distance (m)") + ylab("Speed (km/h)") + ylim(c(0, 100)) +
##     geom_smooth() +
##     scale_colour_viridis(option = "D")

## fit <- gam(speed ~ s(dist) + s(time, k=10), data = dsi)
## summary(fit)
## xdist <- seq(min(dsi$dist), max(dsi$dist), length.out = 101)
## xtime <- seq(min(dsi$time), max(dsi$time), length.out = 101)
## pr <- expand.grid(xdist, xtime)
## names(pr) <- c("dist", "time")
## yspeed <- predict(fit, pr)

## pred <- outer(xdist, xtime, function(d, t)
##     predict(fit, data.frame(dist = d, time = t)))

## with(dsi %>% filter(!weekend),
##      plot3d(dist, time, speed, aspect = c(3, 5, 1)))
## surface3d(xdist, xtime, pred, grid=FALSE, color = "red")


## ## A 'dummy' variable for "peak"
## peak <- c(9, 17)
## peak.sd <- c(0.5, 1)

## dsi <- dsi %>%
##     mutate(peak1 = dnorm(time, peak[1], peak.sd[1]),
##            peak2 = dnorm(time, peak[2], peak.sd[2]))

## p1 <- ggplot(dsi %>% filter(!weekend), aes(time, speed)) +
##     geom_point(aes(alpha = pmax(peak1 + peak2))) +
##     xlab("") + ylab("Speed (km/h)") + ylim(c(0, 100)) +
##     geom_smooth() +
##     geom_vline(xintercept = peak) +
##     theme(legend.position = "none")
## p2 <- ggplot() +
##     geom_path(aes(x = x, y = y),
##               data = data.frame(
##                   x = (xx <- seq(min(dsi$time), max(dsi$time), by = 0.01)),
##                   y = pmax(dnorm(xx, peak[1], peak.sd[1]),
##                            dnorm(xx, peak[2], peak.sd[2]))))
## gridExtra::grid.arrange(p1, p2, ncol = 1, heights = c(3, 1))


## fit <- gam(speed ~ s(dist) + peak1 + peak2, data = dsi,
##            sp = c(-1, 1, 1))
## summary(fit)
## xdist <- seq(min(dsi$dist), max(dsi$dist), length.out = 101)
## xtime <- seq(min(dsi$time), max(dsi$time), length.out = 101)

## pred <- outer(xdist, xtime, function(d, t)
##     predict(fit, data.frame(dist = d,
##                             peak1 = dnorm(t, peak[1], peak.sd[1]),
##                             peak2 = dnorm(t, peak[2], peak.sd[2]))))

## with(dsi %>% filter(!weekend),
##      plot3d(dist, time, speed, aspect = c(3, 5, 1)))
## surface3d(xdist, xtime, pred, grid=FALSE, color = "red")



## gj <- jagam(speed ~ s(dist) + peak1 + peak2, data = dsi,
##             file = "model.jags")

## jags.data <- list(
##     B = (B <- bs(dsi$dist, df = 10)),
##     t = dsi$time,
##     y = dsi$speed,
##     N = nrow(dsi),
##     K = ncol(B)
## )
## jags.pars <- c("intercept", "beta", "tau", "omega", "pi", "alpha", "sig2", "r")
## ##jags.pars <- c("intercept", "beta", "sig2")

## jfit <- jags(jags.data, NULL, jags.pars, model.file = "model.jags",
##              n.chains = 2, n.iter = 2000)

## ##jfit.mcmc <- as.mcmc.list(jfit$BUGSoutput)

## rdf <- jfit$BUGSoutput$sims.matrix %>% as.tibble %>%
##     rename_if(grepl("[", names(.), fixed=T),
##               function(x) gsub("`|\\[|\\]", "", x))

## hist(rdf$sig2, 50)
## hist(rdf$pi2, 50)
## hist(rdf$tau2, 50)
## hist(rdf$omega2, 50)
## hist(rdf$pi1, 50)
## hist(rdf$tau1, 50)
## hist(rdf$omega1, 50)

## pairs(rdf %>% select_if(!grepl("beta|^r.", names(.))))

## sims <- jfit$BUGSoutput$median
## Bx <- bs(xdist, knots = attr(B, "knots"))
## nr <- nrow(sims[[1]])
## pred <- outer(1:length(xdist), xtime, function(j, t) {
##     p <- sapply(t, function(ti)
##         1 - sum(sims$r * sims$alpha * exp(-(ti - sims$tau)^2 / (2 * sims$omega^2))))
##     p * (sims$intercept[1] + Bx[j, ] %*% cbind(sims$beta))
## })
## with(dsi %>% filter(!weekend),
##      plot3d(dist, time, speed, aspect = c(3, 5, 1)))
## surface3d(xdist, xtime, pred, grid=FALSE, color = "#99000020")


## for (i in sample(nrow(rdf), 50)) {
##     sims <- lapply(jfit$BUGSoutput$sims.list, function(x) x[i, ])
##     pred <- outer(1:length(xdist), xtime, function(j, t) {
##         p <- sapply(t, function(ti)
##             1 - sum(sims$r * sims$alpha * exp(-(ti - sims$tau)^2 / (2 * sims$omega^2))))
##         p * (sims$intercept[1] + Bx[j, ] %*% cbind(sims$beta))
##     })
##     with(dsi %>% filter(!weekend),
##          plot3d(dist, time, speed, aspect = c(3, 5, 1)))
##     surface3d(xdist, xtime, pred, grid=FALSE, color = "#990000")
##     Sys.sleep(0.1)
## }


message(" * loading segments and computing distance")
segs <- c("5262", "5073", "2951")
st <- sort(table(data$segment_id), TRUE)
segs <- names(st)[1:5]
sg <- segments %>% filter(id %in% segs)
ds <- data %>%
    filter(segment_id %in% segs) %>%
    filter(!is.na(speed)) %>%
    mutate(route = substr(route_id, 1, 3)) %>%
    group_by(segment_id) %>%
    do((.) %>%
       mutate(dist = distIntoShape(., segments %>%
                                      filter(id == segment_id[1])))) %>%
    ungroup()

## bbox <- with(sg, c(min(lng) - 0.05, min(lat) - 0.05,
##                    max(lng) + 0.05, max(lat) + 0.05))
## aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

## ggmap(aklmap) +
##     geom_path(aes(lng, lat, colour = id), data = sg, lwd = 2)


## ggplot(ds, aes(dist, speed, colour = time)) +
##     geom_point() +
##     facet_wrap(~segment_id, scales = "free", ncol = 1)

## floor(tapply(ds$dist, ds$segment_id, length) / 100)
bsX <- function(x) {
    b <- splines::bs(x, df = floor(length(x) / 100), intercept = TRUE)
    B <- b
    attributes(B) <- NULL
    dim(B) <- dim(b)
    B <- B %>% unclass %>% as.tibble %>%
        rename_all(function(i) gsub("V", "", i)) %>%
        rowid_to_column %>%
        gather(key = "xi", value = "betaj", -rowid) %>%
        filter(betaj > 0) %>%
        mutate(xi = as.integer(xi))
    attr(B, "knots") <- attr(b, "knots")
    B
}



## Now convert Bs into a single sparse matrix ...
Bs <- tapply(1:nrow(ds), ds$segment_id, function(i) {
    x <- ds$dist[i]
    b <- splines::bs(x, df = floor(length(x) / 100), intercept = TRUE)
    B <- b
    attributes(B) <- NULL
    dim(B) <- dim(b)
    B <- B %>% unclass %>% as.tibble %>%
        rename_all(function(i) gsub("V", "", i)) %>%
        mutate(i = i) %>%
        gather(key = "j", value = "beta", -i) %>%
        filter(beta > 0) %>%
        mutate(i = as.integer(i),
               j = as.integer(j))
    attr(B, "knots") <- attr(b, "knots")
    B
})
Bj <- lapply(Bs, function(x) max(x$j)) %>% as.integer
Sk <- rep(names(Bs), Bj)
Bj <- c(0L, cumsum(Bj)[-length(Bj)])
names(Bj) <- names(Bs)
Bs <- lapply(names(Bs), function(sid) {
    Bs[[sid]] %>% mutate(j = j + Bj[sid])
})
B <- do.call(rbind, Bs)



library(rstan)

stan.fit <- stan("stan/hier_seg_model.stan",
                 data = list(Bij = B[,1:2] %>% as.matrix,
                             Bval = B$beta,
                             M = nrow(B),
                             t = ds$time,
                             y = ds$speed,
                             N = nrow(ds),
                             s = ds$segment_id %>% as.factor %>% as.numeric,
                             L = length(unique(ds$segment_id)),
                             K = max(B$j),
                             sk = as.integer(Sk))
)


## Now model is fitted, need to transform B into B matrices for each segment






####### BEGIN JAGS
jags.data <- list(
    
jags.pars <- c("beta", "tau", "omega", "pi", "alpha", "sig2", "r")

message(" * fitting JAGS model")
jfit <- suppressMessages({
    ## jags.parallel(
    jags(
        jags.data, NULL, jags.pars,
        model.file = "model2.jags",
        ## n.chains = 4, n.iter = 5000)
        n.iter = 2000)
})

message(" * writing results")
save(ds, jfit, Bs, file = "model_results.rda")


## rdf <- jfit$BUGSoutput$sims.matrix %>% as.tibble %>%
##     rename_if(grepl("[", names(.), fixed=T),
##               function(x)
##                   gsub(",", "_", gsub("`|\\[|\\]", "", x)))

## pairs(rdfx <- rdf %>% select_if(!grepl("beta|^r.", names(.))))

## op <- par(mfrow = c(5, 4))
## for (i in colnames(rdfx))
##     hist(rdf[[i]], 50, xlab=i,main=i)
## par(op)



## plotPlane(ds, jfit, 2, Bs, which = 'median')


message(" * done")
