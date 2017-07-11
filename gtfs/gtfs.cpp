#include <string>
#include <vector>
#include <stdexcept>
#include <sqlite3.h>

#include "gtfs.h"
#include "gps.h"

namespace gtfs {
	/**
	 * Default constructor for the main GTFS object.
	 *
	 * This contains the actual values of all the GTFS static data,
	 * so it can be accessed throughout the program.
	 *
	 * @param dbname the name of the database to pull the GTFS data from
	 * @param v      a version string to limit how much data is loaded
	 */
	GTFS::GTFS (std::string& dbname, std::string& v) : database_ (dbname), version_ (v) {

		sqlite3 *db;
		if (sqlite3_open (database_.c_str (), &db)) {
			std::cerr << " * Can't open database: " << sqlite3_errmsg (db) << "\n";
			throw std::runtime_error ("Can't open database.");
		}
		std::clog << "\n * Connected to database \"" << database_ << "\"";

		// --- Load all stops
		sqlite3_stmt* select_stops;
		if (sqlite3_prepare_v2 (db, "SELECT stop_id, lat, lng FROM stops", -1, &select_stops, 0) != SQLITE_OK) {
			std::cerr << " * Can't prepare query: " << sqlite3_errmsg (db) << "\n";
			throw std::runtime_error ("Can't prepare query.");
		}
		std::clog << "\n * Prepared query: SELECT stops";
		while (sqlite3_step (select_stops) == SQLITE_ROW) {
			std::string stop_id = (char*)sqlite3_column_text (select_stops, 0);
			gps::Coord pos (sqlite3_column_double (select_stops, 1),
							sqlite3_column_double (select_stops, 2));
			std::shared_ptr<Stop> stop (new Stop (stop_id, pos));
			stops.emplace (stop_id, stop);
		}
		sqlite3_finalize (select_stops);

		// --- Load all intersections
		sqlite3_stmt* select_ints;
		if (sqlite3_prepare_v2 (db, "SELECT intersection_id, type, lat, lng FROM intersections",
								-1, &select_ints, 0) != SQLITE_OK) {
			std::cerr << " * Can't prepare query: " << sqlite3_errmsg (db) << "\n";
			throw std::runtime_error ("Can't prepare query.");
		}
		std::clog << "\n * Prepared query: SELECT intersections";
		while (sqlite3_step (select_ints) == SQLITE_ROW) {
			unsigned int int_id = sqlite3_column_int (select_ints, 0);
			std::string type = (char*)sqlite3_column_text (select_ints, 1);
			gps::Coord pos (sqlite3_column_double (select_ints, 2),
							sqlite3_column_double (select_ints, 3));
			std::shared_ptr<Intersection> Int (new Intersection (int_id, pos, type));
			intersections.emplace (int_id, Int);
		}
		sqlite3_finalize (select_ints);

		// --- Load all segments
		sqlite3_stmt* select_segs;
		if (sqlite3_prepare_v2 (db, "SELECT segment_id, from_id, to_id, start_at, end_at FROM segments",
								-1, &select_segs, 0) != SQLITE_OK) {
			std::cerr << " * Can't prepare query: " << sqlite3_errmsg (db) << "\n";
			throw std::runtime_error ("Can't prepare query.");
		}
		std::clog << "\n * Prepared query: SELECT segments";
		while (sqlite3_step (select_segs) == SQLITE_ROW) {
			unsigned long seg_id = sqlite3_column_int (select_segs, 0);
			if (sqlite3_column_type (select_segs, 1) == SQLITE_INTEGER &&
				sqlite3_column_type (select_segs, 3) == SQLITE_NULL) {
				// FROM intersection
				auto from = get_intersection (sqlite3_column_int (select_segs, 1));
				if (sqlite3_column_type (select_segs, 2) == SQLITE_INTEGER &&
					sqlite3_column_type (select_segs, 4) == SQLITE_NULL) {
					// INTERSECTION -> INTERSECTION
					auto to = get_intersection (sqlite3_column_int (select_segs, 2));
					std::shared_ptr<Segment> seg (new Segment(seg_id, from, to, 0));
					segments.emplace (seg_id, seg);
				} else if (sqlite3_column_type (select_segs, 2) == SQLITE_NULL &&
						   sqlite3_column_type (select_segs, 4) == SQLITE_TEXT) {
				    // INTERSECTION -> STOP
				    std::string stop_id = (char*)sqlite3_column_text (select_segs, 4);
					auto to = get_stop (stop_id);
					std::shared_ptr<Segment> seg (new Segment(seg_id, from, to, 0));
					segments.emplace (seg_id, seg);
			    } else {
					// std::cout << "[1]";
				}
			} else if (sqlite3_column_type (select_segs, 1) == SQLITE_NULL &&
					   sqlite3_column_type (select_segs, 3) == SQLITE_TEXT) {
			    // FROM stop
				std::string stop_id = (char*)sqlite3_column_text (select_segs, 3);
				auto from = get_stop (stop_id);
				if (sqlite3_column_type (select_segs, 2) == SQLITE_INTEGER &&
					sqlite3_column_type (select_segs, 4) == SQLITE_NULL) {
					// STOP -> INTERSECTION
					auto to = get_intersection (sqlite3_column_int (select_segs, 2));
					std::shared_ptr<Segment> seg (new Segment(seg_id, from, to, 0));
					segments.emplace (seg_id, seg);
				} else if (sqlite3_column_type (select_segs, 2) == SQLITE_NULL &&
						   sqlite3_column_type (select_segs, 4) == SQLITE_TEXT) {
				    // STOP -> STOP
					std::string stop_id = (char*)sqlite3_column_text (select_segs, 4);
					auto to = get_stop (stop_id);
					std::shared_ptr<Segment> seg (new Segment(seg_id, from, to, 0));
					segments.emplace (seg_id, seg);
			    } else {
					// std::cout << "[2: " << seg_id << "]";
				}
		    } else {
				// std::cout << "[3]";
			}
		}
		sqlite3_finalize (select_segs);

		/**
		if (false) {
			std::string qry;

			// Stops
			sqlite3_stmt* stmt_stops;
			qry = "SELECT stop_id, lat, lng FROM stops";
			if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &stmt_stops, 0) != SQLITE_OK) {
				std::cerr << " * Can't prepare query " << qry << "\n";
				throw std::runtime_error ("Cannot prepare query.");
			};
			std::clog << " * [prepared] " << qry << "\n";
			while (sqlite3_step (stmt_stops) == SQLITE_ROW) {
				std::string stop_id = (char*)sqlite3_column_text (stmt_stops, 0);
				gps::Coord pos (sqlite3_column_double (stmt_stops, 1),
								sqlite3_column_double (stmt_stops, 2));
				std::shared_ptr<gtfs::Stop> stop (new gtfs::Stop(stop_id, pos));
				stops.emplace (stop_id, stop);
			}
			sqlite3_finalize (stmt_stops);

			// N segments:
			sqlite3_stmt* stmt_n;
			int Nseg = 0;
			std::string qwhere;
			qwhere = " WHERE segment_id IN (SELECT segment_id FROM shapes WHERE shape_id LIKE '%_v" + v + "')";
			// qwhere = " WHERE segment_id IN (SELECT segment_id FROM shapes WHERE shape_id IN ("
			// 	"SELECT shape_id FROM routes WHERE shape_id LIKE '%_v" + v + "' AND route_short_name IN ('274','277','258','NEX','881') ) )";
			if (sqlite3_prepare_v2 (db, ("SELECT count(segment_id) FROM segments" + qwhere).c_str (),
									-1, &stmt_n, 0) == SQLITE_OK &&
				sqlite3_step (stmt_n) == SQLITE_ROW) {
				Nseg = sqlite3_column_int (stmt_n, 0);
			}
			sqlite3_finalize (stmt_n);

			// Load all gtfs `segments`
			sqlite3_stmt* stmt_segs;
			qry = "SELECT segment_id, start_id, end_id, length FROM segments" + qwhere;

			if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &stmt_segs, 0) != SQLITE_OK) {
				std::cerr << " * Can't prepare query " << qry << ": " << sqlite3_errmsg (db) << "\n";
				throw std::runtime_error ("Cannot prepare query.");
			}
			std::clog << " * [prepared] " << qry << "\n";
			// PREPARE query for segment path
			sqlite3_stmt* stmt_path;
			if (sqlite3_prepare_v2 (db, "SELECT lat, lng, seg_dist_traveled FROM segment_pt WHERE segment_id=?1 ORDER BY seg_pt_sequence", -1, &stmt_path, 0) != SQLITE_OK) {
				throw std::runtime_error ("Cannot prepare SELECT PATH query.");
			}
			int i = 0;
			while (sqlite3_step (stmt_segs) == SQLITE_ROW) {
				unsigned long segment_id = sqlite3_column_int (stmt_segs, 0);
				std::shared_ptr<Intersection> start_at, end_at;
				if (sqlite3_column_type (stmt_segs, 1) != SQLITE_NULL) {
					unsigned long start_id = sqlite3_column_int (stmt_segs, 1);
					// get intersection object

				}
				if (sqlite3_column_type (stmt_segs, 2) != SQLITE_NULL) {
					unsigned long end_id = sqlite3_column_int (stmt_segs, 2);
					// get intersection object

				}
				double length = sqlite3_column_double (stmt_segs, 3);
				// Get the segment's PATH:
				if (sqlite3_bind_int64 (stmt_path, 1, segment_id) != SQLITE_OK) {
					throw std::runtime_error ("Cannot fetch segment path.");
				}
				std::vector<ShapePt> path;
				while (sqlite3_step (stmt_path) == SQLITE_ROW) {
					path.emplace_back (gps::Coord (sqlite3_column_double (stmt_path, 0),
												   sqlite3_column_double (stmt_path, 1)),
									   sqlite3_column_double (stmt_path, 2));
				}
				sqlite3_reset (stmt_path);
				std::shared_ptr<Segment> segment (new Segment (segment_id, start_at, end_at, path, length));
				segments.emplace (segment_id, segment);

				printf(" * Loading shapes: %*d%%\r", 3, 100 * (++i) / Nseg);
				std::cout.flush ();
			}
			sqlite3_finalize (stmt_segs);
			sqlite3_finalize (stmt_path);

			// Load all gtfs `shapes`
			sqlite3_stmt* stmt_shapes;
			qry = "SELECT shape_id, leg, segment_id, shape_dist_traveled FROM shapes";
			if (version_ != "") qry += " WHERE shape_id LIKE '%_v" + version_ + "'";
			qry += " ORDER BY shape_id, leg";
			if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &stmt_shapes, 0) != SQLITE_OK) {
				std::cerr << " * Can't prepare query " << qry << "\n";
				throw std::runtime_error ("Cannot prepare query.");
			}
			std::clog << " * [prepared] " << qry << "\n";
			while (sqlite3_step (stmt_shapes) == SQLITE_ROW) {
				std::string shape_id = (char*)sqlite3_column_text (stmt_shapes, 0);
				std::shared_ptr<Shape> shape (get_shape (shape_id));
				// Does that shape already exist?
				if (shape == nullptr) {
					// Nope - create a new one
					std::shared_ptr<Shape> sh (new Shape(shape_id));
					shape = sh;
					shapes.emplace (shape_id, shape);
				}
				// Find segment:
				std::shared_ptr<Segment> segment = get_segment (sqlite3_column_int (stmt_shapes, 2));
				if (segment != nullptr) {
				  shape->add_segment (segment, sqlite3_column_double (stmt_shapes, 3));
				}
			}
			sqlite3_finalize (stmt_shapes);

			// Load all gtfs `routes`
			sqlite3_stmt* stmt_routes;
			qry = "SELECT route_id, route_short_name, route_long_name, shape_id FROM routes";
			if (version_ != "") qry += " WHERE route_id LIKE '%_v" + version_ + "'";
			if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &stmt_routes, 0) != SQLITE_OK) {
				std::cerr << " * Can't prepare query " << qry << "\n";
				throw std::runtime_error ("Cannot prepare query.");
			}
			std::clog << " * [prepared] " << qry << "\n";
			while (sqlite3_step (stmt_routes) == SQLITE_ROW) {
				std::string route_id = (char*)sqlite3_column_text (stmt_routes, 0);
				std::string short_name = (char*)sqlite3_column_text (stmt_routes, 1);
				std::string long_name = (char*)sqlite3_column_text (stmt_routes, 2);
				std::string shape_id = (char*)sqlite3_column_text (stmt_routes, 3);
				std::shared_ptr<Shape> shape (get_shape (shape_id));
				std::shared_ptr<gtfs::Route> route (new gtfs::Route(route_id, short_name, long_name, shape));
				routes.emplace (route_id, route);
			}
			sqlite3_finalize (stmt_routes);


			// n TRIPS
			int Ntrip = 0;
			qwhere = " WHERE segment_id IN (SELECT segment_id FROM shapes WHERE shape_id LIKE '%_v" + v + "')";
			// qwhere = " WHERE route_id IN (SELECT route_id FROM routes WHERE route_id LIKE '%_v"
			// 	+ v + "' AND route_short_name IN ('274','277','258','NEX','881') )";
			if (sqlite3_prepare_v2 (db, ("SELECT count(trip_id) FROM trips" + qwhere).c_str (),
									-1, &stmt_n, 0) == SQLITE_OK &&
				sqlite3_step (stmt_n) == SQLITE_ROW) {
				Ntrip = sqlite3_column_int (stmt_n, 0);
			}
			sqlite3_finalize (stmt_n);

			// Load all gtfs `trips`
			sqlite3_stmt* stmt_trips;
			sqlite3_stmt* stmt_stoptimes;
			qry = "SELECT trip_id, route_id FROM trips" + qwhere;

			if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &stmt_trips, 0) != SQLITE_OK) {
				std::cerr << " * Can't prepare query " << qry << "\n";
				throw std::runtime_error ("Cannot prepare query.");
			}
			std::cout << " * [prepared] " << qry << "\n";
			std::string qry_st ("SELECT stop_id, arrival_time, departure_time, shape_dist_traveled FROM stop_times WHERE trip_id=$1 ORDER BY stop_sequence");
			if (sqlite3_prepare_v2 (db, qry_st.c_str (), -1, &stmt_stoptimes, 0) != SQLITE_OK) {
				std::cerr << " * Can't prepare query " << qry_st << "\n";
				throw std::runtime_error ("Cannot prepare query.");
			}
			std::clog << " * [prepared] " << qry_st << "\n";

			int ti=0;
			std::cout << "\n   - " << Ntrip << " trips to load ...\n";
			while (sqlite3_step (stmt_trips) == SQLITE_ROW) {
				ti++;
				printf(" * Loading trips: %*d%%\r", 3, 100 * (ti) / Ntrip);
				std::cout.flush ();

				// Load that trip into memory: [id, (route)]
				std::string trip_id = (char*)sqlite3_column_text (stmt_trips, 0);
				std::string route_id = (char*)sqlite3_column_text (stmt_trips, 1);
				auto route = get_route (route_id);
				if (!route) continue;
				std::shared_ptr<gtfs::Trip> trip (new gtfs::Trip(trip_id, route));
				route->add_trip (trip);

				// Load trip stop times
				if (sqlite3_bind_text (stmt_stoptimes, 1, trip_id.c_str (), -1, SQLITE_STATIC) == SQLITE_OK) {
					std::vector<StopTime> stoptimes;
					std::vector<double> distances;
					while (sqlite3_step (stmt_stoptimes) == SQLITE_ROW) {
						// Take stop_time append to vector
						std::string stop_id = (char*)sqlite3_column_text (stmt_stoptimes, 0);
						auto stop = get_stop (stop_id);
						if (!stop) break; // missing any stops, don't include any
						std::string arr = (char*)sqlite3_column_text (stmt_stoptimes, 1);
						std::string dep = (char*)sqlite3_column_text (stmt_stoptimes, 2);
						stoptimes.emplace_back (stop, arr, dep);
						distances.emplace_back (sqlite3_column_double (stmt_stoptimes, 3));
					}
					sqlite3_reset (stmt_stoptimes);

					trip->add_stoptimes (stoptimes);

					// If route doesn't have stops already, add them now:
					if (route->get_stops ().size () == 0) {
						std::vector<RouteStop> stops;
						stops.reserve (trip->get_stoptimes ().size ());
						int si = 0;
						for (auto& st: trip->get_stoptimes ()) {
							if (distances.size () == trip->get_stoptimes ().size ()) {
								stops.emplace_back (st.stop, distances[si]);
								si++;
							} else {
								stops.emplace_back (st.stop, 0);
							}
						}
						route->add_stops (stops);
					}
				} else {
					std::cerr << "\n     x Error binding trip ID to stoptimes query ";
					continue;
				}

				// And save the trip away
				trips.emplace (trip_id, trip);
			}
			sqlite3_finalize (stmt_stoptimes);
			sqlite3_finalize (stmt_trips);
			std::clog << "\n";

			sqlite3_close (db);
		}
		**/
	};


	/**
	 * Load a Stop object.
	 * @param  s the ID of the stop we want
	 * @return a Stop object, or null pointer if it wasn't found
	 */
	std::shared_ptr<Stop> GTFS::get_stop (std::string& s) const {
		auto si = stops.find (s);
		if (si == stops.end ()) return nullptr;

		return si->second;
	}

	/**
	 * Load an Intersection object.
	 * @param  i the ID of the intersection we want
	 * @return an Intersection object, or null pointer if it wasn't found
	 */
	std::shared_ptr<Intersection> GTFS::get_intersection (unsigned int i) const {
		auto ii = intersections.find (i);
		if (ii == intersections.end ()) return nullptr;

		return ii->second;
	}

	/**
	 * Load a Segment object.
	 * @param  s the ID of the segment we want
	 * @return a Segment object, or null pointer if it wasn't found
	 */
	std::shared_ptr<Segment> GTFS::get_segment (unsigned long s) const {
		auto si = segments.find (s);
		if (si == segments.end ()) return nullptr;

		return si->second;
	}

	// --- More complex `get` methods - load if not already present

	/**
	 * Load a Trip object.
	 * @param  t the ID of the trip we want
	 * @return a Trip object, or null pointer if it wasn't found
	 */
	std::shared_ptr<Trip> GTFS::get_trip (std::string& t) {
		auto ti = trips.find (t);
		if (ti == trips.end ()) {
			// Create trip and emplace into `trips`
			sqlite3 *db;
			if (sqlite3_open (database_.c_str (), &db)) {
				std::cerr << "\n * Can't open database: " << sqlite3_errmsg (db) << "\n";
				sqlite3_close (db);
				return nullptr;
			}

			// --- Get the ROUTE
			sqlite3_stmt* select_route_id;
			if (sqlite3_prepare_v2 (db, "SELECT route_id FROM trips WHERE trip_id=?1",
									-1, &select_route_id, 0) != SQLITE_OK) {
				std::cerr << "\n * Can't prepare query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_route_id);
				sqlite3_close (db);
				return nullptr;
			}
			if (sqlite3_bind_text (select_route_id, 1, t.c_str (), -1, SQLITE_STATIC) != SQLITE_OK) {
				std::cerr << " * Can't bind trip_id to query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_route_id);
				sqlite3_close (db);
				return nullptr;
			}
			if (sqlite3_step (select_route_id) != SQLITE_ROW) {
				std::cerr << " * Error executing query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_route_id);
				sqlite3_close (db);
				return nullptr;
			}
			std::string route_id = (char*)sqlite3_column_text (select_route_id, 0);
			sqlite3_finalize (select_route_id);

			std::shared_ptr<Route> route = get_route (route_id);
			if (!route) {
				sqlite3_close (db);
				std::cout << "+";
				return nullptr;
			}

			// create the trip object and add to trips (it doesn't *have* to have stop times)
			std::shared_ptr<Trip> trip (new Trip (t, route));

			trips.emplace (t, trip);

			// --- Get the STOP TIMES
			sqlite3_stmt* select_stop_times;
			std::string qry = "SELECT stop_id, arrival_time, departure_time FROM stop_times "
				"WHERE trip_id=? ORDER BY stop_sequence";
			if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_stop_times, 0) != SQLITE_OK) {
				std::cerr << "\n * Can't prepare query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_stop_times);
				sqlite3_close (db);
				return trip;
			}
			if (sqlite3_bind_text (select_stop_times, 1, t.c_str (), -1, SQLITE_STATIC) != SQLITE_OK) {
				std::cerr << " * Can't bind trip_id to query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_stop_times);
				sqlite3_close (db);
				return trip;
			}
			std::vector<StopTime> stoptimes;
			while (sqlite3_step (select_stop_times) == SQLITE_ROW) {
				std::string stopid = (char*)sqlite3_column_text (select_stop_times, 0);
				auto stop = get_stop (stopid);
				if (!stop) {
					sqlite3_finalize (select_stop_times);
					sqlite3_close (db);
					return trip;
				}
				std::string arr = (char*)sqlite3_column_text (select_stop_times, 1);
				std::string dep = (char*)sqlite3_column_text (select_stop_times, 2);
				stoptimes.emplace_back (stop, arr, dep);
			}
			sqlite3_finalize (select_stop_times);
			sqlite3_close (db);
			trip->add_stoptimes (stoptimes);
			return trip;
		}
		return ti->second;
	}

	/**
	 * Load a Route object.
	 * @param  r the ID of the route we want
	 * @return a Route object, or null pointer if it wasn't found
	 */
	std::shared_ptr<Route> GTFS::get_route (std::string& r) {
		auto ri = routes.find (r);
		if (ri == routes.end ()) {
			// Create route and emplace into `routes`
			sqlite3 *db;
			if (sqlite3_open (database_.c_str (), &db)) {
				std::cerr << "\n * Can't open database: " << sqlite3_errmsg (db) << "\n";
				sqlite3_close (db);
				return nullptr;
			}
			sqlite3_stmt* select_route;
			std::string qry = "SELECT route_short_name, route_long_name, shape_id FROM routes "
				"WHERE route_id=?1";
			if (sqlite3_prepare_v2 (db, qry.c_str (),
									-1, &select_route, 0) != SQLITE_OK) {
				std::cerr << "\n * Can't prepare query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_route);
				sqlite3_close (db);
				return nullptr;
			}
			if (sqlite3_bind_text (select_route, 1, r.c_str (), -1, SQLITE_STATIC) != SQLITE_OK) {
				std::cerr << " * Can't bind route_id to query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_route);
				sqlite3_close (db);
				return nullptr;
			}
			if (sqlite3_step (select_route) != SQLITE_ROW) {
				std::cerr << " * Error executing query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_route);
				sqlite3_close (db);
				return nullptr;
			}
			std::string shortname = (char*)sqlite3_column_text (select_route, 0);
			std::string longname = (char*)sqlite3_column_text (select_route, 1);
			std::string shapeid = (char*)sqlite3_column_text (select_route, 2);
			std::shared_ptr<Route> route (new Route (r, shortname, longname));
			sqlite3_finalize (select_route);

			std::shared_ptr<Shape> shape = get_shape (shapeid);
			if (!shape) {
				sqlite3_close (db);
				std::cout << "x";
				return nullptr;
			}

			route->add_shape (shape);

			// --- Route Stops
			sqlite3_stmt* select_routestops;
			qry = "SELECT stop_id, shape_dist_traveled FROM stop_times "
				"WHERE trip_id IN (SELECT trip_id FROM trips WHERE route_id=? LIMIT 1) ORDER BY stop_sequence";
			if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_routestops, 0) != SQLITE_OK) {
				std::cerr << "\n * Can't prepare query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_routestops);
				sqlite3_close (db);
				return nullptr;
			}
			if (sqlite3_bind_text (select_routestops, 1, r.c_str (), -1, SQLITE_STATIC) != SQLITE_OK) {
				std::cerr << " * Can't bind route_id to query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_routestops);
				sqlite3_close (db);
				return nullptr;
			}
			std::vector<RouteStop> rstops;
			while (sqlite3_step (select_routestops) == SQLITE_ROW) {
				std::string stopid = (char*)sqlite3_column_text (select_routestops, 0);
				auto stop = get_stop (stopid);
				if (!stop) {
					sqlite3_finalize (select_routestops);
					sqlite3_close (db);
					return nullptr;
				}
				rstops.emplace_back (stop, sqlite3_column_double (select_routestops, 1));
			}
			sqlite3_finalize (select_routestops);
			sqlite3_close (db);

			route->add_stops (rstops);
			routes.emplace (r, route);

			return route;
		}
		return ri->second;
	}

	/**
	 * Load a Shape object.
	 * @param  s the ID of the shape we want
	 * @return a Shape object, or null pointer if it wasn't found
	 */
	std::shared_ptr<Shape> GTFS::get_shape (std::string& s) {
		auto si = shapes.find (s);
		if (si == shapes.end ()) {
			// Create shape and emplace into `shapes`
			sqlite3 *db;
			if (sqlite3_open (database_.c_str (), &db)) {
				std::cerr << "\n * Can't open database: " << sqlite3_errmsg (db) << "\n";
				sqlite3_close (db);
				return nullptr;
			}

			// --- PATH
			sqlite3_stmt* select_path;
			std::string qry = "SELECT lat, lng, dist_traveled FROM shapes "
				"WHERE shape_id=? ORDER BY seq";
			if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_path, 0) != SQLITE_OK) {
				std::cerr << "\n * Can't prepare query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_path);
				sqlite3_close (db);
				return nullptr;
			}
			if (sqlite3_bind_text (select_path, 1, s.c_str (), -1, SQLITE_STATIC) != SQLITE_OK) {
				std::cerr << " * Can't bind shape_id to query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_path);
				sqlite3_close (db);
				return nullptr;
			}
			std::vector<ShapePt> shapepts;
			while (sqlite3_step (select_path) == SQLITE_ROW) {
				shapepts.emplace_back (gps::Coord (sqlite3_column_double (select_path, 0),
												   sqlite3_column_double (select_path, 1)),
									   sqlite3_column_double (select_path, 2));
			}
			sqlite3_finalize (select_path);

			// --- SEGMENTS
			sqlite3_stmt* select_segs;
			qry = "SELECT segment_id, shape_dist_traveled FROM shape_segments "
				"WHERE shape_id=? ORDER BY leg";
			if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_segs, 0) != SQLITE_OK) {
				std::cerr << "\n * Can't prepare query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_segs);
				sqlite3_close (db);
				return nullptr;
			}
			if (sqlite3_bind_text (select_segs, 1, s.c_str (), -1, SQLITE_STATIC) != SQLITE_OK) {
				std::cerr << " * Can't bind shape_id to query: " << sqlite3_errmsg (db) << "\n";
				sqlite3_finalize (select_segs);
				sqlite3_close (db);
				return nullptr;
			}
			std::vector<ShapeSegment> shapesegs;
			while (sqlite3_step (select_segs) == SQLITE_ROW) {
				auto seg = get_segment ((unsigned long)sqlite3_column_int (select_segs, 0));
				if (!seg) {
					sqlite3_finalize (select_segs);
					sqlite3_close (db);
					return nullptr;
				}
				shapesegs.emplace_back (seg, sqlite3_column_double (select_segs, 1));
			}
			sqlite3_finalize (select_segs);
			sqlite3_close (db);

			std::shared_ptr<Shape> shape (new Shape (s, shapepts, shapesegs));
			shapes.emplace (s, shape);
			return shape;
		}
		return si->second;
	}



	/**
	 * Get the coordinates of a point a given distance along a shape.
	 * @param  distance total distance traveled along path, in meters
	 * @param  shape    the shape path being traveled along
	 * @return          a coordinate object
	 */
	gps::Coord get_coords (double distance, std::shared_ptr<Shape> shape) {
		auto path = shape->get_path ();
		for (unsigned int i=0; i<path.size (); i++) {
			if (path[i+1].dist_traveled > distance) {
				return path[i].pt.destinationPoint (
					distance - path[i].dist_traveled,
					path[i].pt.bearingTo (path[i+1].pt)
				);
				break;
			}
		}

		return gps::Coord (0, 0);
	};
};
