library(tidyverse)
library(magrittr)
library(ggmap)
library(geosphere)
library(viridis)
library(RSQLite)
library(mgcv)
library(sf)
library(rgl)
library(Rcpp)

### --- Step 1: Load the data
date <- "2018-02-12"

dir <- "historicaldata"
files <- paste0(dir, "/", c("vehicle_positions", "trip_updates"),
                "_", date, ".csv")

#vps <- read.csv(files[1])
#tus <- read.csv(files[2])

### --- Step 2: Filter duplicate rows
vps <- read.csv(files[1],
                colClasses = c("factor", "factor", "factor", "character",
                               "numeric", "numeric", "numeric", "integer")) %>%
    group_by(vehicle_id, timestamp) %>%
    filter(row_number() == 1) %>%
    ungroup() %>%
    arrange(timestamp)
tus <- read.csv(files[2],
                colClasses = c("factor", "factor", "factor", "character",
                               "integer", "factor", "integer", "integer",
                               "integer", "integer", "integer")) %>%
    group_by(vehicle_id, timestamp) %>%
    filter(row_number() == 1) %>%
    ungroup() %>%
    arrange(timestamp)

### --- Step 3: Merge into a single file

v1 <- vps %>%
    select(vehicle_id, timestamp, trip_id, route_id, trip_start_time,
           position_latitude, position_longitude, position_bearing)
t1 <- tus %>%
    select(vehicle_id, timestamp, trip_id, route_id, trip_start_time,
           stop_sequence, stop_id, arrival_time, arrival_delay, departure_time,
           departure_delay)

data <- full_join(v1, t1) %>%
    arrange(timestamp, vehicle_id) %>%
    mutate(delay = ifelse(is.na(arrival_delay), departure_delay,
                          arrival_delay)) %>%
    mutate(delaycat = cut(delay/60, c(-Inf, -30, -10, -5, 5, 10, 30, Inf)))

### --- Step 4: Do stuff with the data!
vid <- names(sort(table(data$vehicle_id), TRUE))[19]
dv <- data %>%
    filter(vehicle_id == vid) %>%
    filter(timestamp > as.numeric(as.POSIXct(date))) %>%
    arrange(timestamp)
ggplot(dv, aes(position_longitude, position_latitude)) +
    geom_point() + coord_fixed(1.2)

bbox <- with(dv, c(min(position_longitude, na.rm = TRUE),
                   min(position_latitude, na.rm = TRUE),
                   max(position_longitude, na.rm = TRUE),
                   max(position_latitude, na.rm = TRUE)))
aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

p <- ggmap(aklmap) +
    #geom_path(aes(x = position_longitude, y = position_latitude),
    #          data = dv, lty = 3) +
    geom_point(aes(x = position_longitude, y = position_latitude,
                   colour = delaycat),
               data = dv)
p

p + facet_grid(~route_id)

p + facet_wrap(~trip_start_time)


### --- Step 5: Load road segments from the database
getsegments <- function(whole.shapes = FALSE) {
    if (whole.shapes) {
        con <- dbConnect(SQLite(), "../gtfs.db")
        segments <- dbGetQuery(
            con,
            "SELECT shape_id AS id, lat, lng FROM shapes ORDER BY shape_id, seq")
        dbDisconnect(con)
        return(segments)
    }
    ## check if they exist yet ...
    if (file.exists("segments.rda")) {
        load("segments.rda")
    } else {
        con <- dbConnect(SQLite(), "../gtfs.db")
        segments <- dbGetQuery(con, "SELECT segment_id FROM segments") %>%
            pluck("segment_id") %>% lapply(function(x) list(id = x, shape = NULL))
        pb <- txtProgressBar(0, length(segments), style = 3)
        for (i in i:length(segments)) {
            setTxtProgressBar(pb, i)
            x <- segments[[i]]$id
            ## get a shape that uses this segment
            q <- dbSendQuery(con, "SELECT shape_id, leg FROM shape_segments
                                   WHERE segment_id=? LIMIT 1")
            dbBind(q, x)
            shp <- dbFetch(q)
            dbClearResult(q)
            if (nrow(shp) == 0) next
            ## get the start/end distances for that shape
            q <- dbSendQuery(con, "SELECT shape_dist_traveled FROM shape_segments
                                   WHERE shape_id=? AND LEG BETWEEN ? AND ?")
            dbBind(q, list(shp$shape_id, shp$leg, shp$leg + 1))
            dr <- dbFetch(q)$shape_dist_traveled
            dbClearResult(q)
            if (length(dr) == 1) dr <- c(dr, Inf)
            ## get the shape points for the shape in the required distance range
            q <- dbSendQuery(con, "SELECT lat, lng FROM shapes
                                   WHERE shape_id=? AND
                                         dist_traveled BETWEEN ? AND ?
                                   ORDER BY seq")
            dbBind(q, list(shp$shape_id, dr[1], dr[2]))
            segments[[i]]$shape <- dbFetch(q)
            dbClearResult(q)
        }
        close(pb)
        dbDisconnect(con)
        segments <-
            segments[sapply(segments, function(x) !is.null(x$shape))] %>%
            lapply(function(x)
                data.frame(id = rep(x$id, nrow(x$shape)), x$shape)) %>%
            do.call(rbind, .) %>%
            mutate(id = as.factor(id))
        save(segments, file = "segments.rda")
    }
    
    segments %>% as.tibble
}

segments <- getsegments(FALSE)

pseg <- ggplot(segments, aes(x = lng, y = lat, group = id)) +
    geom_path() +
    coord_fixed(1.2) +
    xlim(174.7, 174.9) + ylim(-37, -36.8)
pseg


### --- Step 6: model vehicle trajectories to estimate speed
cx <- paste(data$position_latitude, data$position_longitude, sep = ":") %>% table
repPts <- names(cx)[cx > 1]

ds <- data %>%
    filter(!is.na(position_latitude)) %>%
    filter(!paste(position_latitude, position_longitude,
                  sep = ":") %in% repPts) %>%
    arrange(vehicle_id, timestamp) %>%
    mutate(delta_t = c(0, diff(timestamp)),
           delta_d = c(0, distHaversine(cbind(position_longitude[-n()],
                                              position_latitude[-n()]),
                                        cbind(position_longitude[-1],
                                              position_latitude[-1])))) %>%
    mutate(speed = pmin(30, delta_d / delta_t)) %>%
    mutate(speed = ifelse(speed > 0, speed, NA),
           hour = format(as.POSIXct(timestamp, origin = "1970-01-01"), "%H"))

bbox <- with(ds, c(min(position_longitude, na.rm = TRUE),
                   min(position_latitude, na.rm = TRUE),
                   max(position_longitude, na.rm = TRUE),
                   max(position_latitude, na.rm = TRUE)))
bbox <- c(174.7, -37, 174.9, -36.8)
aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

p <- ggmap(aklmap) +
    geom_path(aes(x = lng, y = lat, group = id), data = segments) +
    geom_point(aes(x = position_longitude, y = position_latitude,
                   colour = speed / 1000 * 60 * 60),
               data = ds, size = 0.4) +
    scale_color_viridis() +
    #scale_color_gradientn(colours = c("#990000", viridis(6)[5:6])) +
    labs(colour = "Speed (km/h)")
p

##p + facet_wrap(~hour, nrow = 4)


### For each segment, find all points in it


library(sf)
rad <- function(d) d * pi / 180
deg <- function(r) r * 180 / pi
R <- 6378137

segs.geoms <-
    do.call(rbind, {
        tapply(1:nrow(segments), segments$id,
               function(i) {
                   if (length(i) == 1) return(NULL)
                   ## conversions
                   lat <- rad(segments$lat[i])
                   lng <- rad(segments$lng[i])
                   Lam <- mean(lng)
                   Phi <- mean(lat)
                   ## equirectangular proj
                   x <- (lng - Lam) * cos(Phi)
                   y <- lat - Phi
                   ## polygon buffer around path
                   buf <- st_buffer(st_linestring(cbind(x, y) * R), 15)
                   ## simple df with polygon, in geospace
                   coords <-
                       st_coordinates(buf) %>%
                       as.data.frame %>%
                       mutate(id = segments$id[i[1]],
                              lng = deg(X / R / cos(Phi) + Lam),
                              lat = deg(Y / R + Phi))
                   coords
               })
    })

i <- 16

segid <- levels(segments$id %>% as.factor)[i]

for (segid in levels(segments$id %>% as.factor)) {
s <- segs.geoms %>% filter(id == segid)
ggplot(s) +
    geom_path(aes(lng, lat, group = id), data = segments) +
    geom_point(aes(position_longitude, position_latitude),
               data = ds) +
    geom_polygon(aes(lng, lat, group = id), fill = "red", alpha = 0.5) +
    xlim(min(s$lng) - 0.01, max(s$lng) + 0.01) +
    ylim(min(s$lat) - 0.005, max(s$lat) + 0.005) +
    coord_fixed(1.2)
if (nrow(s) == 0) next
bbox <- with(s, c(min(lng, na.rm = TRUE) - 0.01,
                  min(lat, na.rm = TRUE) - 0.005,
                  max(lng, na.rm = TRUE) + 0.01,
                  max(lat, na.rm = TRUE) + 0.005))
## aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

## ggmap(aklmap) +
##     geom_point(aes(position_longitude, position_latitude),
##                data = ds) +
##     geom_polygon(aes(lng, lat, group = id), data = s,
##                  lwd = 1, color = 'red',
##                  fill = "red", alpha = 0.2)


## get all segment/shape matching stuff
con <- dbConnect(SQLite(), '../gtfs.db')
q <- dbSendQuery(con, 'SELECT DISTINCT shape_segments.shape_id, routes.route_id FROM shape_segments, routes WHERE shape_segments.shape_id=routes.shape_id AND segment_id=?')
dbBind(q, list(segid))
segroutes <- dbFetch(q)$route_id
dbClearResult(q)
dbDisconnect(con)

## Find points in the polygon
dx <- ds %>% filter(position_longitude > bbox[1] &
                    position_longitude < bbox[3] &
                    position_latitude > bbox[2] &
                    position_latitude < bbox[4])
POLY <- cbind(s$lng, s$lat) %>%
    as.matrix %>% list %>% st_polygon

vpos <- do.call(st_sfc, lapply(1:nrow(dx), function(i) {
    st_point(as.numeric(c(dx$position_longitude[i],
                          dx$position_latitude[i])))
}))
dx$inseg <- sapply(st_intersects(vpos, POLY), function(x) length(x) > 0) &
    dx$route_id %in% segroutes

gridExtra::grid.arrange(
    ## ggmap(aklmap) +
    ggplot() +
    geom_polygon(aes(lng, lat, group = id), data = s,
                 lwd = 1, 
                 fill = "magenta", alpha = 0.5) +
    geom_point(aes(position_longitude, position_latitude,
                   color = inseg), data = dx) +
    scale_colour_manual(values = c('#666666', 'white')) +
    theme(legend.position = "none"),
    ggplot(dx %>%
           filter(inseg), aes(as.POSIXct(timestamp, origin='1970-01-01'),
                              speed / 1000 * 60 * 60)) +
    geom_point() + #geom_smooth(method = lm, formula = y ~ splines::bs(x, 10)) +
    xlab("Time") + ylab("Approx. speed (km/h)") + ylim(0, 100),
    ncol = 1, heights = c(2, 1)
    )

grid::grid.locator()
}

## 3D plot
speed2col <- function(v, palette = viridis, r = range(pmin(30, pmax(0, v))), ...) {
    cols <- palette(101, ...)
    w <- round((v - r[1]) / diff(r) * 100)
    cols[w]
}

sinseg <- dx %>%
    filter(inseg) %>%
    filter(!is.na(speed)) %>%
    mutate(x = R * (rad(position_longitude) - rad(mean(ds$position_longitude))) *
               cos(rad(mean(ds$position_latitude))),
           y = R * (rad(position_latitude) - rad(mean(ds$position_latitude))),
           .t = as.POSIXct(timestamp, origin = '1970-01-01'),
           second = format(.t, '%S') %>% as.numeric,
           minute = format(.t, '%M') %>% as.numeric + second / 60,
           time = format(.t, '%H') %>% as.numeric + minute / 60)
plot3d(sinseg$x, sinseg$y, sinseg$time, box = FALSE,
       aspect = c(1, diff(range(sinseg$y)) / diff(range(sinseg$x)), 4), 
       col = speed2col(sinseg$speed, inferno), size = 1, type = "s",
       xlab = 'Longitude', ylab = 'Latitude', zlab = 'Time (hour)')




###### Back to full dataset ...
### Smooth the value at each point ...
ds <- ds %>%
    filter(!is.na(speed)) %>%
    mutate(x = R * (rad(position_longitude) - rad(mean(ds$position_longitude))) *
               cos(rad(mean(ds$position_latitude))),
           y = R * (rad(position_latitude) - rad(mean(ds$position_latitude))))


##' Smooth spatially and temporally
##'
##' Smooth a value by spatial and temporal values.
##' Uses simple uniform kernel (all values within the block are equally weighted).
##' 
##' @title Spatial-temporal Smooth
##' @param value the response, or a formula value ~ x + y + z
##' @param x the x/longitude values
##' @param y the y/latitude values
##' @param z the time value
##' @param data if value is a formula, this is the data argument
##' @param distance smoothing distance in x/y dimensions
##' @param time smoothing distance in z (time) dimension
##' @return a numeric vector of smoothed values
##' @author Tom Elliott
smoothSpatTemp <- function(value, x = NULL, y = NULL, z = NULL, data = NULL,
                           distance = 20,
                           time = 0.5) {
    if (class(value) == 'formula') {
        X <- model.frame(value, data)
        colnames(X) <- c("value", "x", "y", "z")
    } else {
        X <- data.frame(value, x, y, z)
    }
    invisible(smthSpatTmp(as.matrix(X), distance, time))
}

sourceCpp('smoothSpatTemp.cpp')
dsf <- ds %>%
    mutate(.t = as.POSIXct(timestamp, origin = '1970-01-01'),
           second = format(.t, '%S') %>% as.numeric,
           minute = format(.t, '%M') %>% as.numeric + second / 60,
           time = format(.t, '%H') %>% as.numeric + minute / 60) %>%
    mutate(speed.smooth = smoothSpatTemp(speed, x, y, time, t = 0.25))

rids <- sort(table(dsf$route_id), TRUE)
SI <- 7
with(dsf %>% filter(route_id %in% names(rids)[SI]),
     plot3d(x, y, time, box = FALSE, type = "s", size = 1,
            aspect = c(1, diff(range(sinseg$y)) / diff(range(sinseg$x)), 4), 
            col = speed2col(speed.smooth, inferno),
            xlab = 'Longitude', ylab = 'Latitude', zlab = 'Time (hour)'))

bbox <- with(dsf%>% filter(route_id == names(rids)[SI]),
             c(min(position_longitude, na.rm = TRUE),
               min(position_latitude, na.rm = TRUE),
               max(position_longitude, na.rm = TRUE),
               max(position_latitude, na.rm = TRUE)))
aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

p <- ggmap(aklmap) +
    #geom_path(aes(x = lng, y = lat, group = id), data = segments) +
    geom_point(aes(x = position_longitude, y = position_latitude,
                   colour = speed / 1000 * 60 * 60),
               data = dsf %>% filter(route_id == names(rids)[SI]), size = 1) +
    scale_color_gradientn(colours = c("#990000", viridis(6)[5:6]),
                          limits = c(0, 100)) +
    labs(colour = "Speed (km/h)") +
    facet_wrap(~hour, nrow = 3)
p

ps <- ggmap(aklmap2) +
    #geom_path(aes(x = lng, y = lat, group = id), data = segments) +
    geom_point(aes(x = position_longitude, y = position_latitude,
                   colour = speed.smooth / 1000 * 60 * 60),
               data = dsf, size = 1) +
    scale_color_gradientn(colours = c("#990000", viridis(6)[5:6]),
                          limits = c(0, 100)) +
    labs(colour = "Speed (km/h)") +
    facet_wrap(~hour, nrow = 3)
ps

gridExtra::grid.arrange(p, ps, nrow = 1)


for (t in seq(min(ds$timestamp) + 60 * 60 * 2,
              max(ds$timestamp), by = 30)) {
    dev.flush(dev.flush())
    pt <- ggmap(aklmap) +
        geom_point(aes(x = position_longitude, y = position_latitude,
                       color = speed.smooth),
                   data = ds2 %>% filter(timestamp > t - 15 & timestamp < t + 15)) +
        ggtitle(as.POSIXct(t, origin = "1970-01-01")) +
        scale_color_viridis(limits = c(0, 30))
        ##scale_color_gradientn(colours = c("red", "green4", "yellow"), limits = c(0, 30))
    dev.hold()
    print(pt)
    dev.flush()
}

i <- 411
s <- segs.geoms %>% filter(id == i)
bbox <- with(s, c(min(lng, na.rm = TRUE) - 0.01,
                  min(lat, na.rm = TRUE) - 0.005,
                  max(lng, na.rm = TRUE) + 0.01,
                  max(lat, na.rm = TRUE) + 0.005))
aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")
dx <- dsf %>% filter(position_longitude > bbox[1] &
                     position_longitude < bbox[3] &
                     position_latitude > bbox[2] &
                     position_latitude < bbox[4])
POLY <- cbind(s$lng, s$lat) %>%
    as.matrix %>% list %>% st_polygon
vpos <- do.call(st_sfc, lapply(1:nrow(dx), function(i) {
    st_point(as.numeric(c(dx$position_longitude[i],
                          dx$position_latitude[i])))
}))
dx$inseg <- sapply(st_intersects(vpos, POLY), function(x) length(x) > 0)
gridExtra::grid.arrange(
    ggmap(aklmap) +
    geom_polygon(aes(lng, lat, group = id), data = s,
                 lwd = 1, 
                 fill = "magenta", alpha = 0.5) +
    geom_point(aes(position_longitude, position_latitude,
                   color = inseg),
               data = dx) +
    scale_colour_manual(values = c('#666666', 'white')) +
    theme(legend.position = "none"),
    ggplot(dx %>%
           filter(inseg), aes(as.POSIXct(timestamp, origin='1970-01-01'),
                              speed.smooth / 1000 * 60 * 60)) +
    geom_point() + geom_smooth(method = 'loess', span = 0.2) +
    xlab("Time") + ylab("Approx. speed (km/h)") + ylim(0, 100),
    ncol = 1, heights = c(2, 1)
)





####### Alternative segmentation ... yikes!
## So basically, turning polygons back into lines isn't easy :(

getNetwork <- function(db = '../gtfs.db') {
    require('sf')
    c <- dbConnect(SQLite(), db)
    v <- dbGetQuery(c, 'SELECT distinct route_id FROM routes') %>%
        (function(x) sapply(strsplit(x$route_id, '_v'), function(z) z[2])) %>%
        as.numeric %>% max
    ##q <- dbSendQuery(c, 'SELECT * FROM shapes WHERE shapes_id IN (SELECT shape_id FROM routes WHERE route_type=3) AND shape_id LIKE ? ORDER BY shape_id, seq')
    q <- dbSendQuery(c, 'SELECT * FROM shapes WHERE shape_id LIKE ? AND shape_id IN (SELECT shape_id FROM routes WHERE route_short_name IN ("274", "277", "223", "221", "110")) ORDER BY shape_id, seq')
    dbBind(q, paste0('%_v', v))
    r <- dbFetch(q)
    dbClearResult(q)
    dbDisconnect(c)

    ## Convert each shape to a polyline
    slines <-
        tapply(1:nrow(r),
               r$shape_id,
               function(i) {
                   r %>%
                       slice(i) %>%
                       select(lng, lat) %>%
                       as.matrix %>%
                       st_linestring
               })
    shapes <-
        data.frame(shape_id = names(slines),
                   geom = do.call(st_sfc, slines)) %>%
        st_as_sf(crs = 4326)
    rm(slines, r)

    ## Buffer each polyline
    shapes.nz <- st_transform(shapes, 27200) ## 27200 is NZ grid
    shapes.nz.buf <- st_buffer(shapes.nz, 20)
    
    ## s1 <- shapes.nz.buf$geometry[7] %>% st_sfc
    ## s2 <- shapes.nz.buf$geometry[8] %>% st_sfc

    ## plot(s1)
    ## plot(s2, add = TRUE)
    ## st_intersection(s1, s2) %>% plot(add=T)
    ## st_difference(s1, s2) %>% plot
    
    ## plot(st_intersection(shapes.nz.buf$geometry[1], shapes.nz.buf$geometry[2]))

    ## st_join(shapes.nz.buf[1,], shapes.nz.buf[12,]) %>% plot

    
    ## nw <- shapes.nz.buf %>% st_union %>% st_simplify

    ## d <- tempdir()
    ## st_write(nw, file.path(d, 'network.shp'))
    ## system(sprintf('create_centerlines %s nw.geojson', file.path(d, 'network.shp')))
    
    
    plot(nw)

    plot(shapes.nz.buf)
    plot(shapes.nz, add = TRUE)
}












#### Do modeling of 'dv'
#dv <- dv[-(1:13), ]
ggplot(dv, aes(position_longitude, position_latitude)) +
    geom_point() +
    coord_fixed(1.2)

with(dv <- dv %>% filter(!is.na(position_latitude)) %>%
     mutate(.t = as.POSIXct(timestamp, origin = '1970-01-01'),
            second = format(.t, '%S') %>% as.numeric,
            minute = format(.t, '%M') %>% as.numeric + second / 60,
            time = format(.t, '%H') %>% as.numeric + minute / 60),
     plot3d(position_longitude, position_latitude, time,
            aspect = c(1, 1.2, 5)))#, type = 'l', lwd = 3))

trips <- unique(dv$trip_id)
tripids <- gsub('-.*$', '', trips)

con <- dbConnect(SQLite(), '../gtfs.db')
q <- dbSendQuery(con, 'SELECT t.trip_id, t.route_id, r.shape_id FROM trips AS t, routes AS r WHERE trip_id LIKE ? AND t.route_id=r.route_id LIMIT 1')
trips <- do.call(rbind, lapply(tripids, function(tid) {
    dbBind(q, list(paste0(tid, '%')))
    r <- dbFetch(q)
    r
}))
dbClearResult(q)


q <- dbSendQuery(con, 'SELECT * FROM shapes WHERE shape_id IN (?) ORDER BY shape_id, seq')
dbBind(q, list(unique(trips$shape_id)))
shapes <- dbFetch(q)
dbClearResult(q)
dbDisconnect(con)


ggplot(dv, aes(position_longitude, position_latitude)) +
    geom_path(aes(lng, lat, group = shape_id), data = shapes,
              lwd = 2, col = 'orangered') +
    geom_point(shape = 4) +
    coord_fixed(1.2)



### --- A likelihood function for f(y|x)
## x: the state (including the distance into trip)
## y: the observed lat/lng position
library(lwgeom)

h <- function(x, shape) {
    shape <- shape %>%
        mutate(dist = c(0, distGeo(cbind(lng, lat))) %>% cumsum)

    if (x[1] >= max(shape$dist)) {
        z <- shape[nrow(shape), c("lng", "lat")]
    } else if (x[1] == 0) {
        z <- shape[1, c("lng", "lat")]
    } else {
        i <- which(shape$dist > x[1])[1]
        p1 <- shapes[i-1, c("lng", "lat")]
        p2 <- shapes[i, c("lng", "lat")]
        d <- x[1] - shapes$dist[i-1]
        if (d < 1e-16) {
            z <- shape[i, c("lng", "lat")]
        } else {
            z <- destPoint(p1, bearing(p1, p2),
                           d)
        }
    }
    as.numeric(z)
}
lhood <- function(y, x, shape, sigma.gps = 20, log = TRUE) {
    Y <-
        st_multipoint(y) %>%
        st_sfc(crs = 4326) %>%
        st_transform(27200) %>%
        st_coordinates
    Y <- Y[, 1:2]

    Z <- h(x, shape) %>% rbind %>% st_point %>% st_sfc(crs = 4326) %>%
        st_transform(27200) %>% st_coordinates %>% as.numeric

    ## 2. get likelihood of Y given z
    mvtnorm::dmvnorm(Y, Z, diag(2) * sigma.gps, log = log)
}

shape <- shapes %>% filter(shape_id == shapes$shape_id[1])

lh <- lhood(cbind(dv$position_longitude, dv$position_latitude),
            30, shapes %>% filter(shape_id == shapes$shape_id[1]),
            sigma = 20)
plot3d(dv$position_longitude, dv$position_latitude, lh)

## --- test
## X <- dv[1, c('position_longitude', 'position_latitude')] %>% as.matrix %>% t
## Y <- t(sapply(1:360, function(th) destPoint(t(X), th, 20)))
## Sigma <- 20 * diag(2)
## fy <- apply(Y, 1, function(y)
##     1 / (2 * pi * sqrt(det(Sigma))) *
##     exp(-0.5 * t(cbind(y) - X) %*% solve(Sigma) %*% (cbind(y) - X)))
## all(mvtnorm::dmvnorm(Y, X, 20 * diag(2)) - fy < 1e-16)



con <- dbConnect(SQLite(), '../gtfs.db')
q <- dbSendQuery(con, 'SELECT stop_sequence, shape_dist_traveled, arrival_time FROM stop_times WHERE trip_id=? ORDER BY stop_sequence')
dbBind(q, list(dv$trip_id[1]))
stops <- dbFetch(q)
dbClearResult(q)
dbDisconnect(con)



############# NEW
library(splines)
Dmax <- max(shape$dist)

d <- data.frame(x = seq(0, Dmax, length.out = 1001))
B <- bs(d$x, df = 50, intercept = TRUE)

alpha <- runif(ncol(B), 0, 30)
beta <- matrix(cumsum(alpha), ncol = 1)
yhat <- B %*% beta
plot(x, yhat, type = "l", xlab = "Distance", ylab = "Time")

fit <- lm(yhat ~ bs(x, df = 50, intercept = TRUE) - 1, data = d)
target <- function(d, t, fit) {
    (t - predict(fit, newdata = data.frame(x = d)))^2
}

t0 <- 200
d0 <- optimize(target, range(d$x), t = t0, fit = fit)$minimum

plot(x, yhat, type = "l", xlab = "Distance", ylab = "Time", xaxs = "i", yaxs = "i")
#segments(c(0, d0), t0, d0, c(t0, 0), lty = 3)


trip <- dv$trip_id[1]
start <- as.POSIXct(paste(date, stops$arrival_time[1]))
dxv <- dv %>%
    filter(trip_id == trip) %>%
    filter(timestamp >= as.numeric(as.POSIXct(paste(date, stops$arrival_time[1]))))
data <- list(
    Sd = stops$shape_dist_traveled,
    start = start,
    M = nrow(stops),
    N = nrow(dxv),
    t = dxv %>% pluck("timestamp") - as.numeric(start),
    y = dxv %>% 
        select(position_longitude, position_latitude) %>%
        as.matrix,
    shape = shape,
    bd = data.frame(x = seq(0, max(shape$dist), length.out = 1001)),
    df = 10
)
data$B <- bs(data$bd$x, df = data$df, intercept = TRUE)

params <- function(data) {
    d <- list(
        ## probability of stopping
        pi = rbeta(data$M-1, 0.5, 0.5),
        ## dwell time at stops
        tau = rexp(data$M-1, 1 / 10),
        ## "speed" along the route
        alpha = runif(ncol(data$B), 5, 1e6)
    )
    d$p <- rbinom(data$M-1, 1, d$pi)
    d
}

get.dist <- function(data, state, plot = FALSE) {
    ## target function
    beta <- matrix(cumsum(state$alpha), ncol = 1)
    yhat <- data$B %*% beta
    fit <- lm(yhat ~ bs(x, df = data$df, intercept = TRUE) - 1, data = data$bd)
    
    ## find the X value for each time obs
    target <- function(d, t, fit) {
        ## we know d ... so just sum up dwell times
        sj <- which(data$Sd < d)
        if (length(sj) > 0) {
            dt <- sum(state$p[sj] * (6 + state$tau[sj]))
        } else {
            dt <- 0
        }
        if (max(sj) < data$M && d == data$Sd[max(sj)+1]) {
            k <- max(sj) + 1
            ta <- predict(fit, newdata = data.frame(x = d))
            td <- ta + sum(state$p[k] * (6 + state$tau[k]))
            if (t + dt < ta) return((t + dt - ta)^2)
            if (t + dt > td) return((t + dt - td)^2)
            return(0)
        } else {
            return((t - predict(fit, newdata = data.frame(x = d)) - dt)^2)
        }
    }
    x <- numeric(data$N)
    if (plot) plotstate(state, data)
    for (i in seq_along(1:data$N)) {
        x[i] <- optimize(target, range(data$bd$x), t = data$t[i], fit = fit)$minimum
        if (plot)
            segments(data$t[i], c(0, x[i]), c(data$t[i], 0), x[i], lty = 3)
    }
    x
}

plotstate <- function(state, data) {
    dev.hold()
    op <- par(mfrow = c(2, 1))
    xx <- sort(c(unique(c(seq(0, max(data$Sd), length.out = 1001), data$Sd)),
                 data$Sd))
    beta <- matrix(cumsum(state$alpha), ncol = 1)
    yhat <- data$B %*% beta
    fit <- lm(yhat ~ bs(x, df = data$df, intercept = TRUE) - 1, data = data$bd)
    yhat <- predict(fit, newdata = data.frame(x = xx))
    Y <- yhat
    for (i in 1:(data$M - 1)) {
        wi <- which(xx == data$Sd[i])[2]
        yhat[wi:length(yhat)] <- yhat[wi:length(yhat)] +
            state$p[i] * (6 + state$tau[i])
    }
    plot(yhat, xx, type = "l", xlab = "Time", ylab = "Distance",
         xaxs = "i", yaxs = "i")
    lines(Y, xx, col = "gray")
    points(data$t, rep(0, length(data$t)))
    ##plot(xx, yhat, type = "l", ylab = "Time", xlab = "Distance",
    ##     xaxs = "i", yaxs = "i")
    ##lines(xx, Y, col = "gray")

    x <- get.dist(data, state)
    y <- t(sapply(x, h, shape = data$shape))
    plot(data$shape %>% select(lng, lat) %>% as.matrix,
         type = "l", lwd = 2, asp = 1.3)
    points(y, cex = 0.5, pch = 19)
    #lh <- sapply(seq_along(1:nrow(data$y)),
    #             function(i) lhood(data$y[i,,drop=F], x[i], data$shape, log = F,
    #                               sigma.gps = 20))
    points(data$y, pch = 4, col = "#aa0000", cex = 0.5)
    arrows(y[, 1], y[, 2], data$y[, 1], data$y[, 2], col = '#99000060',
           code = 2, length = 0.05)
    dev.flush()
    par(op)
}


nllh <- function(state, data, how.many = nrow(data$y)) {
    x <- get.dist(data, state)
    sum(sapply(seq_along(1:how.many),
               function(i)
                   lhood(data$y[i,,drop=F], x, data$shape, sigma.gps = 20)))
}


state <- params(data)
STATE <- matrix(NA, 100, sum(sapply(state, length)) + 1,
                dimnames =
                    list(NULL,
                         c(do.call(c, lapply(names(state),
                                             function(x)
                                                 paste0(x, 1:length(state[[x]])))),
                           "nllh")))
STATE[1, ] <- c(do.call(c, state), nllh(state, data))
plotstate(state, data)

for (i in 1:nrow(STATE)) {
    cat("Iteration", i, "of", nrow(STATE), "\r")
    ## propose a new value for each parameter
    lh <- nllh(state, data, how.many = i %/% 5 + 1)
    ## Update slopes
    pb <- txtProgressBar(0, length(state$alpha), style = 3)
    pbi <- 0
    for (j in 1:length(state$alpha)) {
        prop <- state
        prop$alpha[j] <- truncnorm::rtruncnorm(1, 0, 30, state$alpha[j], 3)
        lstar <- nllh(prop, data, how.many = i %/% 5 + 1)
        alpha <- exp(min(0, lstar - lh))
        if (rbinom(1, 1, alpha) == 1) {
            state <- prop
            lh <- lstar
            plotstate(state, data)
        }
        pbi <- pbi+1
        setTxtProgressBar(pb, pbi)
    }
    ## ## Update dwell times
    ## for (j in 1:length(state$tau)) {
    ##     prop <- state
    ##     prop$tau[j] <- truncnorm::rtruncnorm(1, 0, Inf, state$tau[j], 3)
    ##     lstar <- nllh(prop, data)#, how.many = i %/% 5 + 1)
    ##     alpha <- exp(min(0, lstar - lh))
    ##     if (rbinom(1, 1, alpha) == 1) {
    ##         state <- prop
    ##         lh <- lstar
    ##         plotstate(state, data)
    ##     }
    ## }
    ## ## Update stopping probabilities
    ## for (j in 1:length(state$p)) {
    ##     prop <- state
    ##     prop$pi[j] <- truncnorm::rtruncnorm(1, 0, 1, prop$pi[j], 0.1)
    ##     lstar <- nllh(prop, data, how.many)# = i %/% 5 + 1)
    ##     alpha <- exp(min(0, lstar - lh))
    ##     if (rbinom(1, 1, alpha) == 1) {
    ##         state <- prop
    ##         lh <- lstar
    ##         plotstate(state, data)
    ##     }
    ## }
    STATE[i, ] <- c(do.call(c, state), lh)
}














