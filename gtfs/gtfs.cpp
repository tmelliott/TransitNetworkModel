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
		std::clog << " * Connected to database \"" << database_ << "\"\n";

		std::string qry;

		// N segments:
		sqlite3_stmt* stmt_n;
		int Nseg = 0;
		std::string qwhere;
		qwhere = " WHERE segment_id IN (SELECT segment_id FROM shapes WHERE shape_id LIKE '%_v" + v + "')";
		// qwhere = " WHERE segment_id IN (SELECT segment_id FROM shapes WHERE shape_id IN ("
		// 	"SELECT shape_id FROM routes WHERE shape_id LIKE '%_v" + v + "' AND route_short_name IN ('274','224','227','258') ) )";
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
			std::cerr << " * Can't prepare query " << qry << "\n";
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

		// Load all gtfs `trips`
		sqlite3_stmt* stmt_trips;
		qry = "SELECT trip_id, route_id FROM trips";
		if (version_ != "") qry += " WHERE trip_id LIKE '%_v" + version_ + "'";
		if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &stmt_trips, 0) != SQLITE_OK) {
			std::cerr << " * Can't prepare query " << qry << "\n";
			throw std::runtime_error ("Cannot prepare query.");
		}
		std::clog << " * [prepared] " << qry << "\n";
		while (sqlite3_step (stmt_trips) == SQLITE_ROW) {
			// Load that trip into memory: [id, (route)]
			std::string trip_id = (char*)sqlite3_column_text (stmt_trips, 0);
			std::string route_id = (char*)sqlite3_column_text (stmt_trips, 1);
			auto route = get_route (route_id);
			std::shared_ptr<gtfs::Trip> trip (new gtfs::Trip(trip_id, route));
			route->add_trip (trip);
			trips.emplace (trip_id, trip);
		}
		sqlite3_finalize (stmt_trips);
		std::clog << "\n";

		sqlite3_close (db);

	};

	/**
	 * Load a Route object.
	 * @param  r the ID of the route we want
	 * @return a Route object, or null pointer if it wasn't found
	 */
	std::shared_ptr<Route> GTFS::get_route (std::string& r) const {
		auto ri = routes.find (r);
		if (ri == routes.end ()) return nullptr;
		return ri->second;
	}

	/**
	 * Load a Trip object.
	 * @param  t the ID of the trip we want
	 * @return a Trip object, or null pointer if it wasn't found
	 */
	std::shared_ptr<Trip> GTFS::get_trip (std::string& t) const {
		auto ti = trips.find (t);
		if (ti == trips.end ()) {
			return nullptr;
		} else {
			return ti->second;
		}
	}

	/**
	 * Load a Shape object.
	 * @param  s the ID of the shape we want
	 * @return a Shape object, or null pointer if it wasn't found
	 */
	std::shared_ptr<Shape> GTFS::get_shape (std::string& s) const {
		auto si = shapes.find (s);
		if (si == shapes.end ()) {
			return nullptr;
		} else {
			return si->second;
		}
	}

	/**
	 * Load a Segment object.
	 * @param  s the ID of the segment we want
	 * @return a Segment object, or null pointer if it wasn't found
	 */
	std::shared_ptr<Segment> GTFS::get_segment (unsigned long s) const {
		auto si = segments.find (s);
		if (si == segments.end ()) {
			return nullptr;
		} else {
			return si->second;
		}
	}


	/**
	 * Get the coordinates of a point a given distance along a shape.
	 * @param  distance total distance traveled along path, in meters
	 * @param  shape    the shape path being traveled along
	 * @return          a coordinate object
	 */
	gps::Coord get_coords (double distance, std::shared_ptr<Shape> shape) {
		for (auto& seg: shape->get_segments ()) {
			if (seg.shape_dist_traveled + seg.segment->get_length () < distance) continue;
			// Point is somewhere along this segment ...
			auto path = seg.segment->get_path ();
			for (int i=0; i<path.size (); i++) {
				if (seg.shape_dist_traveled + path[i+1].seg_dist_traveled > distance) {
					return path[i].pt.destinationPoint (
						distance - (seg.shape_dist_traveled + path[i].seg_dist_traveled),
						path[i].pt.bearingTo (path[i+1].pt)
					);
					break;
				}

			}
		}

		return gps::Coord (0, 0);
	};
};
