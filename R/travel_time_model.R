library(tidyverse)
library(magrittr)
library(ggmap)
library(geosphere)
library(viridis)

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
           stop_sequence, stop_id, arrival_time, arrival_delay, departure_time, departure_delay)

data <- full_join(v1, t1) %>%
    arrange(timestamp, vehicle_id) %>%
    mutate(delay = ifelse(is.na(arrival_delay), departure_delay, arrival_delay)) %>%
    mutate(delaycat = cut(delay/60, c(-Inf, -30, -10, -5, 5, 10, 30, Inf)))

### --- Step 4: Do stuff with the data!
vid <- names(sort(table(data$vehicle_id), TRUE))[1]
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



### --- Step 5: model vehicle trajectories to estimate speed
ds <- data %>%
    filter(!is.na(position_latitude)) %>%
    arrange(vehicle_id, timestamp) %>%
    mutate(delta_t = c(0, diff(timestamp)),
           delta_d = c(0, distHaversine(cbind(position_longitude[-n()], position_latitude[-n()]),
                                        cbind(position_longitude[-1], position_latitude[-1])))) %>%
    mutate(speed = pmin(30, delta_d / delta_t)) %>%
    mutate(speed = ifelse(speed > 0, speed, NA),
           hour = format(as.POSIXct(timestamp, origin = "1970-01-01"), "%H"))

bbox <- with(ds, c(min(position_longitude, na.rm = TRUE),
                   min(position_latitude, na.rm = TRUE),
                   max(position_longitude, na.rm = TRUE),
                   max(position_latitude, na.rm = TRUE)))
aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

p <- ggmap(aklmap) +
    geom_point(aes(x = position_longitude, y = position_latitude,
                   colour = speed),
               data = ds) +
    scale_color_viridis()
p

p + facet_wrap(~hour, nrow = 4)


for (t in seq(min(ds$timestamp) + 60 * 60 * 2,
              max(ds$timestamp), by = 30)) {
    dev.flush(dev.flush())
    pt <- ggmap(aklmap) +
        geom_point(aes(x = position_longitude, y = position_latitude,
                       color = speed),
                   data = ds %>% filter(timestamp > t - 15 & timestamp < t + 15)) +
        ggtitle(as.POSIXct(t, origin = "1970-01-01")) +
        scale_color_viridis(limits = c(0, 30))
        ##scale_color_gradientn(colours = c("red", "green4", "yellow"), limits = c(0, 30))
    dev.hold()
    print(pt)
    dev.flush()
}
