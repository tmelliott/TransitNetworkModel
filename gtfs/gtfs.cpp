#include <string>
#include <vector>
#include <stdexcept>
#include <sqlite3.h>

#include "gtfs.h"
#include "gps.h"

namespace gtfs {
	GTFS::GTFS (std::string& dbname, std::string& v) : database_ (dbname), version_ (v) {
		sqlite3 *db;
		if (sqlite3_open (database_.c_str (), &db)) {
			std::cerr << " * Can't open database: " << sqlite3_errmsg (db) << "\n";
			throw std::runtime_error ("Can't open database.");
		}
		std::cout << " * Connected to database \"" << database_ << "\"\n";

		std::string qry;

		// Load all gtfs `segments`
		

		// Load all gtfs `shapes`
		sqlite3_stmt* stmt_shapes;
		qry = "SELECT shape_id, leg, segment_id, shape_dist_traveled FROM shapes";
		if (version_ != "") qry += " WHERE shape_id LIKE '%_v" + version_ + "'";
		qry += " ORDER BY shape_id, leg";
		if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &stmt_shapes, 0) != SQLITE_OK) {
			std::cerr << " * Can't prepare query " << qry << "\n";
			throw std::runtime_error ("Cannot prepare query.");
		}
		std::cout << " * [prepared] " << qry << "\n";
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
			shape->add_segment (nullptr, sqlite3_column_double (stmt_shapes, 3));
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
		std::cout << " * [prepared] " << qry << "\n";
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
		std::cout << " * [prepared] " << qry << "\n";
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
		std::cout << "\n";

	};

	std::shared_ptr<Route> GTFS::get_route (std::string& r) const {
		auto ri = routes.find (r);
		if (ri == routes.end ()) return nullptr;
		return ri->second;
	}

	std::shared_ptr<Trip> GTFS::get_trip (std::string& t) const {
		auto ti = trips.find (t);
		if (ti == trips.end ()) {
			return nullptr;
		} else {
			return ti->second;
		}
	}

	std::shared_ptr<Shape> GTFS::get_shape (std::string& s) const {
		auto si = shapes.find (s);
		if (si == shapes.end ()) {
			return nullptr;
		} else {
			return si->second;
		}
	}


	/**
	 * Get the coordinates of a point a given distance along a path.
	 * @param  distance total distance traveled along path, in meters
	 * @param  path     the path being traveled along
	 * @return          a coordinate object
	 */
	gps::Coord get_coords (double distance, std::vector<gps::Coord> path) {
		return gps::Coord (-33.2, 174.5);
	};
};
