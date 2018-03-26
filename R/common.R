as.time <- function(x) {
    y <- as.POSIXct(x, origin = "1970-01-01") %>%
        format("%Y-%m-%d") %>%
        as.POSIXct %>% as.numeric
    (x - y) / 60 / 60
}


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


plotPlane <- function(x, fit, seg = 1, B,
                      which = c('median', 'mean', 'max', 'sample')) {
    sid <- levels(x$segment_id %>% as.factor)[seg]
    x <- x %>% filter(segment_id == sid)
    xdist <- seq(min(x$dist), max(x$dist), length.out = 101)
    xtime <- seq(min(x$time), max(x$time), length.out = 101)

    which <- match.arg(which)
    sims <- switch(which,
                   "median" = jfit$BUGSoutput$median,
                   "mean" = jfit$BUGSoutput$mean,
                   "max" = {

                   },
                   "sample" = {
                       i <- sample(fit$n.iter, 1)
                       lapply(fit$BUGSoutput$sims.list,
                              function(z) {
                                  if (length(dim(z)) == 3)
                                      z[i,,]
                                  else
                                      z[i, ]
                              })
                   })
    Bx <- bs(xdist, knots = attr(B[[seg]], "knots"))
    pred <- outer(1:length(xdist), xtime, function(j, t) {
        p <- sapply(t, function(ti)
            1 - sum(sims$r[,seg] * sims$alpha *
                    exp(-(ti - sims$tau)^2 / (2 * sims$omega^2))))
        p * (sims$intercept[seg] + Bx[j, ] %*% cbind(sims$beta[,seg]))
    })
    with(x %>% filter(!weekend),
         plot3d(dist, time, speed, aspect = c(3, 5, 1)))
    surface3d(xdist, xtime, pred, grid=FALSE, color = "#99000020")
}
