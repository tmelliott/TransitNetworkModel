## Process historical data
#  convert protobuf files into (combined) CSV files

library(RProtoBuf)
readProtoFiles(dir = '../proto')

date <- "2018-01-01"

processDay <- function(date, quiet = TRUE) {
    date <- as.character(date)
    cat("+++ Processing", date, "\n")
    dir <- file.path("/mnt", "storage", "history", gsub("-", "/", date))
    cmd <- sprintf("ssh tom@130.216.51.230 find %s -type f -name '*.pb'", dir)
	files <- system(cmd, intern = TRUE)
    if (!quiet) cat(" - found", length(files), "files\n")


    vptmp <- sprintf("historicaldata/.vehicle_positions_%s.csv.partial", date)
    vp <- sprintf("historicaldata/vehicle_positions_%s.csv", date)
    if (!file.exists(vp)) {
        unlink(vptmp)
        vpf <- files[grepl("vehicle_locations", files)]
        if (!quiet) cat(" - processing vehicle locations ...\n")
        if (!quiet) pb <- txtProgressBar(0, length(vpf), style = 3)
        for (i in seq_along(vpf)) {
            processVP(vpf[i], vptmp)
            if (!quiet) setTxtProgressBar(pb, i)
        }
        if (!quiet) close(pb)
        file.rename(vptmp, vp)
    } 

    tutmp <- sprintf("historicaldata/.trip_updates_%s.csv.partial", date)
    tu <- sprintf("historicaldata/trip_updates_%s.csv", date)
    if (!file.exists(tu)) {
        unlink(tutmp)
        tuf <- files[grepl("trip_updates", files)]
        if (!quiet) cat(" - processing trip updates ...\n")
        if (!quiet) pb <- txtProgressBar(0, length(tuf), style = 3)
        for (i in seq_along(tuf)) {
            processTU(tuf[i], tutmp)
            if (!quiet) setTxtProgressBar(pb, i)
        }
        if (!quiet) close(pb)
        file.rename(tutmp, tu)
    }

    if (!quiet) cat(" - done!\n\n")
}

processVP <- function(file, out) {
    ## Returns a data.frame
    con <- "tom@130.216.51.230"
    pb <- NULL
    while (is.null(pb)) {
        pb <- try({
            read(transit_realtime.FeedMessage,
                 pipe(sprintf("ssh %s cat %s", con, file)))$entity
        }, quietly = TRUE)
        if (inherits(pb, "try-error")) pb <- NULL
        Sys.sleep(5 * is.null(pb))
    }
    res <- do.call(rbind, lapply(pb, function(vp) {
        data.frame(
            vehicle_id = vp$vehicle$vehicle$id,
            trip_id = vp$vehicle$trip$trip_id,
            route_id = vp$vehicle$trip$route_id,
            trip_start_time = vp$vehicle$trip$start_time,
            position_latitude = vp$vehicle$position$latitude,
            position_longitude = vp$vehicle$position$longitude,
            position_bearing = vp$vehicle$position$bearing,
            timestamp = vp$vehicle$timestamp)
    }))
    write.table(res, out, append = file.exists(out),
                col.names = !file.exists(out),
                quote = FALSE, row.names = FALSE, sep = ",", na = "")
}
processTU <- function(file, out) {
    ## Returns a data.frame
    con <- "tom@130.216.51.230"
    pb <- NULL
    while (is.null(pb)) {
        pb <- try({
            pb <- read(transit_realtime.FeedMessage,
                       pipe(sprintf("ssh %s cat %s", con, file)))$entity
            }, quietly = TRUE)
        if (inherits(pb, "try-error")) pb <- NULL
        Sys.sleep(5 * is.null(pb))
    }
    res <- do.call(rbind, lapply(pb, function(vp) {
        data.frame(
            vehicle_id = vp$trip_update$vehicle$id,
            trip_id = vp$trip_update$trip$trip_id,
            route_id = vp$trip_update$trip$route_id,
            trip_start_time = vp$trip_update$trip$start_time,
            stop_sequence =
                vp$trip_update$stop_time_update[[1]]$stop_sequence,
            stop_id =
                vp$trip_update$stop_time_update[[1]]$stop_id,
            arrival_time =
                ifelse(vp$trip_update$stop_time_update[[1]]$has('arrival'),
                       vp$trip_update$stop_time_update[[1]]$arrival$time,
                       NA),
            arrival_delay =
                ifelse(vp$trip_update$stop_time_update[[1]]$has('arrival'),
                       vp$trip_update$stop_time_update[[1]]$arrival$delay,
                       NA),
            departure_time =
                ifelse(vp$trip_update$stop_time_update[[1]]$has('departure'),
                       vp$trip_update$stop_time_update[[1]]$departure$time,
                       NA),
            departure_delay =
                ifelse(vp$trip_update$stop_time_update[[1]]$has('departure'),
                       vp$trip_update$stop_time_update[[1]]$departure$delay,
                       NA),
            timestamp = vp$trip_update$timestamp)
    }))
    write.table(res, out, append = file.exists(out),
                col.names = !file.exists(out),
                quote = FALSE, row.names = FALSE, sep = ",", na = "")
}

dates <- seq(as.Date("2017-04-01"), as.Date("2017-12-31"), by = 1)
parallel::mclapply(dates, processDay, mc.cores = 3, mc.preschedule = FALSE)
