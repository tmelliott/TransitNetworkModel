library(tidyverse)

f <- "build/segment_state.csv"
data <-
    read.csv(f, colClasses = c('factor', 'integer', 'numeric',
                               'numeric', 'numeric')) %>%
    mutate(timestamp = as.POSIXct(timestamp, origin = "1970-01-01")) %>%
    filter(length > 0) %>%
    filter(segment_id %in%
           levels(segment_id)[tapply(
                     seq_along(var), segment_id,
                     function(i)
                         !all(var[i] == var[i[1]] &
                              travel_time[i] == travel_time[i[1]])
                 )]) %>%
    mutate(segment_id = factor(segment_id)) %>%
#    filter(segment_id %in% levels(segment_id)[1:5]) %>%
    mutate(speed = length / travel_time) %>%
    mutate(speed.var = var * (-length / speed^2)^2)

data %>%
    ggplot(aes(x = timestamp, y = speed)) +
    geom_ribbon(aes(ymin = pmax(0, qnorm(0.05, speed, sqrt(speed.var))),
                    ymax = pmin(50, qnorm(0.95, speed, sqrt(speed.var)))),
                bg = "red", alpha=0.1) +
    geom_ribbon(aes(ymin = pmax(0, qnorm(0.25, speed, sqrt(speed.var))),
                    ymax = pmin(50, qnorm(0.75, speed, sqrt(speed.var)))),
                bg = "red", alpha=0.4) +
    geom_hline(yintercept = 10, col = "red", lty = 3) +
    geom_line() +
    ylim(c(0, 50)) +
    facet_wrap(~segment_id) +
    theme(strip.text = element_text(size = 5)) 

data %>%
    ggplot(aes(x = timestamp, y = travel_time)) +
    geom_ribbon(aes(ymin = pmax(0, qnorm(0.05, travel_time, sqrt(var))),
                    ymax = qnorm(0.95, travel_time, sqrt(var))),
                bg = "red", alpha=0.1) +
    geom_ribbon(aes(ymin = pmax(0, qnorm(0.25, travel_time, sqrt(var))),
                    ymax = qnorm(0.75, travel_time, sqrt(var))),
                bg = "red", alpha=0.4) +
#    geom_hline(yintercept = 10, col = "red", lty = 3) +
    geom_line() +
    facet_wrap(~segment_id, scales = 'free_y') +
    theme(strip.text = element_text(size = 5)) 
