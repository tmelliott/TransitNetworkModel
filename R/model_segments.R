suppressPackageStartupMessages({
    library(tidyverse)
    library(ggmap)
    library(viridis)
    library(RSQLite)
    library(Rcpp)
    library(rgl)
    library(sf)
})


data <- dbReadTable(dbConnect(SQLite(), "history.db"), "vps")

head(data)
