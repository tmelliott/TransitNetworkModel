library(tidyverse)
library(magrittr)
library(ggmap)

### --- Step 1: Load the data
date <- "2018-01-01"

dir <- "historicaldata"
files <- paste0(dir, "/", c("vehicle_positions", "trip_updates"),
                "_", date, ".csv")

#vps <- read.csv(files[1])
#tus <- read.csv(files[2])

### --- Step 2: Filter duplicate rows
vps <- read.csv(files[1]) %>%
    group_by(vehicle_id, timestamp) %>%
    filter(row_number() == 1) %>%
    ungroup() %>%
    arrange(timestamp)
tus <- read.csv(files[2]) %>%
    group_by(vehicle_id, timestamp) %>%
    filter(row_number() == 1) %>%
    ungroup() %>%
    arrange(timestamp)

### --- Step 3: Merge into a single file

v1 <- vps %>%
    select(vehicle_id, timestamp, trip_id, route_id, trip_start_time, position_latitude, position_longitude, position_bearing)
t1 <- tus %>%
    select(vehicle_id, timestamp, stop_sequence, stop_id, arrival_time, arrival_delay, departure_time, departure_delay)

data <- full_join(v1, t1) %>%
    arrange(timestamp, vehicle_id) %>%
    mutate(delay = ifelse(is.na(arrival_delay), departure_delay, arrival_delay)) %>%
    mutate(delaycat = cut(delay/60, c(-Inf, -30, -10, -5, 5, 10, 30, Inf)))

### --- Step 4: Do stuff with the data!
bbox <- c(min(data$position_longitude, na.rm = TRUE),
          min(data$position_latitude, na.rm = TRUE),
          max(data$position_longitude, na.rm = TRUE),
          max(data$position_latitude, na.rm = TRUE))
aklmap <- get_map(bbox, source = "stamen", maptype = "toner-lite")

ggmap(aklmap) +
    geom_point(aes(x = position_longitude, y = position_latitude,
                   colour = delaycat),
               data = data %>% filter(!is.na(delay)))


### --- Step 5: model vehicle trajectories to estimate speed

