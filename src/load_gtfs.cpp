/**
* Load the most recent GTFS static data from Auckland Transport.
*
* This program will populate the supplied database with the most recent
* version of the GTFS static feed from Auckland Transport.
*
* Segmentation will take place, in which shapes are split at intersections.
* Routes will be compared to existing routes, and if there are no changes
* the segments will simply be duplicated;
* otherwise new segments will be created.
*
* @file
* @author Tom Elliott <tom.elliott@auckland.ac.nz>
* @version 0.0.1
*/

#include <iostream>
#include <vector>
#include <stdlib.h>
#include <fstream>
#include <algorithm>

#include <boost/program_options.hpp>
#include <sqlite3.h>

#include "gps.h"
#include "gtfs.h"

#include "json.hpp"

namespace po = boost::program_options;

/**
 * A system() command that takes a std::string instead of a char.
 * @param  s The command to run.
 * @return   The return value from the system command.
 */
int system (std::string const& s) { return system (s.c_str ()); }
void convert_shapes (sqlite3* db);
void import_intersections (sqlite3* db, std::vector<std::string> files);
void calculate_stop_distances (std::string& dbname);

/**
 * Loads GTFS file into database and segments as necessary.
 *
 * @param  argc number of command line arguments
 * @param  argv arguments
 * @return      0 if everything went OK; 1 if it didn't
 */
int main (int argc, char* argv[]) {

	po::options_description desc ("Allowed options");

	/** database connection to use */
	std::string dbname;
	std::string dir;

	desc.add_options ()
		("database", po::value<std::string>(&dbname)->default_value ("gtfs.db"), "Name of the database to use.")
		("dir", po::value<std::string>(&dir)->default_value (".."), "Directory of the database files. Defaults to ..")
		("help", "Print this message and exit.")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count ("help")) {
		std::cout << desc << "\n";
		return 1;
	}


	// STEP ONE:
	// connect to the SQLite database:
	// std::cout << "Loading GTFS data into database `" << dbname << "`\n";
	// sqlite3 *db;
	// char *zErrMsg = 0;
	// int rc;
	//
	// rc = sqlite3_open ((dir + "/" + dbname).c_str(), &db);
	// if (rc) {
	// 	fprintf(stderr, " * Can't open database: %s\n", sqlite3_errmsg(db));
    //   	return(0);
	// } else {
    // 	fprintf(stderr, " * Opened database successfully\n");
	// }
	//
	// // STEP TWO:
	// // convert shapes -> segments
	// std::cout << " * Converting shapes to segments ... ";
	// convert_shapes (db); // -- temporary dont let it run (though it should die since shapes_tmp not present)
	// std::cout << "\n   ... done.\n";
	//
	// // STEP THREE:
	// // importing intersections.json and segmenting segments:
	// std::cout << " * Importing intersections ... ";
	// std::vector<std::string> files {dir + "/data/intersections_trafficlights.json",
	// 								dir + "/data/intersections_roundabouts.json"};
	// import_intersections (db, files);
	// std::cout << "done.\n";
	//
	// // Get all segments, and split into more segments
	// for (int i=0;i<1000;i++) {
	// 	printf(" * Segmenting shapes ... %*d%%\r", 3, (i+1)/1000 * 100);
	// 	std::cout.flush ();
	//
	//
	// }
	//
	// sqlite3_close (db);
	//
	// std::cout << " * Segmenting shapes ... done.\n";


	// STEP FOUR: stop distance into shape for stop_times
	std::cout << " * Calculating distance into trip of stops ... \n";
	std::string dbn = dir + "/" + dbname;
	calculate_stop_distances (dbn);
	std::cout << "\n   ... done.\n";

	return 0;
}


/**
 * Converts shapes from their raw GTFS format to our segmented format.
 *
 * For now, each shape is simply converted into a single segment.
 * @param db the database to work with
 */
void convert_shapes (sqlite3* db) {
	sqlite3_stmt* stmt;
	sqlite3_stmt* stmt_get_shape;
	sqlite3_stmt* stmt_seg_ins;
	sqlite3_stmt* stmt_shape_ins;
	sqlite3_stmt* stmt_shapept_ins;

	if (sqlite3_prepare_v2 (db, "SELECT DISTINCT shape_id FROM shapes_tmp",
							-1, &stmt, 0) == SQLITE_OK) {
		std::cout << "\n   - SELECT query prepared ";
	} else {
		std::cerr << "\n   x Unable to prepare query ";
		return;
	}

	if (sqlite3_prepare_v2 (db, (char*)("SELECT shape_pt_lat, shape_pt_lon FROM shapes_tmp "
								 		"WHERE shape_id = ?1 ORDER BY CAST(shape_pt_sequence AS INT)"),
							-1, &stmt_get_shape, 0) == SQLITE_OK) {
		std::cout << "\n   - SELECT shape query prepared ";
	} else {
		std::cerr << "\n   x Unable to prepare shape query ";
		return;
	}

	if (sqlite3_prepare_v2  (db, "INSERT INTO segments (length) VALUES (?1)",
							 -1, &stmt_seg_ins, 0) == SQLITE_OK) {
	    std::cout << "\n   - INSERT query prepared ";
	} else {
	    std::cerr << "\n   x Unable to prepare INSERT query ";
		return;
	}

	if (sqlite3_prepare_v2 (db, "INSERT INTO shapes VALUES (?1, 1, ?2, 0)",
							-1, &stmt_shape_ins, 0) == SQLITE_OK) {
		std::cout << "\n   - INSERT shape prepared ";
	} else {
		std::cerr << "\n   x Error preparing shape insert query ";
		return;
	}

	if (sqlite3_prepare_v2 (db, "INSERT INTO segment_pt VALUES (?1, ?2, ?3, ?4, ?5)",
							-1, &stmt_shapept_ins, 0) == SQLITE_OK) {
		std::cout << "\n   - INSERT shape points prepared ";
	} else {
		std::cerr << "\n   x Error preparing shape point insert query ";
		return;
	}

	// For a progress bar (because we can't not have one!)
	int nshapes = 0;
	sqlite3_stmt* stmt_nrow;
	if (sqlite3_prepare_v2 (db, "SELECT COUNT(DISTINCT shape_id) FROM shapes_tmp",
							-1, &stmt_nrow, 0) == SQLITE_OK &&
		sqlite3_step (stmt_nrow) == SQLITE_ROW) {
		nshapes = sqlite3_column_int (stmt_nrow, 0);
	}
	std::cout << "nshapes = " << nshapes;
	sqlite3_finalize (stmt_nrow);

	std::cout << "\n";
	int n = 0;
	while (sqlite3_step (stmt) == SQLITE_ROW) {
		if (nshapes > 0) {
			n++;
			printf("     + Shape %*d of %d (%*d%%) \r", 4, n, nshapes, 3, (int)(100 * (n) / nshapes));
			std::cout.flush();
		}
	    std::string shape_id = (char*)sqlite3_column_text (stmt, 0);
	    std::clog << "\n   +++ " << shape_id;

	    // LOAD SHAPE - COMPUTE LENGTH
		const char* sid = shape_id.c_str ();
		if (sqlite3_bind_text (stmt_get_shape, 1, sid, -1, SQLITE_STATIC) != SQLITE_OK) {
			std::cerr << "\n     x Error binding shape ID to query ";
			continue;
		} else {
			std::clog << "\n     + Shape ID bound successfully ";
		}

		std::vector<gps::Coord> path;
		while (sqlite3_step (stmt_get_shape) == SQLITE_ROW) {
			path.emplace_back (sqlite3_column_double (stmt_get_shape, 0),
							   sqlite3_column_double (stmt_get_shape, 1));
		}
		sqlite3_reset (stmt_get_shape);

		double length = 0.0;
		for (unsigned int i=1; i<path.size(); i++) {
			length += path[i-1].distanceTo (path[i]);
		}

	    // INSERT INTO segments (length) VALUES (?) - [length]
	    if (sqlite3_bind_double (stmt_seg_ins, 1, length) != SQLITE_OK) {
			std::cerr << "\n     x Error binding ID to query ";
			continue;
		} else {
			std::clog << "\n     + ID bound to query ";
		}
		auto res = sqlite3_step (stmt_seg_ins);
		if (res != SQLITE_ROW && res != SQLITE_DONE) {
			std::cerr << "\n     x Error running INSERT query ";
			continue;
		} else {
			std::clog << "\n     + Segment Inserted ";
		}
		sqlite3_reset (stmt_seg_ins);

		// RETURN ID of new segment
		auto segment_id = sqlite3_last_insert_rowid (db);
		std::clog << " -> segment_id: " << segment_id;

	    // INSERT INTO shapes (shape_id, leg, segment_id, shape_dist_traveled)
	    //      VALUES (?, 0, ?, 0) - [shape_id, segment_id]
	    if (sqlite3_bind_text (stmt_shape_ins, 1, sid, -1, SQLITE_STATIC) != SQLITE_OK ||
			sqlite3_bind_int64 (stmt_shape_ins, 2, segment_id) != SQLITE_OK) {
			std::cerr << "   Error binding shape ";
			continue;
		} else {
			std::clog << "   Shape bound OK ";
		}
		if (sqlite3_step (stmt_shape_ins) != SQLITE_DONE) {
			std::cerr << "    Error inserting shape ";
			continue;
		} else {
			std::clog << "    Shape inserted OK";
		}
		sqlite3_reset (stmt_shape_ins);


		// INSERT INTO segment_pt VALUES (?,?,?,?,?) - [segment_id, seg_pt_seq, lat, lng, dist]
		double dist = 0.0;
		sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
		for (unsigned int i=0; i<path.size (); i++) {
			if (i > 0) dist += path[i-1].distanceTo (path[i]);
			if (sqlite3_bind_int64 (stmt_shapept_ins, 1, segment_id) != SQLITE_OK ||
				sqlite3_bind_int (stmt_shapept_ins, 2, i+1) != SQLITE_OK ||
				sqlite3_bind_double (stmt_shapept_ins, 3, path[i].lat) != SQLITE_OK ||
				sqlite3_bind_double (stmt_shapept_ins, 4, path[i].lng) != SQLITE_OK ||
				sqlite3_bind_double (stmt_shapept_ins, 5, dist) != SQLITE_OK) {
				std::cerr << "   Error inserting shape points ";
				sqlite3_exec (db, "ROLLBACK", NULL, NULL, NULL);
				continue;
			}
			if (sqlite3_step (stmt_shapept_ins) != SQLITE_DONE) {
				std::cerr << "   Error running query :( ";
				sqlite3_exec (db, "ROLLBACK", NULL, NULL, NULL);
				continue;
			}
			// OK
			sqlite3_reset (stmt_shapept_ins);
		}
		sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);
	}
	std::clog << "\n   ";

	// Close the statements to prevent memory leak
	sqlite3_finalize (stmt);
	sqlite3_finalize (stmt_get_shape);
	sqlite3_finalize (stmt_seg_ins);
	sqlite3_finalize (stmt_shapept_ins);


	// Delete TMP tables
	sqlite3_exec (db, "DROP TABLE shapes_tmp", NULL, NULL, NULL);
};



/**
 * Import insersections from a JSON file into the database.
 *
 * Additionally, clusters of "points" belonging to the same intersection
 * are combined into a single object.
 *
 * @param db    the database to use
 * @param files the files to load intersections from
 */
void import_intersections (sqlite3* db, std::vector<std::string> files) {
	using json = nlohmann::json;

	// read json file
	std::vector<gps::Coord> ints;
	std::vector<int> types; // 0 = traffic light, 1 = roundabout
	for (auto file: files) {
		std::ifstream f(file.c_str ());
		json j;
		f >> j;

		int type = 0;
		if (file.find ("roundabout") != std::string::npos) type = 1;

		// get coordinates
		for (auto i: j["elements"]) {
			if (!i["lat"].empty () && !i["lon"].empty ()) {
				ints.emplace_back (i["lat"], i["lon"]);
				types.push_back (type);
			}
		}
	}

	// auto Ints = ints;
	// ints.clear();
	// for (int i=0; i<200; i++) ints.push_back (Ints[i]);


	// Compute distance between each 'intersection' point
	double threshold = 40.0;
	std::cout << "\n   + Loaded " << ints.size () << " intersections.\n";
	int N = ints.size ();
	std::vector<std::vector<double> > distmat (N, std::vector<double> (N, 0));
	for (int i=0; i<N; i++) {
		printf("   + Calculating distance matrix ... %*d%%\r", 3, 100 * (i+1) / N);
		std::cout.flush ();
		for (int j=i+1; j<N; j++) {
			distmat[i][j] = ints[i].distanceTo (ints[j]);
			distmat[j][i] = distmat[i][j];
		}
	}
	std::cout << "   + Calculating distance matrix ... done.\n";

	// for (auto& r: distmat) {
	// 	for (auto& c: r) printf(" %*.2f ", 8, c);
	// 	std::cout << "\n";
	// }


	// Kick out rows that are singletons
	std::cout << "   + Finding groups of intersections that could be just one intersection ... ";
	std::cout.flush ();
	std::vector<int> wkeep;
	for (int i=0; i<N; i++) {
		int sum = 0;
		for (int j=0; j<N; j++) sum += (int)(distmat[i][j] < threshold);
		if (sum > 1) wkeep.push_back (i);
	}
	// dmat is a logical matrix of clumped intersections
	N = wkeep.size ();
	std::vector<std::vector<bool> > dmat (N, std::vector<bool> (N, 0));
	for (int i=0; i<N; i++) {
		for (int j=0; j<N; j++) {
			dmat[i][j] = (distmat[wkeep[i]][wkeep[j]] < threshold);
		}
	}
	std::cout << "done.\n";

	// Go through and find individual clusters
	std::cout << "   + Identifying clusters ... \r";
	std::cout.flush ();
	std::vector<std::vector<int> > y;
	for (int i=0; i<N; i++) {
		printf("   + Identifying clusters ... %*d%%\r", 3, (int) (100 * (i+1) / N));
		std::cout.flush ();
		std::vector<int> xi;
		for (int j=0; j<N; j++) {
			if (dmat[i][j]) xi.push_back (j);
		}
		std::vector<int> xj;
		for (unsigned int j=0; j<xi.size (); j++) {
			for (int k=0; k<N; k++) {
				if (dmat[k][xi[j]] && std::find (xj.begin(), xj.end (), k) == xj.end ())
					xj.push_back (k);
			}
		}
		if (xj.size () > 0) {
			for (unsigned int j=0; j<xj.size (); j++) for (int k=0; k<N; k++) dmat[xj[j]][k] = false;
			y.emplace_back (xj);
		}
	}
	std::cout << "   + Identifying clusters ... done.\n";

	// Compute means and unify intersection clusters
	std::cout << "   + Create new intersections in the middle of clusters ... ";
	std::cout.flush ();
	for (unsigned int i=0; i<y.size (); i++) {
		auto newint = gps::Coord(0, 0);
		for (unsigned int j=0; j<y[i].size (); j++) {
			newint.lat += ints[wkeep[y[i][j]]].lat;
			newint.lng += ints[wkeep[y[i][j]]].lng;
		}
		newint.lat = newint.lat / y[i].size ();
		newint.lng = newint.lng / y[i].size ();
		ints[wkeep[y[i][1]]] = newint;
		for (unsigned int j=1; j<y[i].size (); j++) {
			ints[wkeep[y[i][j]]] = gps::Coord(1.0/0.0, 1.0/0.0);
		}
	}
	std::cout << "done.\n";

	std::cout << "   + Inserting intersections into database ... ";
	std::cout.flush ();


	// NOTE: not yet storing the intersection type ...
	sqlite3_stmt* stmt;
	if (sqlite3_prepare_v2 (db, "INSERT INTO intersections (type, lat, lng) VALUES ('traffic_light',?1,?2)",
							-1, &stmt, 0) != SQLITE_OK) {
		std::cerr << "\n   x Error preparing insert query ";
		return;
	}
	sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
	int Nfinal = 0;
	for (auto i: ints) {
		if (i.lat == INFINITY || i.lng == INFINITY) continue;
		if (sqlite3_bind_double (stmt, 1, i.lat) != SQLITE_OK ||
			sqlite3_bind_double (stmt, 2, i.lng) != SQLITE_OK) {
			std::cerr << "   Error binding parameters.\n";
			return;
		}
		if (sqlite3_step (stmt) != SQLITE_DONE) {
			std::cerr << "   Error running insert query.\n";
		}
		sqlite3_reset (stmt);
		Nfinal++;
	}
	sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);
	sqlite3_finalize (stmt);

	std::cout << "done.\n"
		<< "   -> Inserted " << Nfinal << " intersections.\n";
}

/**
 * Compute the shape distance traveled for each stop along a route.
 * @param dbname The database to connect to
 */
void calculate_stop_distances (std::string& dbname) {
	std::string v = "54.27";
	gtfs::GTFS gtfs (dbname, v);
	std::cout << " * GTFS database loaded     ";


	sqlite3* db;
	if (sqlite3_open (dbname.c_str (), &db)) {
		fprintf(stderr, " * Can't open database: %s\n", sqlite3_errmsg(db));
		return;
	}
	std::cout << " \n* Connected to database ";

	sqlite3_stmt* stmt_upd;
	if (sqlite3_prepare_v2 (db, "UPDATE stop_times SET shape_dist_traveled=$1 WHERE trip_id IN (SELECT trip_id FROM trips WHERE route_id=$2) AND stop_id=$3",
		  					-1, &stmt_upd, 0) != SQLITE_OK) {
		std::cerr << " x Unable to prepare UPDATE query :(";
		return;
	}

	auto Nr = gtfs.get_routes ().size ();
	unsigned int Ni = 0;
	std::cout << "\n* Computing stop distances into trip "
	<< "(" << Nr << " routes)\n";
	// For each route
	for (auto& r: gtfs.get_routes ()) {
		Ni++;
		printf("  %*d%%\r", 3, (int)(100 * Ni / Nr));
		std::cout.flush ();
		std::shared_ptr<gtfs::Route> route = std::get<1> (r);
		auto shape = route->get_shape ();
		if (!shape) continue; // no shape associated with this route

		auto stops = route->get_stops ();
		if (stops.size () == 0) continue; // No stops - skip!

		auto segments = shape->get_segments ();
		if (segments.size () == 0) continue;

		// Travel along the route until the next stop is between STOP and STOP+1
		unsigned int si = 0;
		for (auto& s: segments) {
			auto seg = s.segment;
			if (!seg) break;
			auto path = seg->get_path (); // sequence of ShapePt structs
			if (path.size () == 0) break;
			double dmin (s.shape_dist_traveled);

			// Check the first point in the segment (likely only for first stop)
			if (stops[si].stop->get_pos ().distanceTo (path[0].pt) < 1) {
				stops[si].shape_dist_traveled = dmin + path[0].seg_dist_traveled;
				// printf(" - Stop %*d [%*s]: %*d m\n",
				// 	   3, si+1,
				// 	   6, stops[si].stop->get_id ().c_str (),
				// 	   6, (int)stops[si].shape_dist_traveled);
				si ++;
				if (si == stops.size ()) break;
			}
			// step through path
			for (unsigned int i=1; i<path.size (); i++) {
				// ---Consider stop is AT the point if distance < 1m (rounding error!)
				// FIRST check stop I
				if (stops[si].stop->get_pos ().distanceTo (path[i].pt) < 1) {
					stops[si].shape_dist_traveled = dmin + path[i].seg_dist_traveled;
					// printf(" - Stop %*d [%*s]: %*d m\n",
					// 	   3, si+1,
					// 	   6, stops[si].stop->get_id ().c_str (),
					// 	   6, (int)stops[si].shape_dist_traveled);
					si ++;
					if (si == stops.size ()) break;

					continue; // it wont be BEHIND this point!
				}

				/** THEN, see if stop lies between stop i-1 and stop i
				 * actually, since AT joins route shapes to the stop,
				 * we don't need to do this bit
				 */
			}
		}

		if (si != stops.size ()) break; // unable to get distance for all stops

		// Now save the distances into the database for each trip
		sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
		for (auto& s: stops) {
			if (sqlite3_bind_double (stmt_upd, 1, s.shape_dist_traveled) != SQLITE_OK ||
				sqlite3_bind_text (stmt_upd, 2, route->get_id ().c_str (), -1, SQLITE_STATIC) != SQLITE_OK ||
				sqlite3_bind_text (stmt_upd, 3, s.stop->get_id ().c_str (), -1, SQLITE_STATIC) != SQLITE_OK) {
				std::cerr << "\n x Unable to bind parameters.";
				return;
			}
			if (sqlite3_step (stmt_upd) != SQLITE_DONE) {
				std::cerr << "\n x Unable to execute query: " << sqlite3_errmsg (db);
				return;
			}
			sqlite3_reset (stmt_upd);

			// std::cout << "(" << s.shape_dist_traveled << ","
			// 	<< route->get_id () << "," << s.stop->get_id () << "), ";
			// std::cout.flush ();
		}
		sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);

	}
	sqlite3_finalize (stmt_upd);
	sqlite3_close (db);

	// for (auto& s: gtfs.get_stops ()) {
	// 	std::shared_ptr<gtfs::Stop> stop = std::get<1> (s);
	// 	std::cout << "Stop " << stop->get_id () << ": " << stop->get_pos () << "\n";
	// };
};
