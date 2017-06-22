library(RSQLite)
con = dbConnect(SQLite(), "../gtfs.db")
f <- textConnection("id,lat,lng
1074,-36.932940,174.912570
588,-36.931670,174.911010
469,-36.928920,174.912380
190,-36.929070,174.914120
182,-36.929540,174.915910
184,-36.935100,174.914790
183,-36.936750,174.914330
470,-36.942910,174.912600
635,-36.961170,174.925720
529,-36.967170,174.924960
1062,-36.950090,174.966100
1101,-36.945430,174.963920
1376,-36.889830,175.012790
1378,-36.889120,175.009950
1374,-36.887570,175.003250
1375,-36.881010,175.005550
1374,-36.887570,175.003250
1378,-36.889120,175.009950
1376,-36.889830,175.012790
1382,-36.880860,175.042110
")

x = read.csv(f)
shp = dbGetQuery(con, "SELECT * FROM segment_pt WHERE segment_id=10")
ints = dbGetQuery(con, sprintf("SELECT * FROM intersections WHERE intersection_id IN (%s)", paste(x$id, collapse=",")))
with(shp, plot(lng, lat, type="l",asp=1.2,col = "#009900", lwd=2))
with(ints, points(lng, lat, pch = 1, cex = 1, col = "#999999"))

with(x, points(lng, lat, col = "#990000", pch = 1:4))
with(ints, text(lng, lat, intersection_id))
