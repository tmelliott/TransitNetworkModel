library(tidyverse)
library(RSQLite)
library(dbplyr)
library(viridis)
library(lubridate)
library(hms)

if (exists("con")) dbDisconnect(con)
con <- dbConnect(SQLite(), "rawhistory.db")

if (exists("gtfs")) dbDisconnect(gtfs)
gtfs <- dbConnect(SQLite(), "../gtfs.db")

vps <- tbl(con, 'vps') %>%
    mutate(trip_date = strftime('%Y-%m-%d', datetime(timestamp, 'unixepoch')))

inbbox <- function(x, bbox, R = 6371000) {
    ## x: [[lng, lat], ...] (matrix)
    ## bbox: [lngmin, latmin, lngmax, latmax]
    bbox <- matrix(bbox, nrow = 2, byrow = TRUE)
    center <- colMeans(bbox)
    ## convert to radians
    center <- center * pi / 180
    x <- x * pi / 180
    x[,1] <- (x[,1] - center[1]) * cos(center[2]) * R
    x[,2] <- (x[,2] - center[2]) * R
    bbox <- bbox * pi / 180
    bbox[,1] <- (bbox[,1] - center[1]) * cos(center[2]) * R
    bbox[,2] <- (bbox[,2] - center[2]) * R
    ## within 50m of the bbox
    span <- apply(bbox, 2, max) + 50
    abs(x[,1]) < span[1] & abs(x[,2]) < span[2]
}

if (!file.exists("history_cleaned.db")) {
    tcon <- dbConnect(SQLite(), "history_cleaned.db")
    trips.keep <- vps %>% 
        mutate(lat = position_latitude, lng = position_longitude) %>%
        select(vehicle_id, trip_id, route_id, trip_start_time,
               lat, lng, timestamp, trip_date) %>% head(1) %>% collect %>%
        mutate(trip_start_time = as.hms(trip_start_time),
               time = as.POSIXct(timestamp, origin = "1970-01-01") %>%
                   format("%H:%M:%S") %>% as.hms) %>%
        filter(FALSE)
    dbWriteTable(tcon, "trips_raw", trips.keep)
} else {
    tcon <- dbConnect(SQLite(), "history_cleaned.db")
}

## For each day ...
#dates <- vps %>% select(trip_date) %>% distinct %>% collect %>% pluck("trip_date")
dates <- character()
for (date in dates) {
    cat("\n +++", date, "\n")
    ## For each trip ...
    trips <- vps %>% filter(trip_date == date) %>%
        group_by(trip_id) %>% summarize(count = n()) %>%
        filter(count >= 10) %>% collect %>% pluck("trip_id")
    tii <- 0
    for (trip in trips) {
        tii <- tii + 1
        if (tcon %>% tbl("trips_raw") %>%
            filter(trip_date == date & trip_id == trip) %>%
            collect %>% nrow) next()
        cat(sep = "", "\r + [", tii, " of ", length(trips), "] ", trip)
        ## Find trip's GTFS entry -> start and end times
        tid <- gsub("-.*", "", trip)
        
        tid.gtfs <- try(
            gtfs %>% tbl("trips") %>%
            filter(trip_id %like% paste0(tid, "%")) %>% select(trip_id) %>%
            head(1) %>% collect %>% pluck("trip_id"))
        if (inherits(tid.gtfs, "try-error")) next()
        if (length(tid.gtfs) == 0) next()
        
        trip.times <- try(
            gtfs %>% tbl('stop_times') %>% filter(trip_id == tid.gtfs))
        if (inherits(trip.times, "try-error")) next()
        
        trip.start <- try(
            trip.times %>% arrange(stop_sequence) %>%
            select(departure_time) %>% head(1) %>%
            collect %>% pluck('departure_time') %>% as.hms)
        if (inherits(trip.start, "try-error")) next()
        
        trip.end <- try(
            trip.times %>% arrange(desc(stop_sequence)) %>%
            select(arrival_time) %>% head(1) %>%
            collect %>% pluck('arrival_time') %>% as.hms)
        if (inherits(trip.end, "try-error")) next()
        trip.duration <- as.numeric(trip.end - trip.start)
        
        tdata <-
            vps %>%
            filter(trip_date == date & trip_id == trip) %>%
            mutate(lat = position_latitude, lng = position_longitude) %>%
            select(vehicle_id, trip_id, route_id, trip_start_time,
                   lat, lng, timestamp, trip_date) %>%
            collect %>%
            mutate(trip_start_time = as.hms(trip_start_time),
                   time = as.POSIXct(timestamp, origin = "1970-01-01") %>%
                       format("%H:%M:%S") %>% as.hms)
        
        if (nrow(tdata) < 10 || (!is.na(tdata$trip_start_time[1]) &&
                                 tdata$trip_start_time[1] != trip.start)) {
            #cat("Skipping... [1]")
            next()
        }

        ## temporal deletion
        temporal.window <- c(as.hms(trip.start - hms(minutes = 10)),
                             as.hms(trip.end + hms(minutes = 90)))
        tdata <- tdata %>%
            filter(between(time, temporal.window[1], temporal.window[2]))
        if (nrow(tdata) < 10 || diff(range(tdata$time)) < 10 * 60)  {
            #cat("Skipping... [2]")
            next()
        }

        shape <-
            gtfs %>% tbl('trips') %>%
            inner_join(gtfs %>% tbl('routes'), by = "route_id") %>%
            filter(trip_id == tid.gtfs) %>%
            select(shape_id) %>%
            inner_join(gtfs %>% tbl('shapes'), by = "shape_id") %>%
            select(seq, lng, lat) %>% arrange(seq)
        shape.bbox <- shape %>%
            summarize(lng.min = min(lng, na.rm = TRUE),
                      lat.min = min(lat, na.rm = TRUE),
                      lng.max = max(lng, na.rm = TRUE),
                      lat.max = max(lat, na.rm = TRUE)) %>%
            head(1) %>% collect %>% as.numeric

        ## p <- tdata %>%
        ##     mutate(inbox = inbbox(cbind(lng, lat), shape.bbox)) %>%
        ##     ggplot(aes(lng, lat)) +
        ##     geom_point(aes(color = inbox)) +
        ##     geom_path(data = shape) +
        ##     coord_fixed(1.3)
        ## print(p)

        ## spatial deletion - bbox
        tdata <- tdata %>%
            filter(inbbox(cbind(lng, lat), shape.bbox)) %>%
            arrange(timestamp)
        if (nrow(tdata) < 10)  {
            #cat("Skipping... [3]")
            next()
        }

        ## p <- tdata %>% 
        ##     ggplot(aes(lng, lat)) +
        ##     geom_path(data = shape, lwd = 2) +
        ##     geom_point(colour = 'red') +
        ##     geom_path(colour = 'red') +
        ##     coord_fixed(1.3)
        ## print(p)

        ## spatial deletion - on/near the actual line
        dLast <-
            geosphere::distGeo(tdata %>% select(lng, lat) %>% as.matrix,
                               shape %>% arrange(desc(seq)) %>% select(lng, lat) %>%
                               head(1) %>% collect %>% as.numeric, f = 0)
        if (min(dLast) < 20) 
            tdata <- tdata %>% slice(1:which.min(dLast))
        
        if (nrow(tdata) < 10)  {
            #cat("Skipping... [4]")
            next()
        }

        ## multiple vehicles? grab the one with most points
        vs <- table(tdata$vehicle_id)
        if (length(vs) > 1) {
            vid <- names(vs)[which.max(vs)]
            tdata <- tdata %>% filter(vehicle_id == vid)
        }
        if (nrow(tdata) < 10) {
            #cat("Skipping... [5]")
            next()
        }

        dFirst <-
            geosphere::distGeo(tdata %>% select(lng, lat) %>% as.matrix,
                               shape %>% select(lng, lat) %>%
                               head(1) %>% collect %>% as.numeric, f = 0)
        if (min(dFirst) < 20)
            tdata <- tdata %>%
            slice(tail(which(dFirst == dFirst[which.min(dFirst)]), 1):n())
        if (nrow(tdata) < 10)  {
            #cat("Skipping... [6]")
            next()
        }
        
        ## p <- tdata %>%
        ##     ggplot(aes(lng, lat)) +
        ##     geom_path(data = shape, lwd = 2) +
        ##     geom_point(colour = 'red') +
        ##     geom_path(colour = 'red') +
        ##     coord_fixed(1.3)
        ## print(p)

        dbWriteTable(tcon, "trips_raw", tdata, append = TRUE)
    }
}



h <- function(d, shape) {
    o <- do.call(rbind, lapply(d, function(x) {
        S <- shape %>% select(lng, lat) %>% as.matrix
        if (x <= 0) return(S[1,])
        if (x >= max(shape$dist)) return(S[nrow(S),])
        di <- max(which(shape$dist <= x))
        dr <- x - shape$dist[di]
        geosphere::destPoint(S[di, ], geosphere::bearing(S[di,], S[di+1,], f = 0),
                             dr, f = 0) %>% as.matrix
    }))
    colnames(o) <- c('lng', 'lat')
    as.tibble(o) %>% add_column(dist = d)
}


trips.final <- NULL
trips <- tcon %>% tbl("trips_raw") %>% collect %>% pluck('trip_id') %>% unique
tii <- 0
cat("\n *** Processing trips\n")
for (trip in trips) {
    tii <- tii + 1
    cat(sep = "", "\r * [", tii, "/", length(trips), "]  ")
    tdata <- tcon %>% tbl("trips_raw") %>%
        filter(trip_id == trip) %>%
        collect %>%
        mutate(trip_start_time = as.hms(trip_start_time),
               time = as.hms(time))
    vs <- table(tdata$vehicle_id)
    if (length(vs) > 1) {
        vid <- names(vs)[which.max(vs)]
        tdata <- tdata %>% filter(vehicle_id == vid)
    }
    
    tid <- gsub("-.*", "", trip)    
    tid.gtfs <- try(
        gtfs %>% tbl("trips") %>%
        filter(trip_id %like% paste0(tid, "%")) %>% select(trip_id) %>%
        head(1) %>% collect %>% pluck("trip_id"))
    shape <-
        gtfs %>% tbl('trips') %>%
        inner_join(gtfs %>% tbl('routes'), by = 'route_id') %>%
        filter(trip_id == tid.gtfs) %>%
        select(shape_id) %>%
        inner_join(gtfs %>% tbl('shapes'), by = 'shape_id') %>%
        select(seq, lng, lat) %>% arrange(seq) %>% collect
    
    ## ggplot(tdata, aes(lng, lat)) +
    ##     geom_path(data = shape, colour = 'orangered') +
    ##     geom_point(data = shape[1,], size = 3, colour = "orangered") +
    ##     geom_path(lty = 2) +
    ##     geom_point()

    ## Find closest point on line
    shape <- shape %>%
        mutate(dist = cumsum(c(0, cbind(lng, lat) %>% as.matrix %>%
                                  geosphere::distGeo(f = 0))))
    {
        N <- nrow(tdata)
        tx <- (tdata$time - tdata$trip_start_time) %>% as.numeric
        sx <- c(0, geosphere::distGeo(tdata %>% select(lng, lat) %>%
                                      as.matrix, f = 0) / diff(tx))

        for (j in 1:N) {
            d <- Inf
            i <- 0
            while (d > 10 + i/10) {
                .sx <- sx
                .sx[j] <- truncnorm::rtruncnorm(1, 0, 30, .sx[j], 1)
                dx <- cumsum(.sx * c(0, diff(tx)))
                zx <- h(dx, shape)
                
                .d <- geosphere::distGeo(
                    zx[j,] %>% select(lng, lat) %>% as.matrix,
                    tdata[j,] %>% select(lng, lat) %>% as.matrix,
                    f = 0)
                if (.d < d || runif(1) < d / .d) {
                    d <- .d
                    sx <- .sx
                }
                i <- i+1
            }
            ## p <- ggplot(zx[1:j,], aes(lng, lat)) +
            ##     geom_path(data = shape, colour = 'orangered') +
            ##     geom_point(data = shape[1,], size = 3, colour = "orangered") +
            ##     geom_path(lty = 2) +
            ##     geom_point(data = tdata, size = 2, col = 'magenta') +
            ##     geom_point() +
            ##     coord_fixed(1.3)
            ## print(p)
        }      
        
        trips.final <- trips.final %>%
            bind_rows(tdata %>% mutate(dist = dx, speed = sx))   
    }    
}

dbWriteTable(tcon, "trips", trips.final)
cat("\nDone\n")

##save(list=ls(), file="workspace.rda")
##load("workspace.rda")


## for (trip in trips.final %>% pluck("trip_id") %>% unique) {
##     tid <- gsub("-.*", "", trip)    
##     tid.gtfs <- try(
##         gtfs %>% tbl("trips") %>%
##         filter(trip_id %like% paste0(tid, "%")) %>% select(trip_id) %>%
##         head(1) %>% collect %>% pluck("trip_id"))
##     shape <-
##         gtfs %>% tbl('trips') %>%
##         inner_join(gtfs %>% tbl('routes'), by = 'route_id') %>%
##         filter(trip_id == tid.gtfs) %>%
##         select(shape_id) %>%
##         inner_join(gtfs %>% tbl('shapes'), by = 'shape_id') %>%
##         select(seq, lng, lat) %>% arrange(seq) %>% collect
    
##     p1 <- ggplot(trips.final %>% filter(trip_id == trip),
##                 aes(lng, lat)) +
##         geom_path(data = shape, colour = 'orangered') +
##         geom_point(data = shape[1,], size = 3, colour = "orangered") +
##         geom_path(aes(colour = speed / 1000 * 60 * 60), lwd = 2) +
##         #geom_point() +
##         scale_colour_viridis() +
##         coord_fixed(1.3) +
##         labs(colour = "Speed (km/h)")
##     p2 <- ggplot(trips.final %>% filter(trip_id == trip),
##                  aes(as.numeric(time - trip_start_time) / 60, dist/1000)) +
##         #geom_path(lty = 2) +
##         geom_point() +
##         xlab("Time into trip (min)") + ylab("Distance into trip (km)")
##     p3 <- ggplot(trips.final %>% filter(trip_id == trip),
##                  aes(c(0, dist[-1] - diff(dist)/2)/1000,
##                      speed / 1000 * 60 * 60)) +
##         #geom_path(lty = 3) +
##         geom_point() +
##         geom_smooth(se=FALSE) +
##         xlab("Distance into trip (km)") + ylab("Speed (km/h)")
##     gridExtra::grid.arrange(p1, p2, p3, layout_matrix= rbind(c(1, 2), c(1, 3)))
    
##     grid::grid.locator()
## }


## ggplot(trips.final, aes(lng, lat, colour = speed / 1000 * 60 * 60)) +
##     ## geom_point(alpha = 0.5) +
##     geom_path(aes(group = trip_id), alpha = 0.5, lwd = 2, lineend = "round") +
##     coord_fixed(1.3) +
##     scale_colour_viridis() +
##     labs(colour = "Speed (km/h)")



## segments <-
##     gtfs %>% tbl('segments') %>% #, by = 'segment_id') %>%
##     left_join(gtfs %>% tbl('intersections'),
##               by = c('from_id' = 'intersection_id')) %>%
##     left_join(gtfs %>% tbl('intersections'),
##               by = c('to_id' = 'intersection_id'),
##               suffix = c(".from", ".to")) %>%
##     left_join(gtfs %>% tbl('stops'),
##               by = c('start_at' = 'stop_id')) %>%
##     left_join(gtfs %>% tbl('stops'),
##               by = c('end_at' = 'stop_id'),
##               suffix = c('.start', '.end')) %>%
##     mutate(lat.from = case_when(is.na(from_id) ~ lat.start, TRUE ~ lat.from),
##            lng.from = case_when(is.na(from_id) ~ lng.start, TRUE ~ lng.from),
##            lat.to = case_when(is.na(to_id) ~ lat.end, TRUE ~ lat.to),
##            lng.to = case_when(is.na(to_id) ~ lng.end, TRUE ~ lng.to)) %>%
##     select(segment_id, lat.from, lng.from, lat.to, lng.to) %>%
##     collect
    
## segspeeds <- NULL
## ## For each route_id ->
## for (route in trips.final %>% pluck('route_id') %>% unique) {
##     ## get segments ->
##     rid <- gsub("-.*", "", route)
##     rid.gtfs <- try(
##         gtfs %>% tbl("routes") %>%
##         filter(route_id %like% paste0(rid, "%")) %>% select(route_id) %>%
##         head(1) %>% collect %>% pluck("route_id"))

##     shseg <- gtfs %>% tbl("routes") %>%
##         filter(route_id == rid.gtfs) %>%
##         select(shape_id) %>%
##         left_join(gtfs %>% tbl('shape_segments'), by = 'shape_id') %>%
##         mutate(dist = shape_dist_traveled) %>%
##         select(segment_id, dist) %>% arrange(dist) %>% collect

##     ## Get segment_id for each point
##     ss <- trips.final %>% filter(route_id == route) %>%
##         mutate(segment_id = sapply(dist, function(d) {
##             shseg$segment_id[max(which(shseg$dist <= d))]
##         })) %>%
##         select(lat, lng, time, speed, segment_id)
##     segspeeds <- segspeeds %>% bind_rows(ss)
## }

## segs <- segspeeds %>%
##     group_by(segment_id) %>%
##     summarize(speed.mean = mean(speed, na.rm = TRUE),
##               speed.sd = sd(speed, na.rm = TRUE)) %>%
##     left_join(segments, by = 'segment_id')

## ggplot(segs, aes(lng.from, lat.from,
##                      colour = speed.mean / 1000 * 60 * 60)) +
##     geom_segment(aes(xend=lng.to, yend=lat.to, group = segment_id),
##                  lwd = 1.5, lineend = 'butt', alpha = 0.8) +
##     labs(colour = "Speed (km/h)") +
##     scale_colour_viridis() +
##     coord_fixed(1.3)
    


## segment <- segspeeds$segment_id[1]



## ggplot(segspeeds, aes(time, speed / 1000 * 60 * 60)) +
##     geom_point() +
##     facet_wrap(~segment_id) +
##     xlab("Time") + ylab("Speed (km/h)")

## sids <- segspeeds %>%
##     group_by(segment_id) %>%
##     summarize(count = n()) %>% ungroup %>%
##     arrange(desc(count)) %>% pluck('segment_id')


## for (segment in sids) {
##     p <- ggplot(segspeeds %>% filter(segment_id == segment),
##            aes(time, speed / 1000 * 60 * 60)) +
##         geom_point() +
##         xlab("Time") + ylab("Speed (km/h)") +
##         ylim(0, 100)
##     print(p)
##     grid::grid.locator()
## }
