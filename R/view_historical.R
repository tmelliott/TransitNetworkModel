library(tidyverse)
library(RSQLite)
library(dbplyr)
library(viridis)
library(lubridate)
library(hms)
library(mgcv)
library(ggmap)
library(rgl)

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

ii <- 1
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
    filter(format(as.POSIXct(timestamp, origin = "1970-01-01", tz="Pacific/Auckland"),
                  "%Y-%m-%d") %in% c("2018-04-04", "2018-04-05")) %>%
    mutate(tx = as.POSIXct(timestamp, origin = "1970-01-01", tz="Pacific/Auckland"),
           th = format(tx, "%H") %>% as.numeric,
           tm = format(tx, "%M") %>% as.numeric,
           tt = th + tm/60,
           peak.effect =
               dnorm(tt, 7.5, 1) + dnorm(tt, 17, 1) + dnorm(tt, 15.5, 1))

sids <- data %>%
    filter(!is.na(segment_id)) %>%
    group_by(segment_id) %>%
    summarize(n = n(), nadj = n / length(unique(trip_id)),
              dmax = max(seg_dist), vmax = max(speed)) %>%
    # filter(vmax > 90 & n > 60) %>% 
    arrange(desc(n)) %>% head(50) %>%
    pluck('segment_id')
ggplot(data %>% filter(segment_id %in% sids),
       aes(seg_dist, speed / 1000 * 60 * 60)) +
    geom_point(aes(colour = peak.effect)) + 
    geom_smooth() +
    ylim(0, 100) +
    scale_colour_viridis() +
    facet_wrap(~segment_id, scales="free") +
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




## Segments to view: 3285, 3437, 1214, 3432, 3433, 2847
# for (segid in data$segment_id %>% unique) {
	segid <- "2765"
	d1 <- data %>% filter(segment_id == segid & speed < 19)
	px <- ggplot(d1, aes(tt, seg_dist, colour = speed)) + 
	    geom_point() +
	    scale_colour_viridis() +
	    ggtitle(segid)
    print(px)
# 	grid::grid.locator()
# }

### WRITE THE DATA
set.seed(2018)
write.csv(d1 %>% 
	mutate(id = 1:n(),
		   lngr = lng + rnorm(n(),0,0.0005),
           latr = lat + rnorm(n(),0,0.0005)) %>% 
	select(id, tt, seg_dist, speed, lat, lng, latr, lngr) %>%
	arrange(desc(speed)),
	"../../talks/2018-07_useR/data/segspeed.csv",
	row.names=FALSE, quote=FALSE)

d1$dmax <- max(d1$seg_dist)
g <- gam(speed ~ s(tt, I(seg_dist / dmax * 12)), data = d1)
xt <- seq(min(d1$tt), max(d1$tt), length = 41)[1:40]
xd <- seq(min(d1$seg_dist), max(d1$seg_dist), length = 21)[1:20]
xdf <- expand.grid(tt = xt, seg_dist = xd, dmax = max(d1$seg_dist))
xs <- predict(g, newdata = xdf, se.fit = TRUE)
##contour(xt, xd, matrix(xs, nrow = length(xt)), nlevels = 10)
plot(g)
vis.gam(g, se=FALSE, plot.type="3d", zlim = c(0, 25),
	xlab = "Time", ylab = "Distance Traveled", zlab = "Speed")

## spd <- round(xs$fit/1000*60*60)
## spd <- spd - min(spd)
## plot3d(xdf$time, xdf$seg_dist, xs$fit,
##        col = viridis(max(spd))[spd])
## plot3d(d1$time, d1$seg_dist, d1$speed, add = TRUE)
xdf <- xdf %>% add_column(speed = xs$fit, se = xs$se.fit)

### Write it ...
write.csv(xdf %>% select(tt, seg_dist, speed) %>%
	mutate(id = 1:n()),
	"../../talks/2018-07_useR/data/fitspeed.csv",
	quote = FALSE, row.names = FALSE)

p <- ggplot(xdf, aes(tt, seg_dist/1000)) +
    xlab("Time") + ylab("Distance (km)") + 
    coord_cartesian(expand = FALSE) +
    scale_x_continuous(breaks = c(8, 12, 16, 20),
                       labels = paste0(c(8, 12, 16, 20), "h00"))

p1 <- p + geom_point(aes(colour = speed/1000*60*60)) +
    labs(colour = "Speed (km/h)") +
    scale_colour_viridis(option="D")
p2 <- p + geom_point(aes(colour = se)) + labs(colour = "Std. err") +
    scale_colour_viridis(option = "C", direction = -1)

p1

gridExtra::grid.arrange(p1, p2, nrow = 2)
    
## simulate from the posterior
gV <- vcov(g, freq = FALSE)
Rbeta <- MASS::mvrnorm(100, coef(g), gV)
Xp <- predict(g, newdata = xdf, type = "lpmatrix")
xsim <- apply(Rbeta, 1, function(rb) Xp %*% cbind(rb))

plots <- vector("list", 100)
for (i in 1:100) {
    dev.flush(dev.flush())
	plots[[i]] <- ggplot(xdf %>% add_column(speed = xsim[,i]),
	            aes(hms(time), seg_dist/1000)) +
	    xlab("Time") + ylab("Distance (km)") + 
	    coord_cartesian(expand = FALSE) +
	    theme(legend.position = "bottom") +
	    geom_point(aes(colour = speed/1000*60*60)) +
	    labs(colour = "Speed (km/h)") +
	    scale_colour_viridis(option="D", limits = range(xsim) / 1000 * 60 * 60)
    dev.hold()
    print(plots[[i]])
    dev.flush()
	#ggsave(sprintf("segment_fits/sim_%03d.png", i), plots[[i]], device = "png")
}

plot3d(d1$time, d1$seg_dist, d1$speed, zlim = range(xsim), asp = c(5, 2, 1))
for (i in sample(1:100, 10)) {
    surface3d(xt, xd, matrix(xsim[,i], nrow = length(xt)),
              col = "blue", alpha = 0.2)
}


###### Plots for useR talk
dseg <- data %>%
    filter(route_id%in% !!(d1 %>% pluck('route_id') %>% unique)) %>%
    mutate(inseg = segment_id == segid) %>%
    filter(speed / 1000 * 60 * 60 < 80)

## Plot 1: raw points
akl <- get_googlemap(c(mean(range(d1$lng)), mean(range(d1$lat))), zoom = 12,
                     size = c(640, 640), maptype = "roadmap")

ggmap(akl) +
    geom_point(aes(lng, lat), data = dseg)

ggmap(akl) +
    geom_point(aes(lng, lat, colour = inseg), data = dseg) +
    scale_colour_manual(values = c('gray', 'black')) +
    theme(legend.position = "none")

ggmap(akl) +
    geom_path(aes(lng, lat, colour = route_id, group = interaction(trip_date, trip_id)), data = dseg) +
    theme(legend.position = "none")

akl2 <- get_googlemap(c(mean(range(d1$lng)), mean(range(d1$lat))), zoom = 13,
                     size = c(640, 320), maptype = "satellite")

ggmap(akl2) +
    geom_path(aes(lng, lat, colour = route_id, group = interaction(trip_date, trip_id)),
              data = dseg %>% filter(segment_id == segid)) +
    theme(legend.position = 'none')

ggmap(akl2) +
    geom_path(aes(lng, lat, colour = speed/1000*60*60, group = trip_id),
              data = dseg %>% filter(segment_id == segid)) +
    scale_colour_viridis()

set.seed(2018)
ggmap(akl2) +
    geom_point(aes(lng, lat, colour = speed/1000*60*60),
              data = dseg %>% filter(segment_id == segid) %>%
              mutate(lng = lng + rnorm(n(),0,0.0005),
              	     lat = lat + rnorm(n(),0,0.0005)) %>%
              arrange(desc(speed))) +
    scale_colour_viridis()

## ggmap(akl2)

ggplot(dseg %>% filter(segment_id == segid) %>%
       ##filter(between(tt, 14, 20)),
       filter(between(tt, 0, 24)),
       aes(tt, seg_dist / 1000, colour = speed / 1000 * 60 * 60,
           group = trip_id)) +
    geom_path(lwd = 2) +
    scale_colour_viridis() +
    xlab('Time') + ylab('Distance (km)') + labs(colour = "Speed (km/h)") +
    scale_x_continuous(breaks = c(8, 12, 16, 20),
                       labels = paste0(c(8, 12, 16, 20), "h00"))

p1

p1 + geom_path(aes(tt,
                  seg_dist / 1000,
                  #colour = speed / 1000 * 60 * 60,
                  group = trip_id),
               data = dseg %>% filter(segment_id == segid),
               colour = "white", lty = 2) +
	xlim(15, 18)
    ## scale_x_continuous(breaks = c(14, 16, 18, 20),
    ##                    labels = paste0(c(14, 16, 18, 20), "h00"),
    ##                    limits = c(14, 20))


## predict the future
final = as.tibble(xdf) %>% 
	filter(tt %in% xt[c(12, 29)]) 

write.csv(final %>% filter(tt < 15) %>% select(seg_dist, speed) %>% mutate(id = 1:n()), 
	'../../talks/2018-07_useR/data/predspeeds_10.csv',
	quote = FALSE, row.names = FALSE)
write.csv(final %>% filter(tt > 15) %>% select(seg_dist, speed) %>% mutate(id = 1:n()), 
	'../../talks/2018-07_useR/data/predspeeds_17.csv',
	quote = FALSE, row.names = FALSE)

ggplot(final, aes(seg_dist / 1000, speed / 1000 * 60 * 60, 
		color = factor(tt, labels = c("10am", "5:30pm")))) +
		geom_path() +
		labs(color = "") +
		xlab("Distance (km)") + ylab("Speed (km/h)") +
		theme(legend.position = "bottom")

ggsave("../../talks/2018-07_useR/assets/img/pred.png")
