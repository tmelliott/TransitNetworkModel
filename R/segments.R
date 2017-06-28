library(ggmap)
library(RSQLite)
con = dbConnect(SQLite(), "../gtfs.db")

segs = dbGetQuery(con, "SELECT * FROM segment_pt ORDER BY segment_id, seg_pt_sequence")

xr = extendrange(segs$lng)
yr = extendrange(segs$lat)
bbox = c(xr[1], yr[1], xr[2], yr[2])
akl = get_stamenmap(bbox, zoom = 11, maptype = "toner-lite")

ggmap(akl) +
    geom_path(aes(x = lng, y = lat, group = segment_id),
              data = segs, color = "#00000020")
