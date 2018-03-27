message(" * loading packages")
suppressPackageStartupMessages({
    library(tidyverse)
    library(viridis)
    library(RSQLite)
    library(rgl)
    library(splines)
    library(rstan)
})

source("common.R")
segments <- getsegments()


load("model_results.rda")

## Now model is fitted, need to transform B into B matrices for each segment

smrypars <- c("tau", "omega", "alpha")
summary(stan.fit, pars = smrypars,
        prob = c(0.025, 0.5, 0.975))$summary
plot(stan.fit, pars = c("omega", "alpha"))

plotfit <- function(fit, data, seg, Bs,
                    type = c('median', 'mean', 'mode', 'random',
                             'trace', 'hist')) {
    sid <- levels(data$segment_id %>% as.factor)[seg]
    type <- match.arg(type)

    knots <- attr(Bs, "knots")[[seg]]
    betaj <- which(attr(Bs, "sk") == sid)
    pars2keep <- c("tau", "omega", "alpha", "sigma",
                   paste0("beta[", betaj, "]"))

    if (type %in% c("trace", "hist"))
        return(plot(fit, plotfun = type, pars = pars2keep))
    
    data <- data %>% filter(segment_id == sid & !weekend)
    xd <- with(data, seq(min(dist), max(dist), length.out = 101))
    xt <- with(data, seq(min(time), max(time), length.out = 101))

    sims <-
        switch(type,
               "median" = summary(fit, pars = pars2keep, prob = c(0.5))$summary[, "50%"],
               "mean" = summary(fit, pars = pars2keep, prob = NULL)$summary[, "mean"],
               "mode" = {
                   lp <- get_logposterior(fit) %>% unlist
                   as.matrix(fit, pars = pars2keep)[which(lp == max(lp)), ]
               },
               "random" = {
                   mm <- as.matrix(fit, pars = pars2keep)
                   mm[sample(nrow(mm), 1), ]
               })
    beta <- sims[grep("beta", names(sims))] %>% as.numeric
    B <- bs(xd, knots = knots, intercept = TRUE)
    pred <- outer(1:length(xd), xt, function(j, t) {
        B[j, ] %*% beta
    })
    with(data, plot3d(dist, time, speed, aspect = c(3, 5, 1)))
    surface3d(xd, xt, pred, grid = FALSE, color = "#990000")
}

w <- "random"
plotfit(stan.fit, ds, 1, Bs, "random")
plotfit(stan.fit, ds, 2, Bs, "random")
plotfit(stan.fit, ds, 3, Bs, "random")
plotfit(stan.fit, ds, 4, Bs, "random")
plotfit(stan.fit, ds, 5, Bs, "random")


plotfit(stan.fit, ds, 4, Bs, "trace")
