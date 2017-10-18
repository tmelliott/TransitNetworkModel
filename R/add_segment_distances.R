## Add segment distances to the `segments` table using Google's Maps API

library(jsonlite)
library(RSQLite)

calculateDistance <- function(a, b, apikey) {
	## using Google Maps API, calcualte distance from a to b
	url <- "https://maps.googleapis.com/maps/api/distancematrix/json?origins=%s&destinations=%s&key=%s"
	url <- sprintf(url, paste(a, collapse = ","), paste(b, collapse = ","), apikey)
	dist <- fromJSON(url)
	dist$distance = dist$rows$elements[[1]]$distance$value
	dist
}


calculateDistances <- function(key) {
	con <- dbConnect(SQLite(), "gtfs.db")

	## get all INT -> INT
	cat("\n * Int -> Int:\n")
	qry <- paste0("SELECT segment_id, from_id, to_id, ifrom.lat AS lat1, ifrom.lng AS lng1, ito.lat AS lat2, ito.lng AS lng2 ",
		   "  FROM segments ",
		   "       INNER JOIN intersections AS ifrom on from_id=ifrom.intersection_id ",
		   "       INNER JOIN intersections AS ito on to_id=ito.intersection_id ",
	   	   "  WHERE length IS NULL AND from_id IS NOT NULL AND to_id IS NOT NULL")
	segs <- dbGetQuery(con, qry)
	pb <- txtProgressBar(0, nrow(segs), style = 3)
	for (i in 1:nrow(segs)) {
		setTxtProgressBar(pb, i)
		dr <- calculateDistance(segs[i, c("lat1", "lng1")], segs[i, c("lat2", "lng2")], key)
		if (dr$status != "OK") {
			cat("\n")
			print(dr$status)
			continue()
		}
		pq <- dbSendQuery(con,
			sprintf("UPDATE segments SET length=%s WHERE segment_id=%s", dr$distance, segs$segment_id[i]))
		dbClearResult(pq)
		Sys.sleep(1) ## don't overwhelm Google
	}
	close(pb)

	## get all INT -> STOP
	cat("\n * Int -> Stop:\n")
	qry <- paste0("SELECT segment_id, from_id, end_at, ifrom.lat AS lat1, ifrom.lng AS lng1, ito.lat AS lat2, ito.lng AS lng2 ",
		   "  FROM segments ",
		   "       INNER JOIN intersections AS ifrom on from_id=ifrom.intersection_id ",
		   "       INNER JOIN stops AS ito on end_at=ito.stop_id ",
	   	   "  WHERE length IS NULL AND from_id IS NOT NULL AND end_at IS NOT NULL")
	segs <- dbGetQuery(con, qry)
	pb <- txtProgressBar(0, nrow(segs), style = 3)
	for (i in 1:nrow(segs)) {
		setTxtProgressBar(pb, i)
		dr <- calculateDistance(segs[i, c("lat1", "lng1")], segs[i, c("lat2", "lng2")], key)
		if (dr$status != "OK") {
			cat("\n")
			print(dr$status)
			continue()
		}
		pq <- dbSendQuery(con,
			sprintf("UPDATE segments SET length=%s WHERE segment_id=%s", dr$distance, segs$segment_id[i]))
		dbClearResult(pq)
		Sys.sleep(1) ## don't overwhelm Google
	}
	close(pb)


	## get all STOP -> INT
	cat("\n * Stop -> Int:\n")
	qry <- paste0("SELECT segment_id, start_at, to_id, ifrom.lat AS lat1, ifrom.lng AS lng1, ito.lat AS lat2, ito.lng AS lng2 ",
		   "  FROM segments ",
		   "       INNER JOIN stops AS ifrom on start_at=ifrom.stop_id ",
		   "       INNER JOIN intersections AS ito on to_id=ito.intersection_id ",
	   	   "  WHERE length IS NULL AND start_at IS NOT NULL AND to_id IS NOT NULL")
	segs <- dbGetQuery(con, qry)
	pb <- txtProgressBar(0, nrow(segs), style = 3)
	for (i in 1:nrow(segs)) {
		setTxtProgressBar(pb, i)
		dr <- calculateDistance(segs[i, c("lat1", "lng1")], segs[i, c("lat2", "lng2")], key)
		if (dr$status != "OK") {
			cat("\n")
			print(dr$status)
			continue()
		}
		pq <- dbSendQuery(con,
			sprintf("UPDATE segments SET length=%s WHERE segment_id=%s", dr$distance, segs$segment_id[i]))
		dbClearResult(pq)
		Sys.sleep(1) ## don't overwhelm Google
	}
	close(pb)

	## STOP -> STOP
	cat("\n * Stop -> Stop:\n")
	qry <- paste0("SELECT segment_id, start_at, end_at, ifrom.lat AS lat1, ifrom.lng AS lng1, ito.lat AS lat2, ito.lng AS lng2 ",
		   "  FROM segments ",
		   "       INNER JOIN stops AS ifrom on start_at=ifrom.stop_id ",
		   "       INNER JOIN stops AS ito on end_at=ito.stop_id ",
	   	   "  WHERE length IS NULL AND start_at IS NOT NULL AND end_at IS NOT NULL")
	segs <- dbGetQuery(con, qry)
	pb <- txtProgressBar(0, nrow(segs), style = 3)
	for (i in 1:nrow(segs)) {
		dr <- calculateDistance(segs[i, c("lat1", "lng1")], segs[i, c("lat2", "lng2")], key)
		if (dr$status != "OK") {
			cat("\n")
			print(dr$status)
			continue()
		}
		pq <- dbSendQuery(con,
			sprintf("UPDATE segments SET length=%s WHERE segment_id=%s", dr$distance, segs$segment_id[i]))
		dbClearResult(pq)
		setTxtProgressBar(pb, i)
		Sys.sleep(1) ## don't overwhelm Google
	}
	close(pb)

	dbDisconnect(con)

	cat("Finished!\n")
}


## test it
key <- "AIzaSyDnNVWsMExhGuxsXOTqTcr67_5OU-lpDR8"
test <- calculateDistance(c(-36.7965332, 175.0320143), c(-36.7819468, 175.008956), key)
if (test$status == "OK") cat("API is working...\n")

calculateDistances(key)
