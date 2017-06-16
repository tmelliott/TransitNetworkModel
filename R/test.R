library(RSQLite)
con = dbConnect(SQLite(), "../gtfs.db")
shp = dbGetQuery(con, "SELECT * FROM segment_pt WHERE segment_id=6")
x <- read.csv("../R/test.csv", head=T)
with(shp, plot(lng, lat, type="l",asp=1.2,col = "#009900", lwd=2))
with(x, points(lng1, lat1, asp=1.2,col=ifelse(keep, "#990000", "#cccccc")))
with(x, arrows(lng1, lat1, lng2, lat2, length = 0.05,
	           col=ifelse(keep, "#990000", "#cccccc")))
