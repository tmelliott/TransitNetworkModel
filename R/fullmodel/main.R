library(RProtoBuf)
library(RSQLite)

## 0. settings etc.
DATE <- "2017-10-04"
HOME <- path.expand("~/Documents/uni/TransitNetworkModel")
PI.IP <- "tom@130.216.51.230"
PI.DIR <- "/mnt/storage/historical_data"


## 1. read protofiles from a day into a database; return connection
readProtoFiles(dir = file.path(HOME, "proto"))
pbToCSV <- function(pb) {
    ## converts protobuf GTFS file into a dataframe
    feed <- pb$entity
    do.call(rbind, lapply(feed, function(x) {
        data.frame(vehicle_id = as.character(x$vehicle$vehicle$id),
                   route_id = x$vehicle$trip$route_id,
                   trip_id = x$vehicle$trip$trip_id,
                   lat = x$vehicle$position$latitude,
                   lng = x$vehicle$position$longitude,
                   timestamp = x$vehicle$timestamp,
                   stringsAsFactors = FALSE)
    }))
}
makeData <- function(date, ip, dir, db = "busdata.db") {
    ## get a list of files for the chosen day
    datef <- gsub("-", "", date)
    cmd <- sprintf("ssh %s find %s -type f -name 'vehicle_locations_%s*.pb'", ip, dir, datef)
    cat(" - Generating list of files ... ")
    files <- system(cmd, intern = TRUE)
    files <- sort(files)
    cat(sprintf("done (%s)", length(files)))

    ## create the database
    cat("\n - Creating database table")
    con <- dbConnect(SQLite(), db)
    if ("vehicle_positions" %in% dbListTables(con)) dbRemoveTable(con, "vehicle_positions")
    dbSendQuery(con, paste(sep="\n",
                          "CREATE TABLE vehicle_positions (",
                          " vehicle_id TEXT,",
                          " route_id TEXT,",
                          " trip_id TEXT,",
                          " lat REAL,",
                          " lng REAL,",
                          " timestamp INTEGER",
                          ")"))
    
    ## read the files one-by-one into a database
    cat("\n - Writing files to database ...\n")
    pbar <- txtProgressBar(0, length(files), style = 3)
    for (file in files) {
        setTxtProgressBar(pbar, which(files == file))
        file <- "/mnt/storage/historical_data/vehicle_locations_20171004054931.pb"
        ip <- PI.IP
        d <- read(transit_realtime.FeedMessage,
                  pipe(sprintf("ssh %s cat %s", ip, file)))
        dbWriteTable(con, "vehicle_positions", pbToCSV(d), append = TRUE)
    }
    close(pbar)

    ## remove duplicates
    cat("\n - Removing duplicates ... ")
#    dbSendQuery(con, "DELETE FROM vehicle_positions WHERE rowid NOT IN (SELECT MIN(rowid) FROM vehicle_positions GROUP BY vehicle_id, timestamp)");
    cat("done")
    
    ## finish up
    dbDisconnect(con)
    cat(sprintf("\n\nFinished. Data written to %s\n", db))
    invisible(NULL)
}

#makeData(DATE, PI.IP, PI.DIR)


## 2. connect to database and, for a given route, fetch GTFS information
getRouteData <- function(route, db, gtfs.db) {
    con <- dbConnect(SQLite(), gtfs.db)
    q <- dbSendQuery(con, "SELECT route_id FROM routes WHERE route_short_name=?")
    dbBind(q, list(route))
    rid <- dbFetch(q)$route_id
    dbClearResult(q)
    dbDisconnect(con)

    print(rid)
    
    con <- dbConnect(SQLite(), db)
    dat <- dbGetQuery(
        con,
        sprintf(
            "SELECT * FROM vehicle_positions WHERE route_id IN ('%s') ORDER BY timestamp, vehicle_id",
            paste(rid, collapse = "','")
        ))
    dbDisconnect(con)

    return(dat)
}

dd <- getRouteData("881", "busdata.db", file.path(HOME, "gtfs.db"))
head(dd); dim(dd)
plot(dd$timestamp)

## 3. implement a STAN model for n = 0, ..., N observations (estimating segment travel times!)

