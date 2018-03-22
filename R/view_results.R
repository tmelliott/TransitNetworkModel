cat(" * loading packages\n")
suppressPackageStartupMessages({
    library(tidyverse)
    library(ggmap)
    library(viridis)
    library(RSQLite)
    library(Rcpp)
    library(rgl)
    library(splines)
    library(sf)
})

source("common.R")

load("model_results.rda")

arr <- jfit$BUGSoutput$sims.array
rdf <- do.call(rbind, lapply(1:dim(arr)[2], function(i) {
    arr[,i,] %>% as.tibble %>%
        mutate(chain = i, it = 1:n())
})) %>%
    rename_if(grepl("[", names(.), fixed=T),
              function(x)
                  gsub(",", "_", gsub("`|\\[|\\]", "", x))) %>%
    mutate(chain = as.factor(chain))
rm(arr)   

pairs((rdfx <- rdf %>% select_if(!grepl("beta|^r.|chain|it", names(.)))),
      col = rdf$chain)

op <- par(mfrow = c(5, 5))
for (i in colnames(rdfx))
    hist(rdf[[i]], 50, xlab=i,main=i)
par(op)


for (i in colnames(rdf)) {
    quo_var <- quo(i)
    p <- ggplot(rdf, aes(x = it, group = chain, colour = chain)) +
        geom_line(aes_string(y = i))
    print(p)
    grid::grid.locator()
}


which <- 'sample'

plotPlane(ds, jfit, 1, Bs, which = which)
plotPlane(ds, jfit, 2, Bs, which = which)
plotPlane(ds, jfit, 3, Bs, which = which)
plotPlane(ds, jfit, 4, Bs, which = which)
plotPlane(ds, jfit, 5, Bs, which = which)
