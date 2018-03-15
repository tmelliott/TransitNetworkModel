suppressPackageStartupMessages({
    library(tidyverse)
    library(parallel)
    library(pbapply)
    library(RSQLite)
    library(sf)
})

getsegments <- function(whole.shapes = FALSE) {
    if (whole.shapes) {
        con <- dbConnect(SQLite(), "../gtfs.db")
        segments <- dbGetQuery(
            con,
            "SELECT shape_id AS id, lat, lng FROM shapes ORDER BY shape_id, seq")
        dbDisconnect(con)
        return(segments)
    }
    ## check if they exist yet ...
    if (file.exists("segments.rda")) {
        load("segments.rda")
    } else {
        con <- dbConnect(SQLite(), "../gtfs.db")
        segments <- dbGetQuery(con, "SELECT segment_id FROM segments") %>%
            pluck("segment_id") %>% lapply(function(x) list(id = x, shape = NULL))
        pb <- txtProgressBar(0, length(segments), style = 3)
        for (i in i:length(segments)) {
            setTxtProgressBar(pb, i)
            x <- segments[[i]]$id
            ## get a shape that uses this segment
            q <- dbSendQuery(con, "SELECT shape_id, leg FROM shape_segments
                                   WHERE segment_id=? LIMIT 1")
            dbBind(q, x)
            shp <- dbFetch(q)
            dbClearResult(q)
            if (nrow(shp) == 0) next
            ## get the start/end distances for that shape
            q <- dbSendQuery(con, "SELECT shape_dist_traveled FROM shape_segments
                                   WHERE shape_id=? AND LEG BETWEEN ? AND ?")
            dbBind(q, list(shp$shape_id, shp$leg, shp$leg + 1))
            dr <- dbFetch(q)$shape_dist_traveled
            dbClearResult(q)
            if (length(dr) == 1) dr <- c(dr, Inf)
            ## get the shape points for the shape in the required distance range
            q <- dbSendQuery(con, "SELECT lat, lng FROM shapes
                                   WHERE shape_id=? AND
                                         dist_traveled BETWEEN ? AND ?
                                   ORDER BY seq")
            dbBind(q, list(shp$shape_id, dr[1], dr[2]))
            segments[[i]]$shape <- dbFetch(q)
            dbClearResult(q)
        }
        close(pb)
        dbDisconnect(con)
        segments <-
            segments[sapply(segments, function(x) !is.null(x$shape))] %>%
            lapply(function(x)
                data.frame(id = rep(x$id, nrow(x$shape)), x$shape)) %>%
            do.call(rbind, .) %>%
            mutate(id = as.factor(id))
        save(segments, file = "segments.rda")
    }
    
    segments %>% as.tibble
}

segments <- getsegments(FALSE)

cl <- makeCluster(3L)
clusterExport(cl, "segments")
invisible(clusterEvalQ(cl, {
    library(sf)
    library(tidyverse)
    library(RSQLite)
}))
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

db <- "history.db"
con <- dbConnect(SQLite(), db)
data <- dbGetQuery(con, "SELECT vehicle_id, timestamp, route_id, position_latitude AS lat, position_longitude AS lng, segment_id FROM vps")
pts <- do.call(st_sfc,
               pblapply(1:nrow(data), function(i) {
                   data[i, c("lng", "lat")] %>% as.numeric %>% st_point
               }, cl = cl))
st_crs(pts) <- 4326

clusterExport(cl, c("segs.geoms", "data", "pts"))
segids <-
    pblapply(names(segs.geoms), function(sid) {
        con <- dbConnect(SQLite(), "../gtfs.db")
        q <- dbSendQuery(con, 'SELECT DISTINCT shape_segments.shape_id, routes.route_id FROM shape_segments, routes WHERE shape_segments.shape_id=routes.shape_id AND segment_id=?')
        dbBind(q, list(sid))
        segroutes <- dbFetch(q)$route_id
        dbClearResult(q)

        inroute <- which(data$route_id %in% segroutes)
        bbox <- st_bbox(segs.geoms[[sid]]) %>% st_as_sfc(crs = 4326)
        inbox <- inroute[sapply(st_intersects(pts[inroute], bbox), length) > 0]
        
        inbox[sapply(st_intersects(pts[inbox], segs.geoms[[sid]]), length) > 0]
    }, cl = cl)
stopCluster(cl)
names(segids) <- names(segs.geoms)

invisible(pblapply(names(segs.geoms), function(sid) {
    if (length(segids[[sid]]) > 0) {
        data$segment_id[segids[[sid]]] <<- sid
    }
}))

ggplot(data, aes(lng, lat, color = segment_id)) +
    geom_point() +
    theme(legend.position = 'none')
