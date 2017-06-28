library(ggmap)
library(RSQLite)
con = dbConnect(SQLite(), "../gtfs.db")


drawStuff <- function(segment = 17, int=FALSE) {
    dir = file.path ("..", "build", "tmp", paste0("segment_", segment))
    f1 = read.csv (file.path (dir, "original.csv"))
    f2 = read.csv (file.path (dir, "intersections.csv"))
    f3 = read.csv (file.path (dir, "split_points.csv"))
    f4 = read.csv (file.path (dir, "segment_shape.csv"))
    
    xr = extendrange(f1$lng)
    yr = extendrange(f1$lat)
    bbox = c(xr[1], yr[1], xr[2], yr[2])
    ##akl = get_map(colMeans(f1[, c("lng", "lat")]), zoom = 14, source = "google", maptype = "roadmap")
    akl = get_stamenmap(bbox = bbox, zoom = 14,  maptype = "toner-lite")

    if (int) {
        for (i in 1:length(unique(f4$segment_id))) {
            id = unique(f4$segment_id)[i]
            zi = f4[f4$segment_id==id, ]
            dev.hold()
            print(ggmap(akl) +
                  geom_path(aes(x = lng, y = lat), data = f1, color = "black", lwd = 3) +
                                        # geom_point(aes(x = lng, y = lat), data = f2, color = "red", pch = 19) +
                  geom_path(aes(x = lng, y = lat), color = "blue",
                                        #colour = as.numeric(as.factor(segment_id)) %% 2 == 1),
                            data = zi, show.legend = FALSE, lwd = 2) +
                  geom_point(aes(x = lng, y = lat), data = f3, color = "magenta", pch = 4, lwd = 2, cex = 2))
            dev.flush()
            grid::grid.locator ()
        }
    }
    
    dev.hold()
    print(ggmap(akl) +
        geom_path(aes(x = lng, y = lat), data = f1, color = "black", lwd = 3) +
        geom_point(aes(x = lng, y = lat), data = f2, color = "red", pch = 19) +
        geom_path(aes(x = lng, y = lat, group = segment_id,
                      colour = as.numeric(as.factor(segment_id)) %% 2 == 1),
                  data = f4, show.legend = FALSE, lwd = 2) +
        geom_point(aes(x = lng, y = lat), data = f3, color = "magenta", pch = 4, lwd = 2, cex = 2))
    dev.flush()
    
}

drawStuff(43)

shp = dbGetQuery(con, "SELECT * FROM segment_pt WHERE segment_id=9")
ints = dbGetQuery(con, sprintf("SELECT * FROM intersections WHERE intersection_id IN (%s)", paste(x$id, collapse=",")))
with(shp, plot(lng, lat, type="l",asp=1.2,col = "#009900", lwd=2))
with(ints, points(lng, lat, pch = 1, cex = 1, col = "#999999"))

with(x, points(lng, lat, col = "#990000", pch = 19, cex = 0.3))

with(x, points(lng, lat, col = "#990000", pch = 1:9))
#with(ints, text(lng, lat, intersection_id))
