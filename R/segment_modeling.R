library(ggplot2)
library(tidyverse)
library(splines)
library(RSQLite)
library(rstan)
library(magrittr)
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
                       span = 1, show.peak = TRUE, speed = TRUE,
                       trim = !speed,
                       estimates) {
    times <- x
    date <- format(times$timestamp[1], "%Y-%m-%d")

    which <- match.arg(which)
    p <- if (speed) ggplot(times, aes(x = timestamp, y = speed))
         else ggplot(times, aes(x = timestamp, y = travel_time))
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
    
    p <- p + xlab("Time") +
        ylab(ifelse(speed, "Speed (m/s)", "Travel time (s)")) +
        ylim(c(0, ifelse(speed, 100,
                  ifelse(trim, quantile(times$travel_time, 0.9), max(times$travel_time)))))
    if (attr(x, "n.min") > 1)
        p <- p + ggtitle(sprintf("Segments with %s+ observations",
                                 attr(x, "n.min")))

    if (!missing(estimates)) {
        estimates <- as.data.frame(estimates)
        colnames(estimates) <- c("t", "X", "P")
        est <- estimates %>%
            mutate(t = (min(segtimes$timestamp) + t - 60),
                   q125 = qnorm(0.125, X, sqrt(P)),
                   q25 = qnorm(0.25, X, sqrt(P)),
                   q75 = qnorm(0.75, X, sqrt(P)),
                   q875 = qnorm(0.875, X, sqrt(P)))
        p <- p +
            geom_ribbon(aes(x = t, y = NULL, ymin = q125, ymax = q875),
                        data = est, fill = "darkred") +    
            geom_path(aes(x = t, y = X, group = NULL), data = est,
                      color = "red", lwd = 2, lty = 1)
    }

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

f <- function(x) {
    for (i in 1:Delta) x <- x + Lambda * (Mu - x)
    x
}
F <- function() (1 - Lambda)^Delta


Mu <- 50
Sigma <- 10
Lambda <- 0.005
Delta <- 1
Y <- rbind(c(20, 80, 3),
           c(40, 78, 4),
           c(90, 92, 6),
           c(180, 93, 3),
           c(240, 67, 6))

X <- Mu
P <- Sigma^2
T <- seq(0, 10, by = 0.5)
Q <- Sigma^2 * (1 - (1 - Lambda)^(2 * Delta))
px <- dnorm(Y[, 2], Y[, 2], Y[, 3])
curve(dnorm(x, Mu, Sigma), 1001, from = 0, to = 120, ylim = c(0, 2*max(px)),
      col = "red", lwd = 2)
curve(dnorm(x, X, sqrt(P)), 1001, from = 0, to = 120, add = TRUE)
for (i in 1:1000) {
    Lambda <- 0.01 * P / (Sigma^2 + P)
    Q <- Sigma^2 * (1 - (1 - Lambda)^(2 * Delta))
    X <- f(X)
    P <- F()^2 * P + Q
    j <- which(i * Delta == Y[, 1])
    if (length(j) == 1) {
        print("Observing")
        z <- Y[j, 2]
        r <- Y[j, 3]
        y <- z - X
        S <- P + r^2
        K <- P * (1 / S)
        X <- X + K * y
        P <- (1 - K) * P
    }
    dev.hold()
    curve(dnorm(x, Mu, Sigma), 1001, from = 0, to = 120, ylim = c(0, 2*max(px)),
          col = "red", lwd = 2)
    curve(dnorm(x, X, sqrt(P)), 1001, from = 0, to = 120, add = TRUE)
    abline(v = c(Mu, X), lty = 2, col = c('red', 'black'))
    jj <- which(Y[, 1] <= i * Delta)
    if (length(jj) > 0) {
        for (j in jj) {
            curve(dnorm(x, Y[j, 2], Y[j, 3]),
                  0, 120, 1001, add = TRUE, lwd = 2,
                  col = rgb(0, 0, 1,
                            max(0, 1 - (i * Delta - Y[j, 1]) / 120)))
            #print(max(0, 1 - (i * Delta - Y[j, 1]) / 120))
        }
    }
    title(main = sprintf("State after %s minutes without data",
                         floor(i * Delta / 60)))
    abline(v = Mu, lty = 2)
    dev.flush()
    Sys.sleep(0.1)
}




v <- 1:6 * 10
V <- matrix(NA, ncol = length(v), nrow = 20)
V[1, ] <- v
for (i in 2:nrow(V)) 
    V[i, ] <- v <- f(v, 0.1)

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


######### REPEAT using reeeal data
times <- gettimes(file, n.min = 50)
plottimes(times, speed = FALSE)

f <- function(x) {
    for (i in 1:Delta) x <- x + Lambda * (Mu - x)
    x
}
F <- function() (1 - Lambda)^Delta


segtimes <- times %>%
    filter(segment_id == "389")
plottimes(segtimes, show.peak = FALSE, speed = FALSE) +
    ggtitle("")

Mu <- segtimes %>% .$travel_time %>% mean(na.rm = TRUE)
Sigma <- segtimes %>% .$travel_time %>% sd(na.rm = TRUE)
#    ( function(x) sd(x, na.rm = TRUE) / sqrt(sum(!is.na(x))) )
Lambda <- 0.005
Delta <- 10
Y <- data.frame(t = as.integer(segtimes$timestamp - min(segtimes$timestamp) + 60),
                y = segtimes$travel_time,
                r = sqrt(20))


#saveVideo({
X <- Mu
P <- Sigma^2
Q <- Sigma^2 * (1 - (1 - Lambda)^(2 * Delta))
px <- dnorm(Y[, 2], Y[, 2], Y[, 3])
Ymax <- max(Y$y)
curve(dnorm(x, (Mu), Sigma), 1001, from = 0, to = Ymax, ylim = c(0, 2*max(px)),
      col = "red", lwd = 2, xlab = "Travel Time (s)", ylab = "")
curve(dnorm(x, (X), sqrt(P)), 1001, from = 0, to = Ymax, add = TRUE)
Tmax <- (floor(max(Y$t) / 10) + 6)
pb <- txtProgressBar(0, Tmax, style = 3)
Ystate <- matrix(NA, ncol = 3, nrow = Tmax)
for (i in 1:Tmax) {
    setTxtProgressBar(pb, i)
    Lambda <- 0.001 * P / (Sigma^2 + P)
    Q <- Sigma^2 * (1 - (1 - Lambda)^(2 * Delta))
    X <- f(X)
    P <- F()^2 * P + Q
    tk <- i * Delta
    tk1 <- tk - Delta
    j <- which(Y$t > tk1 & Y$t <= tk)
    if (length(j) > 0) {
        z <- (mean(Y$y[j]))
        r <- mean(Y$y[j]^2 + Y$r[j]^2) - z^2
        ## for (k in j) {
        ## z <- Y[k, 2]
        ## r <- Y[k, 3]
        y <- z - X
        S <- P + r^2
        K <- P * (1 / S)
        X <- X + K * y
        P <- (1 - K) * P
        ## }
    }
    Ystate[i, ] <- c(tk, X, P)
    dev.hold()
    curve(dnorm(x, (Mu), Sigma), 1001, from = 0, to = Ymax, ylim = c(0, 2*max(px)),
          col = "red", lwd = 2, xlab = "Travel time (s)", ylab = "")
    curve(dnorm(x, (X), sqrt(P)), 1001, from = 0, to = Ymax, add = TRUE)
    abline(v = c((Mu), (X)), lty = 2, col = c('red', 'black'))
    jj <- which(Y$t <= tk)
    if (length(jj) > 0) {
        for (j in jj) {
            curve(dnorm(x, Y[j, 2], Y[j, 3]),
                  0, Ymax, 1001, add = TRUE, lwd = 2,
                  col = rgb(0, 0, 1,
                            max(0, 1 - (i * Delta - Y[j, 1]) / 60 / 15)))
        }
    }
    title(main = sprintf("State at %s",
                         min(segtimes$timestamp) + tk - 60))
    abline(v = (Mu), lty = 2)
    dev.flush(dev.flush())
}; close(pb)

#}, 'travelstate.mp4', interval = 1 / 60)


## Yhist <- as.data.frame(Ystate)
## colnames(Yhist) <- c("t", "X", "P")
## Yhist %<>% 
##     mutate(t = (min(segtimes$timestamp) + t - 60),
##            q125 = qnorm(0.125, X, sqrt(P)),
##            q25 = qnorm(0.25, X, sqrt(P)),
##            q75 = qnorm(0.75, X, sqrt(P)),
##            q875 = qnorm(0.875, X, sqrt(P)))

plottimes(segtimes, which = 'combined', show.peak = FALSE,
          speed = FALSE, span = NULL, trim = FALSE,
          estimates = Ystate)

    geom_ribbon(aes(x = t, ymin = q125, ymax = q875),
                data = Yhist, fill = "darkred") +    
    geom_path(aes(x = t, y = X, group = NULL), data = Yhist,
              color = "red", lwd = 2, lty = 1)


    geom_path(aes(x = t, y = qnorm(0.25, X, sqrt(P)), group = NULL),
              data = Ystate, color = "orangered", lty = 2) +
    geom_path(aes(x = t, y = qnorm(0.75, X, sqrt(P)), group = NULL),
              data = Ystate, color = "orangered", lty = 2) +
    geom_path(aes(x = t, y = qnorm(0.025, X, sqrt(P)), group = NULL),
              data = Ystate, color = "orangered", lty = 3) +
    geom_path(aes(x = t, y = qnorm(0.975, X, sqrt(P)), group = NULL),
              data = Ystate, color = "orangered", lty = 3) 
