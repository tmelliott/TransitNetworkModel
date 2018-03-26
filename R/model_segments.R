message(" * loading packages")
suppressPackageStartupMessages({
    library(tidyverse)
    library(ggmap)
    library(viridis)
    library(RSQLite)
    library(Rcpp)
    library(sf)
    library(mgcv)
    library(splines)
    library(rstan)
})

source("common.R")


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

## Now convert Bs into a single sparse matrix ...
message(" * preparing sparse basis matrices")
Bs <- tapply(1:nrow(ds), ds$segment_id, function(i) {
    x <- ds$dist[i]
    b <- splines::bs(x, df = floor(max(ds$dist[i]) / 200), intercept = TRUE)
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
Knots <- lapply(Bs, function(x) attr(x, "knots"))
Bj <- lapply(Bs, function(x) max(x$j)) %>% as.integer
Sk <- rep(names(Bs), Bj)
Bj <- c(0L, cumsum(Bj)[-length(Bj)])
names(Bj) <- names(Bs)
Bs <- lapply(names(Bs), function(sid) {
    Bs[[sid]] %>% mutate(j = j + Bj[sid])
})
B <- do.call(rbind, Bs)
attr(Bs, "knots") <- Knots
attr(Bs, "sk") <- Sk


message(" * fitting model")
stan.fit <-
    rstan::stan("stan/hier_seg_model.stan",
                data = list(Bij = B[,1:2] %>% as.matrix,
                            Bval = B$beta,
                            M = nrow(B),
                            t = ds$time,
                            y = ds$speed,
                            N = nrow(ds),
                            s = ds$segment_id %>% as.factor %>% as.numeric,
                            L = length(unique(ds$segment_id)),
                            K = max(B$j),
                            sk = as.integer(Sk)),
                cores = 4
                )


message(" * writing results to file")
save(ds, stan.fit, Bs, file = "model_results.rda")

message(" * done")
