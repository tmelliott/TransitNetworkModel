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
if (file.exists("thedata.rda")) {
    load("thedata.rda")
} else {
    ds <- data %>%
        filter(segment_id %in% segs) %>%
        filter(!is.na(speed)) %>%
        mutate(route = substr(route_id, 1, 3),
               date = format(as.POSIXct(timestamp, origin = "1970-01-01"),
                             "%Y-%m-%d")) %>%
        group_by(segment_id) %>%
        do((.) %>%
           mutate(dist = distIntoShape(., segments %>%
                                          filter(id == segment_id[1])))) %>%
        ungroup()
    save(ds, file = "thedata.rda")
}

startt <-
    tapply(1:nrow(ds),
           paste0(ds$vehicle_id, ds$segment_id, ds$date, ds$trip_id),
           function(i) min(ds$timestamp[i]))
ds <- ds %>%
    mutate(segment_time = timestamp -
               startt[paste0(vehicle_id, segment_id, date, trip_id)])

hist(ds$segment_time)
    #group_by(interaction(vehicle_id, segment_id, date, trip_id)) %>%
    #do((.) %>% mutate(segment_time = timestamp - min(timestamp))) %>%
    #ungroup 

ggplot(ds, aes(segment_time/60, dist, group = interaction(vehicle_id, trip_id))) +
    geom_path(aes(colour = date)) +
    facet_wrap(~segment_id, scales = "free") +
    theme(legend.position = "none") +
    xlab("Time in segment (min)")


sis <- unique(ds$segment_id)
ggplot(ds %>% filter(segment_id == sis[3] & speed > 0) %>%
       mutate(peak = case_when(time > 7.5 & time < 8.5 ~ "peak (morning)",
                               time > 17 & time < 18 ~ "peak (evening)",
                               time > 11 & time < 13 ~ "off",
                               TRUE ~ "")) %>%
       filter(peak != ""),
       aes(dist, speed, colour = peak)) +
    geom_point() +
    geom_smooth(span = 0.4)


## Now convert Bs into a single sparse matrix ...
message(" * preparing sparse basis matrices")
Bs <- tapply(1:nrow(ds), ds$segment_id, function(i) {
    x <- ds$dist[i]
    b <- splines::bs(x, df = floor(max(ds$dist[i]) / 200), intercept = FALSE)
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
    rstan::stan("stan/hier_seg_model2.stan",
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
                cores = 4,
                pars = c("eta", "alpha"), include = FALSE
                )


message(" * writing results to file")
save(ds, stan.fit, Bs, file = "model_results.rda")

message(" * done")











############################### Process a single trip

library(tidyverse)
library(RSQLite)
library(dbplyr)
library(lubridate)

con <- dbConnect(SQLite(), "rawhistory.db")

vps <- tbl(con, 'vps') %>%
    mutate(date = strftime('%Y-%m-%d', datetime(timestamp, 'unixepoch')))

c2 <- dbConnect(SQLite(), "../../TransitNetworkModel/gtfs.db")
routes <- tbl(c2, 'routes')
## trips <- tbl(c2, 'trips')

shapes <- tbl(c2, 'shapes') %>%
    inner_join(routes) %>%
    select(shape_id, seq, lat, lng, route_id)

dates <- vps %>%
    select(date) %>% distinct %>% collect
date <- dates$date[1]

trips <- vps %>%
    group_by(trip_id) %>%
    summarize(n = n()) %>%
    arrange(desc(n)) %>%
    collect

routes <- vps %>%
    group_by(route_id) %>%
    summarize(n = n()) %>%
    arrange(desc(n)) %>%
    collect

trip <- trips$trip_id[1]
route <- routes$route_id[1]
x <- vps %>%
    filter(route_id == route & date == date) %>%
    select(vehicle_id, route_id, trip_id, trip_start_time, timestamp,
           position_latitude, position_longitude, date) %>%
    arrange(timestamp) %>%
    mutate(lat = position_latitude, lng = position_longitude) %>%
    collect

z <- process_trip(x)

for (i in 1:nrow(routes)) {
    route <- routes$route_id[i]
    x <- vps %>%
        filter(route_id == route) %>%
        select(vehicle_id, route_id, trip_id, trip_start_time, timestamp,
               position_latitude, position_longitude, date) %>%
        arrange(timestamp) %>%
        collect
    z <- process_trip(x)
    if (is.null(z)) next
    grid::grid.locator()
}

process_trip <- function(x) {

    x$time <- format(as.POSIXct(x$timestamp, origin = "1970-01-01"), "%H:%M:%S")

    tstart <- hms(x$trip_start_time)
    x <- x %>%
        filter(hms(time) >= tstart - minutes(5) &
               hms(time) < tstart + minutes(90))

    if (nrow(x) <= 1) return()
    
    rid <- gsub('-.*', '', x$route_id[1])
    shape <- shapes %>%
        filter(route_id %like% paste0(rid, '%')) %>%
        arrange(seq) %>%
        collect
    
    x <- x %>% group_by(date, trip_id, vehicle_id) %>%
        do((.) %>% arrange(timestamp) %>%
           mutate(
               dt = c(0, diff(timestamp)),
               dx = c(0,
                      if ((.) %>% nrow < 2) {
                          numeric()
                      } else if ((.) %>% nrow == 2) {
                          zz <- cbind(position_longitude, position_latitude)
                          geosphere::distGeo(zz[1,], zz[2,])
                      } else {
                          geosphere::distGeo(
                              cbind(position_longitude, position_latitude))
                      }),
               dist = cumsum(dx),
               speed = dx / dt / 1000 * 60 * 60
           )) %>%
        ungroup()

    s0 <- x %>% filter(speed == 0)

    gridExtra::grid.arrange(
        ggplot(x %>% filter(speed > 0) %>% arrange(timestamp),
               aes(position_longitude, position_latitude)) +
        geom_path(aes(lng, lat), data = shape, lwd = 2) +
        geom_path(aes(colour = speed, group = interaction(vehicle_id, trip_id)),
                  lwd = 2) +
        geom_point(aes(size = dt), data = s0, color = "red") +
        facet_wrap(~date, nrow = 1) +
        coord_fixed(1.3) +
        labs(size = "Dwell time (seconds)") +
        scale_colour_viridis(option = 'C'),
        ggplot(x %>% filter(speed > 0),
               aes(dist, speed, group = interaction(date, vehicle_id, trip_id))) +
        geom_path(),
        ##geom_smooth(method = "gam", se=FALSE),
        nrow = 2)
        
}

process_trip(x)
