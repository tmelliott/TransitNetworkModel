library(tidyverse)
library(magrittr)
library(ggmap)
library(geosphere)
library(viridis)
library(RSQLite)

### --- Step 1: Load the data
date <- "2017-04-04"

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
vid <- names(sort(table(data$vehicle_id), TRUE))[7]
dv <- data %>%
    filter(vehicle_id == vid) %>%
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
getsegments <- function() {
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
segments <- getsegments()

pseg <- ggplot(segments, aes(x = lng, y = lat, group = id)) +
    geom_path() +
    coord_fixed(1.2)
pseg


### --- Step 6: model vehicle trajectories to estimate speed
ds <- data %>%
    filter(!is.na(position_latitude)) %>%
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
aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

p <- ggmap(aklmap) +
    geom_path(aes(x = lng, y = lat, group = id), data = segments) +
    geom_point(aes(x = position_longitude, y = position_latitude,
                   colour = speed / 1000 * 60 * 60),
               data = ds, size = 0.2) +
    scale_color_gradientn(colours = c("#990000", viridis(6)[5:6])) +
    labs(colour = "Speed (km/h)")
##p

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

i <- i + 1; print(i)
s <- segs.geoms %>% filter(id == i)
ggplot(s) +
    geom_path(aes(lng, lat, group = id), data = segments) +
    geom_point(aes(position_longitude, position_latitude),
               data = ds) +
    geom_polygon(aes(lng, lat, group = id), fill = "red", alpha = 0.5) +
    xlim(min(s$lng) - 0.01, max(s$lng) + 0.01) +
    ylim(min(s$lat) - 0.005, max(s$lat) + 0.005) +
    coord_fixed(1.2)

bbox <- with(s, c(min(lng, na.rm = TRUE) - 0.01,
                  min(lat, na.rm = TRUE) - 0.005,
                  max(lng, na.rm = TRUE) + 0.01,
                  max(lat, na.rm = TRUE) + 0.005))
aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

ggmap(aklmap) +
    geom_point(aes(position_longitude, position_latitude),
               data = ds) +
    geom_polygon(aes(lng, lat, group = id), data = s,
                 lwd = 1, color = 'red',
                 fill = "red", alpha = 0.2)


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
dx$inseg <- sapply(st_intersects(vpos, POLY), function(x) length(x) > 0)
plot(vpos, cex = 0.5, pch = 19,
     col = ifelse(inseg, 'blue', 'black'))
plot(POLY, fill='red', border='red', add=T)

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
                              speed / 1000 * 60 * 60)) +
    geom_point() + geom_smooth() + #facet_wrap(~route_id) +
    xlab("Time") + ylab("Approx. speed (km/h)") + ylim(0, 100),
    ncol = 1, heights = c(2, 1)
    )


### Smooth the value at each point ...
## library(mgcv)
## 

## ds <- ds %>%
##     filter(!is.na(speed)) %>%
##     mutate(x = R * (rad(position_longitude) - rad(mean(ds$position_longitude))) *
##                cos(rad(mean(ds$position_latitude))),
##            y = R * (rad(position_latitude) - rad(mean(ds$position_latitude))))

## smth <- function(x, y, z, span = 20) {
##     dist(cbind(x, y)) %>% as.matrix %>%
##         apply(1, function(d) mean(z[d < span], na.rm = TRUE)) %>%
##         as.numeric
## }

## dsf <- ds %>% filter(as.numeric(hour) <= 7) %>% group_by(hour) %>%
##     do(mutate(., speed.smooth = smth(.$x, .$y, .$speed))) %>%
##     ungroup

## speed.fit <- loess(speed ~ x + y,
##                    data = ds1000 %>%
##                       ),
##                    na.action = na.exclude, span = 0.1, degree = 1)
## ds1000$speed.smooth <- predict(speed.fit)

## bbox <- with(dsf, c(min(position_longitude, na.rm = TRUE),
##                        min(position_latitude, na.rm = TRUE),
##                        max(position_longitude, na.rm = TRUE),
##                        max(position_latitude, na.rm = TRUE)))
## aklmap2 <- get_map(bbox, source = "stamen", maptype = "toner-lite")

## p <- ggmap(aklmap2) +
##     #geom_path(aes(x = lng, y = lat, group = id), data = segments) +
##     geom_point(aes(x = position_longitude, y = position_latitude,
##                    colour = speed / 1000 * 60 * 60),
##                data = dsf, size = 1) +
##     scale_color_gradientn(colours = c("#990000", viridis(6)[5:6]),
##                           limits = c(0, 100)) +
##     labs(colour = "Speed (km/h)") +
##     facet_wrap(~hour)
## ps <- ggmap(aklmap2) +
##     #geom_path(aes(x = lng, y = lat, group = id), data = segments) +
##     geom_point(aes(x = position_longitude, y = position_latitude,
##                    colour = speed.smooth / 1000 * 60 * 60),
##                data = dsf, size = 1) +
##     scale_color_gradientn(colours = c("#990000", viridis(6)[5:6]),
##                           limits = c(0, 100)) +
##     labs(colour = "Speed (km/h)") +
##     facet_wrap(~hour)

## gridExtra::grid.arrange(p, ps, nrow = 1)


## for (t in seq(min(ds$timestamp) + 60 * 60 * 2,
##               max(ds$timestamp), by = 30)) {
##     dev.flush(dev.flush())
##     pt <- ggmap(aklmap) +
##         geom_point(aes(x = position_longitude, y = position_latitude,
##                        color = speed),
##                    data = ds %>% filter(timestamp > t - 15 & timestamp < t + 15)) +
##         ggtitle(as.POSIXct(t, origin = "1970-01-01")) +
##         scale_color_viridis(limits = c(0, 30))
##         ##scale_color_gradientn(colours = c("red", "green4", "yellow"), limits = c(0, 30))
##     dev.hold()
##     print(pt)
##     dev.flush()
## }
