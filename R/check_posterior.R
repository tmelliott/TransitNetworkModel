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

smrypars <- c("tau", "omega", "mu_alpha", "intercept")
summary(stan.fit, pars = smrypars,
        prob = c(0.025, 0.5, 0.975))$summary
plot(stan.fit, pars = c("mu_alpha"))

plotfit <- function(fit, data, seg, Bs,
                    type = c('median', 'mean', 'mode', 'random',
                             'trace', 'hist', 'pairs'),
                    pars = pars2keep[!grepl("beta", pars2keep)]) {
    sid <- levels(data$segment_id %>% as.factor)[seg]
    type <- match.arg(type)

    knots <- attr(Bs, "knots")[[seg]]
    betaj <- which(attr(Bs, "sk") == sid)
    pars2keep <- c("tau", "omega", "sigma",
                   sprintf("intercept[%d]", seg),
                   paste0(sprintf("mu_alpha[%d", seg), ",", 1:2, "]"),
                   paste0("beta[", betaj, "]"))

    if (type %in% c("trace", "hist"))
        return(plot(fit, plotfun = type, pars = pars))
    if (type == "pairs")
        return(pairs(fit, pars = pars))
    
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
    intercept <- sims[grep("intercept", names(sims))] %>% as.numeric
    beta <- sims[grep("beta", names(sims))] %>% as.numeric
    alpha <- sims[grep("mu_alpha", names(sims))] %>%
        as.numeric %>% pmax(0)
    tau <- sims[grep("tau", names(sims))] %>% as.numeric
    omega <- sims[grep("omega", names(sims))] %>% as.numeric
    B <- bs(xd, knots = knots)
    pred <- outer(1:length(xd), xt, function(j, t) {
        intercept + B[j, ] %*% beta -
            sapply(t, function(ti)
                sum(alpha * exp(-(ti - tau)^2 / 2 * omega^2)))
    })
    with(data, plot3d(dist, time, speed, aspect = c(3, 5, 1)))
    surface3d(xd, xt, pred, grid = FALSE, color = "#990000")
}

#w <- "random"
#plotfit(stan.fit, ds, 1, Bs, "random")
#plotfit(stan.fit, ds, 2, Bs, "random")
#plotfit(stan.fit, ds, 3, Bs, "random")
#plotfit(stan.fit, ds, 4, Bs, "random")
#plotfit(stan.fit, ds, 5, Bs, "random")


plotfit(stan.fit, ds, 1, Bs, "trace")
plotfit(stan.fit, ds, 1, Bs, "hist")
plotfit(stan.fit, ds, 1, Bs, "pairs",
        pars = c("intercept", paste0("beta[", 1:10, "]")))
