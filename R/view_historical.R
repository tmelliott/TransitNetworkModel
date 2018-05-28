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

plotRoute <- function(route, which = c('dist', 'speed', 'map')) {
	which <- match.arg(which)
	rdb <- gsub("-.*", "%", route)
	sid <- gtfs %>% tbl('routes') %>% filter(route_id %LIKE% rdb) %>% 
		select(shape_id) %>% head(1) %>% collect %>% pluck('shape_id')
	shape <- gtfs %>% tbl('shapes') %>% filter(shape_id == sid) %>%
		select(lat, lng, dist_traveled) %>%
		collect

	switch(which, 
		"dist" = {	
			trips %>% filter(route_id == route & dist > 0) %>%
				select(trip_id, time, dist) %>%
				collect %>% mutate(time = hms(time)) %>%
				ggplot(aes(time, dist, group = trip_id)) +
					geom_path() +
					geom_hline(yintercept = max(shape$dist_traveled), lty = 2)
		},
		"speed" = {
			trips %>% filter(route_id == route & dist > 0) %>%
				select(trip_id, dist, speed) %>%
				collect %>% 
				ggplot(aes(dist, speed, group = trip_id)) +
					geom_path()
		},
		"map" = {
			trips %>% filter(route_id == route & dist > 0) %>%
				select(trip_id, lat, lng, speed) %>%
				collect %>%
				ggplot(aes(lng, lat, group = trip_id)) +
					geom_path(aes(group = NULL), data = shape, col = "orangered", lwd = 2) +
					geom_path()
		})
}

plotRoute(routes[2], "dist")
plotRoute(routes[2], "speed")
plotRoute(routes[2], "map")
