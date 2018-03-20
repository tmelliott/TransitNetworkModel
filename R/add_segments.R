suppressPackageStartupMessages({
    library(tidyverse)
    library(parallel)
    library(pbapply)
    library(RSQLite)
    library(sf)
})

pboptions(type = "timer")
options("mc.cores" = detectCores() - 1)

source("common.R")

cat(" * loading segments\n")
segments <- getsegments(FALSE)

cat(" * setting up cluster\n")
cl <- makeCluster(getOption("mc.cores"))
clusterExport(cl, "segments")
invisible(clusterEvalQ(cl, {
    library(sf)
    library(tidyverse)
    library(RSQLite)
}))

cat(" * reading in segments\n")
segs.geoms <-
    pblapply(levels(segments$id), function(id) {
        segi <- segments[segments$id == id, ]
        if (nrow(segi) <= 1) return(NULL)
        st_linestring(segi %>% select(lng, lat) %>% as.matrix) %>%
            st_sfc(crs = 4326) %>%
            st_transform(27200) %>%
            st_buffer(15) %>%
            st_transform(4326)
    }, cl = cl)
names(segs.geoms) <- levels(segments$id)
segs.geoms <- segs.geoms[!sapply(segs.geoms, is.null)]

cat(" * fetching history\n")
db <- "history.db"
con <- dbConnect(SQLite(), db)
data <- dbReadTable(con, "vps")
cn <- colnames(data)
cn[cn == "position_latitude"] <- "lat"
cn[cn == "position_longitude"] <- "lng"
colnames(data) <- as.character(cn)
clusterExport(cl, "data")

cat(" * creating points\n")
pts <- do.call(st_sfc,
               pblapply(1:nrow(data), function(i) {
                   data[i, c("lng", "lat")] %>% as.numeric %>% st_point
               }, cl = cl))
st_crs(pts) <- 4326

## shapes of the routes
cat(" * loading shapes\n")
con <- dbConnect(SQLite(), "../gtfs.db")
shapes <- dbGetQuery(con, 'SELECT shape_id, lat, lng FROM shapes ORDER BY shape_id, seq')
sr <- dbGetQuery(con, 'SELECT route_id, shape_id FROM routes')
dbDisconnect(con)

bearingAt <- function(x, shape) {
    if (inherits(shape, "sfc")) {
        shape <-shape %>% st_coordinates
        shape <- shape[,1:2]
        colnames(shape) <- c('lng', 'lat')
    }
    
    j <- which.min(apply(shape, 1, function(y) {
        geosphere::distHaversine(x, y)
    }))
    if (j == 1) j <- 2
    bearing <- NA
    k <- j-1
    while(is.na(bearing) && j < k + 10) {
        if (geosphere::distHaversine(shape[k,], shape[j,]) > 1e-15) {
            bearing <- geosphere::bearing(shape[k,], shape[j,])
        } else {
            j <- j + 1
        }
    }
    bearing
}

cat(" * computing segment ids\n")
clusterExport(cl, c("segs.geoms", "data", "pts", "shapes", "sr", "bearingAt"))
segids <-
    pblapply(names(segs.geoms), function(sid) {
        bbox <- st_bbox(segs.geoms[[sid]])
        inbox <- which(data$lng >= bbox[1] & data$lng <= bbox[3] &
                       data$lat >= bbox[2] & data$lat <= bbox[4])
        if (length(inbox) == 0) return(NULL)
        inseg <- suppressMessages(
            inbox[sapply(st_intersects(pts[inbox], segs.geoms[[sid]]), length) > 0])
        if (length(inseg) == 0) return(NULL)
        ## direction!
        con <- dbConnect(SQLite(), "../gtfs.db")
        q <- dbSendQuery(con, 'SELECT shape_id FROM shape_segments WHERE segment_id=?')
        dbBind(q, sid)
        segshapes <- dbFetch(q)$shape_id
        dbClearResult(q)
        dbDisconnect(con)
        shapenot <- character()
        w <- sapply(inseg, function(i) {
            s <- sr %>% filter(route_id == data[i, "route_id"]) %>% pluck("shape_id")
            if (is.null(s) || s %in% shapenot) return(FALSE)
            if (s %in% segshapes) return(TRUE)
            sh <- shapes %>%
                filter(shape_id == s) %>%
                select(lng, lat) %>% as.matrix
            x <- data[i, ] %>% select(lng, lat) %>% as.numeric
            b1 <- bearingAt(x, sh)
            b2 <- bearingAt(x, segs.geoms[[sid]])
            if (b1 - b2 < 90) {
                segshapes <<- c(segshapes, s)
                return(TRUE)
            }
            shapenot <<- c(shapenot, s)
            return(FALSE)
        })
        inseg[w]
    }, cl = cl)
names(segids) <- names(segs.geoms)

cat(" * attaching segment ids\n")
invisible(pblapply(names(segs.geoms), function(sid) {
    if (length(segids[[sid]]) > 0) {
        data$segment_id[segids[[sid]]] <<- sid
    }
}))


cat(" * writing database\n")
con <- dbConnect(SQLite(), db)
dbWriteTable(con, 'vps', data, overwrite = TRUE)
dbDisconnect(con)


cat(" * done!\n")
