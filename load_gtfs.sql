.mode csv
.import data/routes.txt routes_tmp
.import data/trips.txt trips_tmp
.import data/shapes.txt shapes_tmp
.import data/stop_times.txt stop_times_tmp
.import data/stops.txt stops_tmp

CREATE TABLE routes (
	route_id TEXT,
	route_short_name TEXT,
	route_long_name TEXT,
	shape_id TEXT
);
INSERT INTO routes
	SELECT routes_tmp.route_id, route_short_name, route_long_name, MIN(shape_id) as shape_id
	  FROM routes_tmp, trips_tmp
	 WHERE trips_tmp.route_id = routes_tmp.route_id
  GROUP BY routes_tmp.route_id;
DROP TABLE routes_tmp;

CREATE TABLE trips (
	trip_id TEXT,
	route_id TEXT
);
INSERT INTO trips
	SELECT trip_id, route_id FROM trips_tmp;
DROP TABLE trips_tmp;

/* This will be filled in used C++ program */
CREATE TABLE shapes (
	shape_id TEXT,
	leg INT DEFAULT 0,
	segment_id BIGINT,
	shape_dist_traveled REAL
);
CREATE TABLE segments (
	segment_id INTEGER PRIMARY KEY AUTOINCREMENT,
	start_id INT,
	end_id INT,
	length REAL
);
CREATE TABLE segment_pt (
	segment_id INT,
	seg_pt_sequence INT,
	lat REAL,
	lng REAL,
	seg_dist_traveled REAL
);
CREATE TABLE intersections (
	intersection_id INTEGER PRIMARY KEY AUTOINCREMENT,
	type TEXT,
	lat REAL,
	lng REAL
);


CREATE TABLE stops (
	stop_id TEXT,
	lat REAL,
	lng REAL
);
INSERT INTO stops
	SELECT stop_id, CAST(stop_lat AS REAL) AS lat, CAST(stop_lon AS REAL) AS lng
	  FROM stops_tmp;
DROP TABLE stops_tmp;

CREATE TABLE stop_times (
	stop_id TEXT,
	trip_id TEXT,
	stop_sequence INT,
	arrival_time TIME,
	departure_time TIME,
	shape_dist_traveled REAL
);
INSERT INTO stop_times (stop_id, trip_id, stop_sequence, arrival_time, departure_time)
	SELECT stop_id, trip_id, CAST(stop_sequence AS INT) AS stop_sequence, arrival_time, departure_time
	  FROM stop_times_tmp;
DROP TABLE stop_times_tmp;



CREATE TABLE particles (
	particle_id    BIGINT,
	vehicle_id     TEXT,
	trip_id        TEXT,
	distance       REAL,
	velocity       REAL,
	timestamp      TIMESTAMP,
	log_likelihood REAL,
	parent_id      BIGINT,
	initialized    BOOLEAN
);
