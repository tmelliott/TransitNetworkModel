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

segments <- getsegments()
load("model_results.rda")


## Now model is fitted, need to transform B into B matrices for each segment

rstan::summary(stan.fit, pars = c("sigma", "tau", "omega", "alpha", "lp__"),
               prob = c(0.025, 0.5, 0.975))$summary

plotfit <- function(fit, data, seg, Bs,
                    type = c('median', 'mean', 'mode', 'random')) {
    sid <- levels(data$segment_id %>% as.factor)[seg]
    type <- match.arg(type)
    data <- data %>% filter(segment_id == sid & !weekend)
    xd <- with(data, seq(min(dist), max(dist), length.out = 101))
    xt <- with(data, seq(min(time), max(time), length.out = 101))

    knots <- attr(Bs, "knots")[[seg]]
    betaj <- which(attr(Bs, "sk") == sid)

    pars2keep <- c("tau", "omega", "alpha",
                   paste0("beta[", betaj, "]"))
    sims <-
        switch(type,
               "median" = summary(fit, pars = pars2keep, prob = c(0.5))$summary[, "50%"],
               "mean" = summary(fit, pars = pars2keep, prob = NULL)$summary[, "mean"],
               "mode" = {

               },
               "random" = {

               })
    beta <- sims[grep("beta", names(sims))] %>% as.numeric
    B <- bs(xd, knots = knots, intercept = TRUE)
    pred <- outer(1:length(xd), xt, function(j, t) {
        B[j, ] %*% beta
    })
    with(data, plot3d(dist, time, speed, aspect = c(3, 5, 1)))
    surface3d(xd, xt, pred, grid = FALSE, color = "#990000")
}

plotfit(stan.fit, ds, 1, Bs, "median")
plotfit(stan.fit, ds, 2, Bs, "median")
plotfit(stan.fit, ds, 3, Bs, "median")
plotfit(stan.fit, ds, 4, Bs, "median")
plotfit(stan.fit, ds, 5, Bs, "median")

