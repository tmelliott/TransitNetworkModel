library(tidyverse)
library(RSQLite)
library(parallel)
library(pbapply)
library(geosphere)

## Combine multiple histories into one
dates <- seq(as.Date("2018-02-10"), as.Date("2018-02-17"), by = 1)
db <- "history.db"
dir <- "historicaldata"

cat(" * loading the raw data\n")
cl <- makeCluster(3L)
clusterExport(cl, c("dir"))
invisible(clusterEvalQ(cl, library(tidyverse)))
data <-
    pblapply(dates, function(d) {
        ## file names
        f <- paste0(dir, "/", c("vehicle_positions", "trip_updates"),
                    "_", d, ".csv")
        ## read the vehicle positions and trip updates
        vps <-
            read.csv(f[1],
                     colClasses = c("factor", "factor", "factor",
                                    "character","numeric", "numeric",
                                    "numeric", "integer")) %>%
            group_by(vehicle_id, timestamp) %>%
            filter(row_number() == 1) %>%
            ungroup() %>%
            arrange(timestamp)
        vps
    }, cl = cl)
stopCluster(cl)
data <- do.call(rbind, data)

## Clean out stops/intersections:
cat(" * removing observations at stops and intersections\n")
cx <- paste(data$position_latitude, data$position_longitude,
            sep = ":") %>% table
repPts <- names(cx)[cx > 1]
data <- data %>%
    filter(!is.na(position_latitude)) %>%
    filter(!paste(position_latitude, position_longitude,
                  sep = ":") %in% repPts)

## Compute simple distance ('as the crow flies')
cat(" * calculating approximate speed\n")
data <- data %>%
    group_by(vehicle_id) %>%
    do((.) %>% 
       arrange(timestamp) %>%
       (function(dat) {
           nr <- nrow(dat)
           if (nr <= 1) {
               dH <- numeric()
           } else {
               dH <- geosphere::distHaversine(
                   cbind(dat$position_longitude[-nr],
                         dat$position_latitude[-nr]),
                   cbind(dat$position_longitude[-1],
                         dat$position_latitude[-1])
               )
           }
           dat %>% mutate(delta_t = c(0, diff(timestamp)),
                          delta_d = c(0, dH))
       }) %>%
       mutate(speed = pmin(30, delta_d / delta_t)) %>%
       mutate(speed = ifelse(speed > 0, speed, NA),
              hour = format(as.POSIXct(timestamp, origin = "1970-01-01"), "%H"))
       ) %>%
    ungroup

cat(" * writing to database\n")
con <- dbConnect(SQLite(), db)
dbWriteTable(con, "vps", data, append = TRUE)
dbDisconnect(con)


