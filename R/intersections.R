library(RSQLite)
library(ggmap)
con = dbConnect(SQLite(), "../gtfs-backup2.db")

ints = dbGetQuery(con, "SELECT * FROM intersections")

xr = extendrange(ints$lng)
yr = extendrange(ints$lat)
bbox = c(xr[1], yr[1], xr[2], yr[2])

bbox = c(174.76, -37.01, 174.81, -36.99)

akl = get_stamenmap(bbox, zoom = 15, maptype = "toner-lite")

ggmap(akl) +
    geom_point(aes(lng, lat), data = ints) +
    geom_text(aes(lng, lat, label = intersection_id), data = ints, col = "red", nudge_y=-0.0003)

intid = 708
dq = dbSendQuery(con, "DELETE FROM intersections WHERE intersection_id=?")
dbBind(dq, list(intid))
dbGetRowsAffected(dq)
dbClearResult(dq)
