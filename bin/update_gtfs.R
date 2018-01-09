# Load any necessary libraries ...
library(readr)
library(jsonlite)
library(RCurl)

# Check for updates to the GTFS data ...
cat('Checking for GTFS updates ...')
current.versions = read_csv('data/versions.csv', col_types = 'cDD')
latest.versions = fromJSON(
    getURL('https://api.at.govt.nz/v2/gtfs/versions',
           httpheader = c(`Ocp-Apim-Subscription-Key` = Sys.getenv('AT_API_KEY')))
)$response

# If updates available ...
new.versions = latest.versions[!latest.versions$version %in% current.versions$version, ]
if (nrow(new.versions) > 0) {
    cat(' updates available:', paste(new.versions$version, collapse = ", "), "\n")
    
    library(RSQLite)
    con = dbConnect(SQLite(), 'gtfs.db')
    
    ## 1. clean database (remove trips/routes/stops/stop_times/shapes)
    rs = dbSendQuery(con, 'DELETE FROM trips')
    dbClearResult(rs)
    rs = dbSendQuery(con, 'DELETE FROM routes')
    dbClearResult(rs)
    rs = dbSendQuery(con, 'DELETE FROM stops')
    dbClearResult(rs)
    rs = dbSendQuery(con, 'DELETE FROM stop_times')
    dbClearResult(rs)
    rs = dbSendQuery(con, 'DELETE FROM shapes')
    dbClearResult(rs)
    rs = dbSendQuery(con, 'DELETE FROM shape_segments')
    dbClearResult(rs)

    ## 2. download new static GTFS data and save archived version


    ## 3. load new data into database ... 

    dbDisconnect(con)
} else {
    cat(' up to date.\n')
}

# Done!