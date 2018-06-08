library(tidyverse)
library(RSQLite)
library(dbplyr)
library(viridis)
library(lubridate)
library(hms)

con <- dbConnect(SQLite(), "history_cleaned.db")
gtfs <- dbConnect(SQLite(), "../gtfs.db")
trips <- con %>% tbl('trips')

routes <- trips %>% select('route_id') %>% 
	collect %>% pluck('route_id') %>% unique

getRoute <- function(route) {
    rdb <- gsub("-.*", "%", route)
	sid <- gtfs %>% tbl('routes') %>% filter(route_id %LIKE% rdb) %>% 
		select(shape_id) %>% head(1) %>% collect %>% pluck('shape_id')
    trips %>% filter(route_id == route) %>% collect
}

getStops <- function(trip) {
    tdb <- gsub("-.*", "%", trip)
	tid <- gtfs %>% tbl('trips') %>% filter(trip_id %LIKE% tdb) %>% 
		select(trip_id) %>% head(1) %>% collect %>% pluck('trip_id')
    gtfs %>% tbl('stop_times') %>%
        filter(trip_id == tid) %>%
        inner_join(gtfs %>% tbl('stops')) %>%
        select(stop_id, stop_sequence, shape_dist_traveled, lat, lng) %>%
        arrange(stop_sequence) %>%
        collect
}

getSegments <- function(route) {
    rdb <- gsub("-.*", "%", route)
	sid <- gtfs %>% tbl('routes') %>% filter(route_id %LIKE% rdb) %>% 
		select(shape_id) %>% head(1) %>% collect %>% pluck('shape_id')
    gtfs %>% tbl('shape_segments') %>% filter(shape_id == sid) %>%
        collect
}


cleanSpeeds <- function(d) {
    d <- d %>% mutate(speed = c(diff(dist) / diff(t), 0))
    bk <- which(d$speed < 0)
    rm <- numeric()
    #print(d)
    #print(c(bk, nrow(d)))
    for (i in bk) {
        if (i == 1) {
            rm <- c(rm, i)
        } else if (all(d[i-1, c("lat", "lng")] == d[i+1, c("lat", "lng")])) {
            if (d[i, "speed"] > 30)
                rm <- c(rm, i+1)
            else
                rm <- c(rm, i)
        } else {
            rm <- c(rm, i)
        }
    }
    if (length(rm))
        d <- d %>% slice(-rm) %>%
        mutate(speed = c(diff(dist) / diff(t), 0))

    d %>% filter(speed < 30) %>%
        mutate(speed = c(diff(dist) / diff(t), 0))
}


plotRoute <- function(route, which = c('dist', 'speed', 'map'),
                      zero.time = FALSE, stops = TRUE, segments = TRUE) {
	which <- match.arg(which)
	rdb <- gsub("-.*", "%", route)
	sid <- gtfs %>% tbl('routes') %>% filter(route_id %LIKE% rdb) %>% 
		select(shape_id) %>% head(1) %>% collect %>% pluck('shape_id')
	shape <- gtfs %>% tbl('shapes') %>% filter(shape_id == sid) %>%
		select(lat, lng, dist_traveled) %>%
		collect

    d <- trips %>% filter(route_id == route & dist > 0) %>%
                   select(trip_id, time, t, lat, lng, dist) %>%
                   collect %>%
                   filter(trip_id %in%
                          ((.) %>% group_by(trip_id) %>% 
                           summarise(tmax = max(diff(t)) < 3*60) %>%
                           filter(tmax) %>% pluck('trip_id'))) %>%
                   mutate(time = if (zero.time) t else hms(time)) %>%
                   #group_by(trip_id) %>% 
                   #do(cleanSpeeds(.)) %>% 
                   #ungroup() %>%
                   mutate(speed = c(diff(dist) / diff(t), 0)) %>%
                   filter(speed > 0)
    
	p <- switch(which, 
           "dist" = {
               p <- d %>%
                   ggplot(aes(time, dist, group = trip_id)) +
                   geom_path() +
                   geom_hline(yintercept = max(shape$dist_traveled), lty = 2)
               if (stops) {
                   p <- p +
                       geom_hline(aes(x = NULL, y = NULL, group = NULL,
                                      yintercept = shape_dist_traveled),
                                  data = getStops(d$trip_id[1]),
                                  lwd = 0.5, lty = 3, colour = "orangered")
               }
               if (segments) {
                   p <- p +
                       geom_hline(aes(x = NULL, y = NULL, group = NULL,
                                      yintercept = shape_dist_traveled),
                                  data = getSegments(route),
                                  lwd = 0.5, lty = 3, colour = "green4")
               }
               p
           },
           "speed" = {
               p <- d %>%
                   ggplot(aes(dist + c(diff(dist)/2, 0),
                              speed, group = trip_id)) +
                   geom_point()
               if (stops) {
                   p <- p +
                       geom_vline(aes(x = NULL, y = NULL, group = NULL,
                                      xintercept = shape_dist_traveled),
                                  data = getStops(d$trip_id[1]),
                                  lwd = 0.5, lty = 3, colour = "orangered")
               }
               if (segments) {
                   p <- p +
                       geom_vline(aes(x = NULL, y = NULL, group = NULL,
                                      xintercept = shape_dist_traveled),
                                  data = getSegments(route),
                                  lwd = 0.5, lty = 3, colour = "green4")
               }
               p
           },
           "map" = {
               d %>%
                   ggplot(aes(lng, lat, group = trip_id)) +
                   geom_path(aes(group = NULL), data = shape,
                             col = "orangered", lwd = 2) +
                   geom_path()
           })
    p + ggtitle(route)
    
}

ii <- 5
plotRoute(routes[ii], "dist")
plotRoute(routes[ii], "dist", zero.time = TRUE)
plotRoute(routes[ii], "speed")
plotRoute(routes[ii], "map")


### Clean the distance/speed data
f <- "mycleandata.rds"
if (file.exists(f)) {
    data <- readRDS(f)
} else {
    data <- do.call(bind_rows,
                    pbapply::pblapply(routes, function(route) {
                        rdata <- getRoute(route)
                        ## rstops <- getStops(rdata$trip_id[1])
                        rsegs <- getSegments(rdata$route_id[1])                    
                        rdata <- rdata %>%
                            filter(trip_id %in%
                                   ((.) %>% group_by(trip_id) %>% 
                                    summarise(tmax = max(diff(t)) < 3*60) %>%
                                    filter(tmax) %>% pluck('trip_id'))) %>%
                            group_by(trip_id) %>% 
                            do(cleanSpeeds(.)) %>% 
                            ungroup() %>%
                            filter(speed > 0 & c(diff(t), 100) > 10)
                        segi <- sapply(rdata$dist, function(x) 
                            max(which(rsegs$shape_dist_traveled <= x)))
                        if (length(segi) == 0) return(NULL)
                        rdata %>%
                            mutate(segment_id = rsegs$segment_id[segi],
                                   seg_dist = dist - rsegs$shape_dist_traveled[segi])
                    }))
    saveRDS(data, f)
}

getHours <- function(x) hour(x) + minute(x) / 60
data <- data %>%
    mutate(tx = as.POSIXct(timestamp, origin = "1970-01-01"),
           th = format(tx, "%H") %>% as.numeric,
           tm = format(tx, "%M") %>% as.numeric,
           tt = th + tm/60,
           peak.effect = dnorm(tt, 7.5, 1.5) + dnorm(tt, 17, 2))
sids <- data %>%
    filter(!is.na(segment_id) & trip_date == "2018-02-12" &
           !segment_id %in% c('116', '118')) %>%
    group_by(segment_id) %>%
    summarize(n = n()) %>% arrange(desc(n)) %>% head(49) %>%
    pluck('segment_id')
ggplot(data %>% filter(segment_id %in% sids),
       aes(seg_dist, speed / 1000 * 60 * 60)) +
    geom_point(aes(colour = peak.effect)) + # getHours(time %>% hms))) +
    geom_smooth() +
    ylim(0, 100) +
    scale_colour_viridis() +
    facet_wrap(~segment_id, scales="free") +
    #theme(legend.position = "bottom") +
    labs(colour = "Time") +
    xlab("Distance along road segment (m)") + ylab("Speed (km/h)")

## sh <- gtfs %>% tbl('shape_segments') %>%
##     filter(segment_id == "116") %>%
##     collect
## seg <- gtfs %>% tbl("segments") %>%
##     filter(segment_id == 116) %>% collect
## int <- gtfs %>% tbl("intersections") %>% filter(intersection_id == seg$to_id)
## gtfs %>% tbl("shapes") %>%
##     filter(shape_id %in% sh$shape_id) %>%
##     ggplot(aes(lng, lat, group = shape_id, colour = shape_id, size = shape_id)) +
##     geom_path() +
##     scale_size_discrete(range = c(6, 2, 0.5)) +
##     geom_point(aes(group = NULL, colour = NULL, size = NULL),
##                data = int)


library(mgcv)
library(ggmap)
library(rgl)

d1 <- data %>% filter(segment_id == "3285")
ggplot(d1, aes(hms(time), seg_dist, colour = speed)) + 
    geom_point() +
    scale_colour_viridis()


g <- gam(speed ~ s(peak.effect, seg_dist), data = d1)
xt <- seq(min(d1$time), max(d1$time), length = 201)
xd <- seq(min(d1$seg_dist), max(d1$seg_dist), length = 201)
xdf <- expand.grid(time = xt, seg_dist = xd)
xs <- predict(g, newdata = xdf, se.fit = TRUE)
##contour(xt, xd, matrix(xs, nrow = length(xt)), nlevels = 10)
plot(g)
vis.gam(g, se=TRUE, plot.type="3d")

## spd <- round(xs$fit/1000*60*60)
## spd <- spd - min(spd)
## plot3d(xdf$time, xdf$seg_dist, xs$fit,
##        col = viridis(max(spd))[spd])
## plot3d(d1$time, d1$seg_dist, d1$speed, add = TRUE)

p <- ggplot(xdf %>% add_column(speed = xs$fit, se = xs$se.fit),
            aes(hms(time), seg_dist/1000)) +
    xlab("Time") + ylab("Distance (km)") + 
    coord_cartesian(expand = FALSE) +
    theme(legend.position = "bottom")
gridExtra::grid.arrange(
    p + geom_point(aes(colour = speed/1000*60*60)) +
    labs(colour = "Speed (km/h)") +
    scale_colour_viridis(option="A"),
    #geom_point(aes(colour = speed/1000*60*60), data = d1),
    p + geom_point(aes(colour = se)) + labs(colour = "Std. err") +
    scale_colour_viridis(option = "C", direction = -1),
    nrow = 2
)
    




d <- rdata %>% select(trip_id, lat, lng, time, t, dist)
#    filter(trip_id == (.) %>% pluck('trip_id') %>% head(1))

 %>%
    select(time, t, dist, speed) %>% as.data.frame

    ##    filter(speed >= 0) %>% mutate(speed = c(diff(dist) / diff(t), 0)) %>%
    ##    ungroup() %>%
    ggplot(aes(dist, speed, group = trip_id)) +
    ##ggplot(aes(t, dist, group = trip_id)) +
    geom_path() #+
#geom_hline(aes(x = NULL, y = NULL, group = NULL,
#                   yintercept = shape_dist_traveled),
#               data = getStops(d$trip_id[1]),
#               lwd = 0.5, lty = 3, colour = "orangered")
