library(ggplot2)
library(tidyverse)
library(splines)
library(RSQLite)
library(rstan)
library(forcats)
file <- "../build/segment_data.csv"

gettimes <- function(x, n.min = 1) {
    times <- read.csv(
        file,
        col.names = c("segment_id", "vehicle_id", "timestamp", "travel_time", "length"),
        colClasses = c("factor", "factor", "integer",
                       "integer", "numeric")) %>%
        mutate(timestamp = as.POSIXct(timestamp, origin = "1970-01-01"),
               speed = round(length / 1000 / travel_time * 60 * 60),
               length = round(length)) %>%
        group_by(segment_id) %>% filter(n() > n.min)
    attr(times, "n.min") <- n.min
    times
}
plottimes <- function (x, which = c("segments", "combined"),
                       span = 1, show.peak = TRUE) {
    times <- x
    date <- format(times$timestamp[1], "%Y-%m-%d")

    which <- match.arg(which)
    p <- ggplot(times, aes(x = timestamp, y = speed))
    if (show.peak)
        p <- p +
        geom_vline(xintercept = as.POSIXct(
                       paste(date, c("07:00", "09:00", "17:00", "19:00"))),
                            color = "orangered", lty = 2)
    p <- p + geom_point()
    if (!is.null(span))
        p <- p + geom_smooth(method = "loess", span = span)
    switch(which,
            "segments" = {
                p <- p + facet_wrap(~segment_id)
            },
            "combined" = {
                
            })
    
    p <- p + xlab("Time") + ylab("Speed (m/s)") + ylim(c(0, 100))
    if (attr(x, "n.min") > 1)
        p <- p + ggtitle(sprintf("Segments with %s+ observations",
                                 attr(x, "n.min")))

    dev.hold()
    suppressWarnings(print(p))
    dev.flush()

    invisible(p)
}

## while(TRUE) {
##     pdf("~/Dropbox/gtfs/segment_speeds.pdf", width = 18, height = 15)
##     try(plottimes(file), TRUE)
##     dev.off()
##     Sys.sleep(60)
## }

plottimes(gettimes(file, n.min = 50), span = 0.3) +
    geom_hline(yintercept = 50, colour = "magenta", lty = 3)

times <- gettimes(file, n.min = 50)
SEGS <- levels(times$segment_id)[table(times$segment_id) > 0]

BIGYHAT <- vector("list", length(SEGS))
names(BIGYHAT) <- SEGS

for (SEG in SEGS) {
    con <- dbConnect(SQLite(), "../gtfs.db")
    seg <- dbGetQuery(
        con, sprintf("SELECT * FROM segments WHERE segment_id=%s",
                     SEG))
    Ints <- dbGetQuery(
        con, sprintf("SELECT * FROM intersections WHERE intersection_id IN (%s)",
                     paste(seg$from_id, seg$to_id, sep = ",")))
    POS <- paste(Ints$lat, Ints$lng, sep = ",")
    dbDisconnect(con)
    ## browseURL(sprintf("https://www.google.co.nz/maps/dir/%s/%s", POS[1], POS[2]))
    
    seg3746 <- times %>% filter(segment_id == SEG) %>% filter(speed < 60)
    
    if (nrow(seg3746) == 0) next()
    ## plottimes(seg3746)
    
    ## DEG <- 3
    ## KNOTS <- as.POSIXct(paste(date, paste0(6:23, ":00")))
    ## spl <- bs(times$timestamp, knots = KNOTS, degree = DEG)
    ## knts <- c(attr(spl, "knots"))
    ## ft <- lm(speed ~ bs(timestamp, knots = KNOTS, degree = DEG),
    ##          data = seg3746)
    ## tx <- with(seg3746, seq(min(timestamp), max(timestamp), length = 1001))
    ## pdat <- data.frame(x = tx, y = predict(ft, data.frame(timestamp = tx)))
    ## kdat <- data.frame(x = as.POSIXct(knts, origin = "1970-01-01"),
    ##                    y = predict(ft, data.frame(timestamp = knts)))
    
    ## plottimes(seg3746, span = NULL, show.peak = FALSE) +
    ##    geom_line(aes(x, y), data = pdat, colour = "orangered", lwd = 1) +
    ##    geom_point(aes(x, y), data = kdat, colour = "orangered",
    ##               shape = 21, fill = "white", size = 2, stroke = 1.5)


#######################################################################
#######################################################################
### Fit a model

    options(mc.cores = 2)
    
    date <- as.POSIXct(format(seg3746$timestamp[1], "%Y-%m-%d"))
    dat <- list(x = as.numeric(seg3746$timestamp) - as.numeric(date),
                y = seg3746$speed,
                N = nrow(seg3746))
    dat$h <- floor(dat$x / 60 / 60)
    dat$H <- sort(unique(dat$h))
    dat$M <- length(dat$H)
    dat$h <- dat$h - min(dat$h) + 1
    dat$speed <- 10 * 4:10
    dat$p.speed <- c(1, 20, 3, 2, 1, 1, 4)
    dat$p.speed <- dat$p.speed / sum(dat$p.speed)
    dat$S <- length(dat$speed)
    
    fit1 <- stan(file = "segment_model.stan", data = dat,
                 control = list(adapt_delta = 0.99))

    plot(extract(fit1, pars = "Vmax")[[1]], type = "l")
    hist(extract(fit1, pars = c("Vmax"))[[1]], 100, freq=F)
    curve(dtruncnorm(x, 5, 11, 5, 1), 5, 11, 1001, add = TRUE)
    
    ## lapply(do.call(data.frame, extract(fit1)), function(x) {
    ##     plot(x, type = "l")
    ##     locator(1)
    ## })
    
    BIGYHAT[[SEG]] <-
        parmat <- do.call(cbind, extract(fit1, pars = c("yhat")))
    ## colnames(parmat) <- paste0(dat$H, ":00")
    ##     #rownames(summary(fit1, pars = c("yhat"))$summary)
    ## parmat <- parmat[, apply(parmat, 2, function(y) any(y != 0))]
    
    ## corr <- cor(parmat)
    ## diag(corr) <- NA
    ## corr <- corr %>% reshape2::melt()
    
    ## ggplot(data = corr, aes(x = Var1, y = Var2, fill = value)) +
    ##     geom_tile() +
    ##     scale_fill_distiller(palette = "Spectral",
    ##                          na.value = "#cccccc")

    ## plot(fit1, pars = "beta")
    ## hist(extract(fit1, pars = "tau")$tau, freq = FALSE, xlim = c(0, 12))
    ## curve(dexp(x, 1), 0, 12, 1001, add = TRUE)

    ## plot(extract(fit1, pars = "mu")$mu, type = "l")
    
    vals <- do.call(data.frame, extract(fit1, pars = "yhat")) %>%
        mutate(sample = 1:n()) %>%
        ##    filter(sample %in% sample(n(), 100)) %>%
        gather(time, speed, -sample, factor_key = TRUE)
    levels(vals$time) <- dat$H
    vals$t <- as.POSIXct(paste0(date, " ", vals$time, ":00"))
    
    jpeg(sprintf("fig/segment_%s.jpg", SEG), width = 900, height = 500)
    print(
        plottimes(seg3746, span = NULL, show.peak = FALSE) +
        geom_hline(yintercept = extract(fit1, pars = "mu")$mu,
                   colour = "blue", alpha = 0.01) +
        geom_step(aes(x = t, y = speed, group = sample), data = vals,
                  colour = "orangered", alpha = 0.01, linetype = 1)
    )
    dev.off()
    
}


arr <- array(NA, c(4000, 18, 4))
for (i in 1:4) arr[,,i] <- BIGYHAT[[i]]


for (i in 1:18) {
    h1 <- arr[,i,]
    colnames(h1) <- SEGS[1:4]
    corr <- cor(h1)
    diag(corr) <- NA
    corr <- corr %>% reshape2::melt()
    p <- ggplot(data = corr, aes(x = Var1, y = Var2, fill = value)) +
        geom_tile() +
        scale_fill_distiller(palette = "Spectral", #limits = c(-1, 1),
                             na.value = "#cccccc") +
        ggtitle(sprintf("At %s:00", i + 4))
    print(p)
    grid::grid.locator()
}













####################################################
####################################################
##
##
##

f <- function(x, phi = 50, lambda = 0.001, delta = 30) {
    x + lambda * (phi - x)
}
F <- function(lambda = 0.001) 1 - lambda

v <- 1:6 * 10
V <- matrix(NA, ncol = length(v), nrow = 6000)
V[1, ] <- v
for (i in 2:nrow(V)) 
    V[i, ] <- v <- f(v)

plot((1:nrow(V)) / 60, V[, 1], type = "l", ylim = range(V),
     xlab = 'Time (min)', ylab = 'Segment Travel Time (s)')
for (i in 2:ncol(V)) lines((1:nrow(V)) / 60, V[, i])

## test it ...
v <- 80 # seconds
s <- 2  # variance of travel times
Q <- 1#5 / 60 # noise added to travel time per minute

V <- numeric(6000) ## run for 100 minutes
S <- V
V[1] <- v
S[1] <- s
for (i in 2:length(V)) {
    V[i] <- f(V[i-1])#, phi = ifelse(i > 60*60, 90, 50))
    S[i] <- F() * S[i-1] * F() + Q
}

t <- seq_along(V) / 60
plot(t, V, type = "l", lwd = 2, ylim = range(0, qnorm(0.975, V, sqrt(S))))
abline(h = 0, lty = 3)
lines(t, qnorm(0.25, V, sqrt(S)))
lines(t, qnorm(0.75, V, sqrt(S)))
lines(t, qnorm(0.025, V, sqrt(S)), lty = 2)
lines(t, qnorm(0.975, V, sqrt(S)), lty = 2)
