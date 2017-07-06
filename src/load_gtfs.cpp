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
void import_intersections (sqlite3* db, std::vector<std::string> files);
void set_distances (sqlite3* db);

/**
 * A split object, used only to find intersections at which
 * to split the route.
 */
struct Split {
	int id;
	gps::Coord at;
	Split (int id, gps::Coord at) : id (id), at (at) {};
};
std::vector<Split> find_intersections (std::shared_ptr<gtfs::Shape> shape,
									   std::vector<gtfs::Intersection>& intersections);

void segment_shapes (sqlite3* db);
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

	// Prepare database if it needs to be ...
	if (!std::ifstream ("../gtfs-backup1.db")) {
		std::cout << " * Importing raw GTFS into database ...\n";
		system ("cd .. && rm gtfs.db && sqlite3 gtfs.db < load_gtfs.sql && cp gtfs.db gtfs-backup1.db");
	} else {
		system ("cp ../gtfs-backup1.db ../gtfs.db");
	}

	// STEP ONE:
	// connect to the SQLite database:
	std::cout << "Loading GTFS data into database `" << dbname << "`\n";
	sqlite3 *db;

	if (sqlite3_open ((dir + "/" + dbname).c_str(), &db)) {
		fprintf(stderr, " * Can't open database: %s\n", sqlite3_errmsg(db));
      	return(0);
	} else {
    	fprintf(stderr, " * Opened database successfully\n");
	}


	// // STEP TWO:
	// importing intersections.json and segmenting segments:
	if (!std::ifstream ("../gtfs-backup2.db")) {
		std::cout << " * Importing intersections ... ";
		std::vector<std::string> files {dir + "/data/intersections_trafficlights.json",
										dir + "/data/intersections_roundabouts.json"};
		import_intersections (db, files);
		system ("cp ../gtfs.db ../gtfs-backup2.db");
		std::cout << "done.\n";
	} else {
		system ("cp ../gtfs-backup2.db ../gtfs.db");
	}

	// STEP THREE:
	// Step through SHAPES identifying intersections
	// and saving in `shape_segments` table
	if (!std::ifstream ("../gtfs-backup3.db")) {
		set_distances (db);
		// segment_shapes (db);
		// system ("cp ../gtfs.db ../gtfs-backup3.db");
	}

	// // That's enough of the database connection ...
	sqlite3_close (db);

	std::cout << "\n   ... done.\n";

	return 0;
}



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
			sqlite3_errmsg (db);
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
 * Set distance in database for segments and stops
 *
 * To reduce reads, perform all at once, route by route.
 * For each route
 * 	- load associated shape data into a Shape object
 * 	- find intersections (in path order)
 * 		- (use code from segment_shape fn)
 * 	- load stops
 * 	- travel along path computing distance traveled for each stop
 * 	  and intersection along the way
 *
 * @param db the database to use
 */
void set_distances (sqlite3* db) {
	std::string qry; // generic string for whatevery query we're preparing

	// SELECT all intersections and stick 'em in a vector of objects
	sqlite3_stmt* select_intersections;
	qry = "SELECT intersection_id, lat, lng, type FROM intersections";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_intersections, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";

	// Insert 'em into the vector
	std::cout << "\n * Loading Intersections ... ";
	std::vector<gtfs::Intersection> intersections;
	while (sqlite3_step (select_intersections) == SQLITE_ROW) {
		std::string type = (char*)sqlite3_column_text (select_intersections, 3);
		intersections.emplace_back (
			sqlite3_column_int (select_intersections, 0),
			gps::Coord (sqlite3_column_double (select_intersections, 1),
						sqlite3_column_double (select_intersections, 2)),
			type
		);
	}
	std::cout << intersections.size () << " loaded";
	std::cout.flush ();

	// SELECT all route IDs and their shape IDs to loop over.
	sqlite3_stmt* select_routes;
	qry = "SELECT route_id, shape_id FROM routes WHERE route_id LIKE '%_v54.27' LIMIT 20";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_routes, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";

	// SELECT shape [shape_id]
	sqlite3_stmt* select_shape;
	qry = "SELECT lat, lng, seq FROM shapes WHERE shape_id=$1";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_shape, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";

	// DELETE shape [shape_id]
	sqlite3_stmt* delete_shape;
	qry = "DELETE FROM shapes WHERE shape_id=$1";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &delete_shape, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";

	// INSERT shape + distance [shape_id, seq, lat, lng, dist_traveled]
	sqlite3_stmt* insert_shape;
	qry = "INSERT INTO shapes (shape_id,seq,lat,lng,dist_traveled) VALUES ($1,$2,$3,$4,$5)";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &insert_shape, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";

	// SELECT all stops
	sqlite3_stmt* select_stops;
	qry = "SELECT stop_id, lat, lng FROM stops";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_stops, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";


	// SELECT stops (just use FIRST trip_id for each route) [route_id]
	sqlite3_stmt* select_rstops;
	qry = "SELECT stop_id FROM stop_times "
		  "WHERE trip_id=(SELECT trip_id FROM trips WHERE route_id=$1 LIMIT 1) "
		  "ORDER BY stop_sequence";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_rstops, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";

	// UPDATE stop distances (for ALL trips) [distance, stop_id, route_id]
	sqlite3_stmt* update_stops;
	qry = "UPDATE stop_times "
			"SET shape_dist_traveled=$1 "
			"WHERE stop_id=$2 AND trip_id IN "
				"(SELECT trip_id FROM trips WHERE route_id=$3)";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &update_stops, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";

	// SELECT segment - check if it exists, and return id
	sqlite3_stmt* select_segment0;
	qry = "SELECT segment_id FROM segments "
		"WHERE from_id=$1 AND to_id=$2";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_segment0, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";

	sqlite3_stmt* select_segment1;
	qry = "SELECT segment_id FROM segments "
		"WHERE start_at=$1 AND to_id=$2";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_segment1, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";

	sqlite3_stmt* select_segment2;
	qry = "SELECT segment_id FROM segments "
		"WHERE from_id=$1 AND end_at=$2";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_segment2, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";

	sqlite3_stmt* select_segment3;
	qry = "SELECT segment_id FROM segments "
		"WHERE start_at=$1 AND end_at=$2";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &select_segment3, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";


	// INSERT segment - these are created between two intersections/stops
	sqlite3_stmt* insert_segment;
	qry = "INSERT INTO segments (from_id,to_id,start_at,end_at) "
		"VALUES ($1,$2,$3,$4)";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &insert_segment, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";

	// INSERT shape_segments
	sqlite3_stmt* insert_shapeseg;
	qry = "INSERT INTO shape_segments (shape_id,segment_id,leg,shape_dist_traveled) "
		"VALUES ($1,$2,$3,$4)";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &insert_shapeseg, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare `" << qry << "`";
		throw "Unable to prepare query - invalid, perhaps?";
	}
	std::clog << "\n + Prepared `" << qry << "`";

	std::string shapeid, routeid;
	std::vector<gtfs::ShapePt> path;
	std::vector<gtfs::Shape> shapes;
	std::map<std::string, std::shared_ptr<gtfs::Stop> > stops; // for ALL stops

	std::cout << "\n * Loading stops ...";
	while (sqlite3_step (select_stops) == SQLITE_ROW) {
		std::string stop_id = (char*)sqlite3_column_text (select_stops, 0);
		gps::Coord pos (sqlite3_column_double (select_stops, 1),
						sqlite3_column_double (select_stops, 2));
		std::shared_ptr<gtfs::Stop> stop (new gtfs::Stop(stop_id, pos));
		stops.emplace (stop_id, stop);
	}
	std::cout << " done.";

	std::cout << "\n * Loading Shapes ... ";
	while (sqlite3_step (select_routes) == SQLITE_ROW) {
		// fetch the shape
		routeid = (char*)sqlite3_column_text (select_routes, 0);
		shapeid = (char*)sqlite3_column_text (select_routes, 1);
		std::cout << "\n    - loading shape " << shapeid;
		if (sqlite3_bind_text (select_shape, 1, shapeid.c_str (), -1, SQLITE_STATIC) != SQLITE_OK) {
			std::cerr << "\n x Unable to bind shape_id to SELECT shape query";
			throw "Unable to bind shape_id to query  :(";
		}
		double dist (0.0);
		while (sqlite3_step (select_shape) == SQLITE_ROW) {
			auto npt = gps::Coord (sqlite3_column_double (select_shape, 0),
								   sqlite3_column_double (select_shape, 1));
			if (path.size () > 0) {
				dist += path.back ().pt.distanceTo (npt);
			}
			path.emplace_back (npt, dist);
		}
		sqlite3_reset (select_shape);

		auto shape = std::shared_ptr<gtfs::Shape> (new gtfs::Shape (shapeid, path));
		path.clear ();

		// Figure out shape's stops ...
		if (sqlite3_bind_text (select_rstops, 1, routeid.c_str (), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
			std::cerr << "\n x Unable to bind route_id to SELECT route stops query";
			throw "Unable to bind route_id to query  :(";
		}
		std::vector<gtfs::RouteStop> rstops;
		while (sqlite3_step (select_rstops) == SQLITE_ROW) {
			std::string stopid = (char*)sqlite3_column_text (select_rstops, 0);
			auto si = stops.find (stopid);
			if (si == stops.end ()) {
				std::cerr << "\n x Umm... the stops aren't loaded properly.";
				throw "Improper stop requested ...";
			}
			rstops.emplace_back (si->second);
		}
		sqlite3_reset (select_rstops);
		std::cout << " - " << rstops.size () << " stops loaded.";

		// Figure out shape's segments ... use a function 'cause its complicated
		auto split_at = find_intersections (shape, intersections);
		std::cout << " -> " << split_at.size () << " split points";
		// for (auto& s: split_at) {
		// 	std::cout << std::setprecision (10) << "\n" << s.at.lat << "," << s.at.lng;
		// }

		// Create segment objects
		std::vector<gtfs::Segment> segments; // create new ones as necessary
		segments.reserve (split_at.size () + 1);
		for (unsigned int i=0; i<=split_at.size (); i++) {
			unsigned long segid;
			// first, try find the segment ...
			if (split_at.size () == 0) {
				// stop -> stop
				if (sqlite3_bind_text (select_segment3, 1, rstops[0].stop->get_id ().c_str (),
									   -1, SQLITE_STATIC) != SQLITE_OK ||
				    sqlite3_bind_text (select_segment3, 1, rstops.back ().stop->get_id ().c_str (),
									   -1, SQLITE_STATIC) != SQLITE_OK) {
					std::cerr << "\n x Unable to bind values to SELECT segment query";
					throw "Unable to bind values to query";
				}
				auto qr = sqlite3_step (select_segment3);
				if (qr == SQLITE_DONE) {
					// segment doesn't exist
					if (sqlite3_bind_null (insert_segment, 1) != SQLITE_OK ||
						sqlite3_bind_null (insert_segment, 2) != SQLITE_OK ||
						sqlite3_bind_text (insert_segment, 3, rstops[0].stop->get_id ().c_str (),
										   -1, SQLITE_STATIC) != SQLITE_OK ||
						sqlite3_bind_text (insert_segment, 3, rstops.back ().stop->get_id ().c_str (),
										   -1, SQLITE_STATIC) != SQLITE_OK) {
						std::cerr << "\n x Unable to bind values to INSERT/SELECT segment query";
						throw "Unable to bind values to query";
					}
					if (sqlite3_step (insert_segment) != SQLITE_DONE) {
						std::cerr << "\n x Unable to run INSERT/SELECT segment query";
						throw "Unable to run query";
					}
					sqlite3_reset (insert_segment);

					sqlite3_reset (select_segment3);
					if (sqlite3_bind_text (select_segment3, 1, rstops[0].stop->get_id ().c_str (),
										   -1, SQLITE_STATIC) != SQLITE_OK ||
					    sqlite3_bind_text (select_segment3, 1, rstops.back ().stop->get_id ().c_str (),
										   -1, SQLITE_STATIC) != SQLITE_OK) {
						std::cerr << "\n x Unable to bind values to SELECT segment query";
						throw "Unable to bind values to query";
					}
					auto q2 = sqlite3_step (select_segment3);
					if (q2 == SQLITE_DONE) {
						std::cout << "(fail to insert/select) ";
					} else if (q2 != SQLITE_ROW) {
						std::cerr << "\n x Something went wrong trying to insert and select new segment";
						throw "Unable to insert and select new segment";
					}
					segid = sqlite3_column_int (select_segment3, 0);
				} else if (qr == SQLITE_ROW) {
					// segment exists - fetch it!
					segid = sqlite3_column_int (select_segment3, 0);
				} else {
					// query didn't work :(
					std::cerr << "\n x Unable to run SELECT segment query";
					throw "Unable to run query";
				}
				sqlite3_reset (select_segment3);

				segments.emplace_back (segid, rstops[0].stop, rstops.back ().stop, 0);
			} else if (i == 0) {
				// stop -> intersection
				if (sqlite3_bind_text (select_segment1, 1, rstops[0].stop->get_id ().c_str (),
									   -1, SQLITE_STATIC) != SQLITE_OK ||
					sqlite3_bind_int (select_segment1, 2, split_at[i].id) != SQLITE_OK) {
					std::cerr << "\n x Unable to bind values to SELECT segment query";
					throw "Unable to bind values to query";
				}
				auto qr = sqlite3_step (select_segment1);
				if (qr == SQLITE_DONE) {
					// segment doesn't exist
					if (sqlite3_bind_null (insert_segment, 1) != SQLITE_OK ||
						sqlite3_bind_int (insert_segment, 2, split_at[i].id) != SQLITE_OK ||
						sqlite3_bind_text (insert_segment, 3, rstops[0].stop->get_id ().c_str (),
										   -1, SQLITE_STATIC) != SQLITE_OK ||
						sqlite3_bind_null (insert_segment, 4) != SQLITE_OK) {
						std::cerr << "\n x Unable to bind values to INSERT/SELECT segment query";
						throw "Unable to bind values to query";
					}
					if (sqlite3_step (insert_segment) != SQLITE_DONE) {
						std::cerr << "\n x Unable to run INSERT/SELECT segment query";
						throw "Unable to run query";
					}
					sqlite3_reset (insert_segment);

					sqlite3_reset (select_segment1);
					if (sqlite3_bind_text (select_segment1, 1, rstops[0].stop->get_id ().c_str (),
										   -1, SQLITE_STATIC) != SQLITE_OK ||
						sqlite3_bind_int (select_segment1, 2, split_at[i].id) != SQLITE_OK) {
						std::cerr << "\n x Unable to bind values to SELECT segment query";
						throw "Unable to bind values to query";
					}
					auto q2 = sqlite3_step (select_segment1);
					if (q2 == SQLITE_DONE) {
						std::cout << "(fail to insert/select) ";
					} else if (q2 != SQLITE_ROW) {
						std::cerr << "\n x Something went wrong trying to insert and select new segment";
						throw "Unable to insert and select new segment";
					}
					segid = sqlite3_column_int (select_segment1, 0);
				} else if (qr == SQLITE_ROW) {
					// segment exists - fetch it!
					segid = sqlite3_column_int (select_segment1, 0);
				} else {
					// query didn't work :(
					std::cerr << "\n x Unable to run SELECT segment query";
					throw "Unable to run query";
				}
				sqlite3_reset (select_segment1);

				unsigned int intid1 = split_at[i].id;
				auto int1 = find_if (intersections.begin (), intersections.end(),
					[&intid1](const gtfs::Intersection& it) { return it.get_id () == intid1; });
				if (int1 == intersections.end ()) std::cerr << "\n x Couldn't find the intersection.";

				std::shared_ptr<gtfs::Intersection> i1 = std::make_shared<gtfs::Intersection> (int1[0]);
				segments.emplace_back (segid, rstops[0].stop, i1, 0);
			} else if (i == split_at.size ()) {
				// intersection -> stop
				if (sqlite3_bind_int (select_segment2, 1, split_at[i-1].id) != SQLITE_OK ||
					sqlite3_bind_text (select_segment2, 2, rstops.back ().stop->get_id ().c_str (),
									   -1, SQLITE_STATIC) != SQLITE_OK) {
					std::cerr << "\n x Unable to bind values to SELECT segment query";
					throw "Unable to bind values to query";
				}
				auto qr = sqlite3_step (select_segment2);
				if (qr == SQLITE_DONE) {
					// segment doesn't exist
					if (sqlite3_bind_int (insert_segment, 1, split_at[i-1].id) != SQLITE_OK ||
						sqlite3_bind_null (insert_segment, 2) != SQLITE_OK ||
						sqlite3_bind_null (insert_segment, 3) != SQLITE_OK ||
						sqlite3_bind_text (insert_segment, 4, rstops.back ().stop->get_id ().c_str (),
										   -1, SQLITE_STATIC) != SQLITE_OK) {
						std::cerr << "\n x Unable to bind values to INSERT/SELECT segment query";
						throw "Unable to bind values to query";
					}
					if (sqlite3_step (insert_segment) != SQLITE_DONE) {
						std::cerr << "\n x Unable to run INSERT/SELECT segment query";
						throw "Unable to run query";
					}
					sqlite3_reset (insert_segment);

					sqlite3_reset (select_segment2);
					if (sqlite3_bind_int (select_segment2, 1, split_at[i-1].id) != SQLITE_OK ||
						sqlite3_bind_text (select_segment2, 2, rstops.back ().stop->get_id ().c_str (),
										   -1, SQLITE_STATIC) != SQLITE_OK) {
						std::cerr << "\n x Unable to bind values to SELECT segment query";
						throw "Unable to bind values to query";
					}
					auto q2 = sqlite3_step (select_segment2);
					if (q2 == SQLITE_DONE) {
						std::cout << "(fail to insert/select) ";
					} else if (q2 != SQLITE_ROW) {
						std::cerr << "\n x Something went wrong trying to insert and select new segment";
						throw "Unable to insert and select new segment";
					}
					segid = sqlite3_column_int (select_segment2, 0);
				} else if (qr == SQLITE_ROW) {
					// segment exists - fetch it!
					segid = sqlite3_column_int (select_segment2, 0);
				} else {
					// query didn't work :(
					std::cerr << "\n x Unable to run SELECT segment query";
					throw "Unable to run query";
				}
				sqlite3_reset (select_segment2);

				unsigned int intid1 = split_at[i-1].id;
				auto int1 = find_if (intersections.begin (), intersections.end(),
					[&intid1](const gtfs::Intersection& it) { return it.get_id () == intid1; });
				if (int1 == intersections.end ()) std::cerr << "\n x Couldn't find the intersection.";

				std::shared_ptr<gtfs::Intersection> i1 = std::make_shared<gtfs::Intersection> (int1[0]);
				segments.emplace_back (segid, i1, rstops.back ().stop, 0);
			} else {
				// intersection -> intersection
				if (sqlite3_bind_int (select_segment0, 1, split_at[i-1].id) != SQLITE_OK ||
					sqlite3_bind_int (select_segment0, 2, split_at[i].id) != SQLITE_OK) {
					std::cerr << "\n x Unable to bind values to SELECT segment query";
					throw "Unable to bind values to query";
				}
				auto qr = sqlite3_step (select_segment0);
				if (qr == SQLITE_DONE) {
					// segment doesn't exist
					if (sqlite3_bind_int (insert_segment, 1, split_at[i-1].id) != SQLITE_OK ||
						sqlite3_bind_int (insert_segment, 2, split_at[i].id) != SQLITE_OK ||
						sqlite3_bind_null (insert_segment, 3) != SQLITE_OK ||
						sqlite3_bind_null (insert_segment, 4) != SQLITE_OK) {
						std::cerr << "\n x Unable to bind values to INSERT/SELECT segment query";
						throw "Unable to bind values to query";
					}
					if (sqlite3_step (insert_segment) != SQLITE_DONE) {
						std::cerr << "\n x Unable to run INSERT/SELECT segment query";
						throw "Unable to run query";
					}
					sqlite3_reset (insert_segment);

					sqlite3_reset (select_segment0);
					if (sqlite3_bind_int (select_segment0, 1, split_at[i-1].id) != SQLITE_OK ||
						sqlite3_bind_int (select_segment0, 2, split_at[i].id) != SQLITE_OK) {
						std::cerr << "\n x Unable to bind values to SELECT segment query";
						throw "Unable to bind values to query";
					}
					auto q2 = sqlite3_step (select_segment0);
					if (q2 == SQLITE_DONE) {
						std::cout << "(fail to insert/select 3) ";
					} else if (q2 != SQLITE_ROW) {
						std::cerr << "\n x Something went wrong trying to insert and select new segment";
						throw "Unable to insert and select new segment";
					}
					segid = sqlite3_column_int (select_segment0, 0);
				} else if (qr == SQLITE_ROW) {
					// segment exists - fetch it!
					segid = sqlite3_column_int (select_segment0, 0);
				} else {
					// query didn't work :(
					std::cerr << "\n x Unable to run SELECT segment query";
					throw "Unable to run query";
				}
				sqlite3_reset (select_segment0);

				unsigned int intid1 = split_at[i-1].id;
				auto int1 = find_if (intersections.begin (), intersections.end(),
					[&intid1](const gtfs::Intersection& it) { return it.get_id () == intid1; });
				if (int1 == intersections.end ()) std::cerr << "\n x Couldn't find the intersection.";

				unsigned int intid2 = split_at[i].id;
				auto int2 = find_if (intersections.begin (), intersections.end(),
					[&intid2](const gtfs::Intersection& it) { return it.get_id () == intid2; });
				if (int2 == intersections.end ()) std::cerr << "\n x Couldn't find the intersection.";

				// std::cout << int1[0].get_id () << " -> " << int2[0].get_id () << ", ";
				std::shared_ptr<gtfs::Intersection> i1 = std::make_shared<gtfs::Intersection> (int1[0]);
				std::shared_ptr<gtfs::Intersection> i2 = std::make_shared<gtfs::Intersection> (int2[0]);
				segments.emplace_back (segid, i1, i2, 0);
			}
		}
		std::cout << " > formed " << segments.size () << " segments";


		// Now just travel along the route looking for the next stop/intersection,
		// and set it's dist_traveled.
		std::vector<double> seg_ds (segments.size (), 0);
		int stpi (1), segi (1);
		double dtrav = 0;
		for (unsigned int i=1; i<shape->get_path ().size (); i++) {
			auto p1=shape->get_path ()[i-1].pt,
				 p2=shape->get_path ()[i].pt;
			if (p1 == p2) continue;
			double d12 = p1.distanceTo (p2);

			std::vector<gps::Coord> spth;
			spth.push_back (p1);
			spth.push_back (p2);

			// STOPS
			if (stpi < (int)rstops.size ()) {
				auto sipt = rstops[stpi].stop->get_pos ();
				auto np = sipt.nearestPoint (spth);
				// If point is too far away, move on.
				if (np.d <= 40) {
					// if point is AHEAD of p2, move on.
					double sd1 = sipt.alongTrackDistance (p1, p2),
						   sd2 = sipt.alongTrackDistance (p2, p1);
					if (sd1 > d12) {
						// do nothing
					} else if (sd2 > d12) {
						// if point is BEFORE p1, use p1
						rstops[stpi].shape_dist_traveled = dtrav;
						stpi++;
					} else {
						// if point is between p1 and p2, find closest point
						rstops[stpi].shape_dist_traveled = dtrav + sd1;
						stpi++;
					}
				}
			}

			// SEGMENTS (repeat)
			if (segi < (int)segments.size ()) {
				// it's always going to be from an intersection ...
				// gps::Coord& sipt;
				if (!segments[segi].get_from ()) {
					std::cerr << "\n Something bad ........";
				}
				auto sipt = segments[segi].get_from ()->get_pos ();
				auto np = sipt.nearestPoint (spth);
				// If point is too far away, move on.
				if (np.d <= 40) {
					// if point is AHEAD of p2, move on.
					double sd1 = sipt.alongTrackDistance (p1, p2),
						   sd2 = sipt.alongTrackDistance (p2, p1);
					if (sd1 > d12) {
						// do nothing
					} else if (sd2 > d12) {
						// if point is BEFORE p1, use p1
						seg_ds[segi] = dtrav;
						segi++;
					} else {
						// if point is between p1 and p2, find closest point
						seg_ds[segi] = dtrav + sd1;
						segi++;
					}
				}
			}

			dtrav += p1.distanceTo (p2);
		}
		std::cout << " - " << dtrav << "m long";

		std::cout << "\n   -> stops: ";
		for (auto& s: rstops) std::cout << s.shape_dist_traveled << ", ";
		std::cout << "\n   -> segments: ";
		for (auto& s: seg_ds) std::cout << s << ", ";

		// sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
		// for (unsigned int i=0; i<segments.size (); i++) {
		// 	if (sqlite3_bind_string (insert_shapeseg, 1, shapeid) != SQLITE_OK ||
		// 		sqlite3_bind_int (insert_shapeseg, 2, segments[i].get_id ()) != SQLITE_OK ||
		// 		sqlite3_bind_int (insert_shapeseg, 3, i+1) != SQLITE_OK ||
		// 		sqlite3_bind_double (insert_shapeseg, 4, segments[i].get_distance ()) != SQLITE_OK) {
		//
		// 	}
		// }
		// sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);

		shapes.push_back (*shape);
	}
	std::cout << "\n - done\n\n";
	std::cout.flush ();

	// Write updated shapes (i.e., with distances)
	int N = shapes.size ();
	int Ni = 1;
	sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
	for (auto sh: shapes) {
		printf("\r + Updating distances: %*d%%",
			   3, (int) (100 * Ni / N));
		std::cout.flush ();
		Ni++;
		if (sqlite3_bind_text (delete_shape, 1, sh.get_id ().c_str (), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
			std::cerr << "\n x Unable to bind shape_id to DELETE shape query";
			throw "Unable to bind shape_id to DELETE shape query";
		}
		if (sqlite3_step (delete_shape) != SQLITE_DONE) {
			std::cerr << "\n x Unable to delete shape";
			throw "Unable to delete shape";
		}
		sqlite3_reset (delete_shape);

		int i=1;
		for (auto& p: sh.get_path ()) {
			// std::cout << sh.get_id () << "\n";
			if (sqlite3_bind_text (insert_shape, 1, sh.get_id ().c_str (), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
				sqlite3_bind_int (insert_shape, 2, i) != SQLITE_OK ||
				sqlite3_bind_double (insert_shape, 3, p.pt.lat) ||
				sqlite3_bind_double (insert_shape, 4, p.pt.lng) ||
				sqlite3_bind_double (insert_shape, 5, p.dist_traveled) != SQLITE_OK) {
				throw "Unable to bind data to INSERT shape query";
			}
			if (sqlite3_step (insert_shape) != SQLITE_DONE) {
				throw "Unable to run INSERT shape query";
			}
			sqlite3_reset (insert_shape);
			i++;
		}
	}
	sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);


	sqlite3_finalize (select_intersections);
	sqlite3_finalize (select_routes);
	sqlite3_finalize (select_shape);
	sqlite3_finalize (delete_shape);
	sqlite3_finalize (insert_shape);
	sqlite3_finalize (select_stops);
	sqlite3_finalize (select_rstops);
	sqlite3_finalize (update_stops);
	sqlite3_finalize (select_segment0);
	sqlite3_finalize (select_segment1);
	sqlite3_finalize (select_segment2);
	sqlite3_finalize (select_segment3);
	sqlite3_finalize (insert_segment);
	sqlite3_finalize (insert_shapeseg);
	std::clog << "\n --- finished.\n";
};

/**
 * Find intersections along a path (multiple if it loops etc).
 * @param  path          path to look through
 * @param  intersections possible intersections
 * @return               vector of split points
 */
std::vector<Split> find_intersections (std::shared_ptr<gtfs::Shape> shape,
									   std::vector<gtfs::Intersection>& intersections) {
    // OK let's go
	auto path = shape->get_path ();
	std::vector<gps::Coord> shapepts;
	for (auto& p: path) shapepts.emplace_back (p.pt);

	// Compute a bounding box to reduce the number of intersections to check
	double latmin = 90, latmax = -90, lngmin = 180, lngmax = -180;
	for (auto& pt: shapepts) {
		if (pt.lat < latmin) latmin = pt.lat;
		if (pt.lat > latmax) latmax = pt.lat;
		if (pt.lng < lngmin) lngmin = pt.lng;
		if (pt.lng > lngmax) lngmax = pt.lng;
	}

	std::vector<gtfs::Intersection*> ikeep;
	for (auto& it: intersections) {
		auto pt = it.get_pos ();
		if (pt.lat > latmin && pt.lat < latmax &&
			pt.lng > lngmin && pt.lng < lngmax) {
			auto np = pt.nearestPoint (shapepts);
			if (np.d < 40) ikeep.push_back (&it);
		}
	}
	std::cout << " -> found " << ikeep.size () << " intersections.";

	std::vector<Split> splitpts;
	if (ikeep.size () == 0) return splitpts;

	// OK so we've found some intersections - now we gotta order them and stuff
	//
	// Travel along the route, splitting it whenever come to an intersection.
	std::vector<int> x1, x2; // shape index, intersection index  (< 40m)
	for (unsigned int i=1; i<shapepts.size (); i++) {
		auto& p1 = shapepts[i-1], p2 = shapepts[i];
		// if (p1 == p2) continue;

		std::vector<gps::Coord> pseg {p1, p2};
		double closest = 100;
		int cid = -1;
		for (auto it: ikeep) { // ikeep is a vector of Intersection* objects
			auto pt = it->get_pos ();
			auto np = pt.nearestPoint (pseg);
			if (np.d < 40 && np.d < closest) {
				closest = np.d;
				cid = it->get_id ();
			}
		}
		if (cid >= 0 && closest < 40) {
			x1.push_back (i-1);
			x2.push_back (cid);
		}
	}
	x1.push_back (0); // add a zero so we don't need a special end condition
	x2.push_back (0);
	if (x1.size () != x2.size ()) {
		std::cerr << "Something very, very terrible went wrong: "
			<< x1.size () << " + " << x2.size ();
		throw "Something went wrong. Very wrong!";
	}

	// for (unsigned int i=0; i<x1.size (); i++)
	// 	printf("\n [%*d] - %d", 4, x1[i], x2[i]);

	std::vector<gps::Coord> subpath;
	for (unsigned int i=0; i<x1.size ()-1; i++) {
		// so long as shape index is less than 10 smaller than next index,
		// and segment index is the same as the next one, keep going
		if (x1[i] + 10 > x1[i+1] && x2[i] == x2[i+1]) {
			subpath.push_back (shapepts[x1[i]]);
		} else {
			// Otherwise, save this intersection
			subpath.push_back (shapepts[x1[i]]);
			subpath.push_back (shapepts[x1[i]+1]);
			gps::nearPt np;
			for (auto it: ikeep) {
				// step through intersections until we find the one we're after
				if ((int)it->get_id () == x2[i]) {
					auto pt = it->get_pos ();
					// then find the nearest point to it on the shape:
					np = pt.nearestPoint (subpath);
					if (np.d < 40) splitpts.emplace_back (it->get_id (), np.pt);
					break;
				}
			}
			subpath.clear ();
		}
	}

	return splitpts;
};




/**
 * Segment shapes.
 *
 * @param db a database connection
 */
void segment_shapes (sqlite3* db) {
	sqlite3_stmt* stmt_segs;
	if (sqlite3_prepare_v2 (db, "SELECT segment_id FROM segments",
							-1, &stmt_segs, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare SELECT segments";
		return;
	}
	std::clog << "\n + Prepared SELECT segments";

	sqlite3_stmt* stmt_ints;
	if (sqlite3_prepare_v2 (db, "SELECT intersection_id, lat, lng FROM intersections",
							-1, &stmt_ints, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare SELECT intersections";
		return;
	}
	std::clog << "\n + Prepared SELECT intersections ... ";

	struct Int {
		int id;
		gps::Coord pos;
		Int (int id, gps::Coord pos) : id (id), pos (pos) {};
	};
	std::vector<Int> intersections;
	while (sqlite3_step (stmt_ints) == SQLITE_ROW) {
		intersections.emplace_back (sqlite3_column_int (stmt_ints, 0),
									gps::Coord (sqlite3_column_double (stmt_ints, 1),
												sqlite3_column_double (stmt_ints, 2)));
	}
	std::clog << "loaded " << intersections.size () << " intersections";

	sqlite3_stmt* stmt_shape;
	if (sqlite3_prepare_v2 (db, "SELECT lat, lng, seg_dist_traveled FROM segment_pt WHERE segment_id=$1 ORDER BY seg_pt_sequence", -1, &stmt_shape, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare SELECT segment shape points";
		return;
	}
	std::clog << "\n + Prepared SELECT segment shape points";

	std::vector<int> segmentIDs;
	while (sqlite3_step (stmt_segs) == SQLITE_ROW)
		segmentIDs.push_back (sqlite3_column_int (stmt_segs, 0));

	/**
	 * A split object, used only to find intersections at which
	 * to split the route.
	 */
	struct Split {
		int id;
		gps::Coord at;
		Split (int id, gps::Coord at) : id (id), at (at) {};
	};

	// statements to SELECT segment, and insert SEGMENT and SEGMENT_POINTS
	sqlite3_stmt* stmt_segget;
	sqlite3_stmt* stmt_segins;
	sqlite3_stmt* stmt_segID;
	sqlite3_stmt* stmt_ptins;
	sqlite3_stmt* stmt_shapegetid;
	sqlite3_stmt* stmt_shapeget;
	sqlite3_stmt* stmt_shapedel;
	sqlite3_stmt* stmt_shapeins;
	if (sqlite3_prepare_v2 (db, "SELECT segment_id, length FROM segments WHERE start_id=$1 AND end_id=$2",
							-1, &stmt_segget, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare SELECT segment_id query";
		return;
	}
	if (sqlite3_prepare_v2 (db, "INSERT INTO segments (start_id,end_id,length) VALUES ($1,$2,$3)",
							-1, &stmt_segins, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare INSERT segments";
		return;
	}
	if (sqlite3_prepare_v2 (db, "SELECT seq FROM sqlite_sequence WHERE name='segments'",
							-1, &stmt_segID, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare SELECT last ID query";
		return;
	}
	if (sqlite3_prepare_v2 (db, "INSERT INTO segment_pt (segment_id,seg_pt_sequence,lat,lng,seg_dist_traveled) VALUES ($1,$2,$3,$4,$5)", -1, &stmt_ptins, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare INSERT segment_pt";
		return;
	}
	if (sqlite3_prepare_v2 (db, "SELECT DISTINCT shape_id FROM shapes WHERE segment_id=$1",
							-1, &stmt_shapegetid, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare SELECT shape_id query";
		return;
	}
	if (sqlite3_prepare_v2 (db, "SELECT shapes.segment_id, segments.length FROM shapes, segments WHERE shapes.shape_id=$1 AND shapes.segment_id=segments.segment_id ORDER BY leg",
							-1, &stmt_shapeget, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare SELECT shape_id query";
		return;
	}
	if (sqlite3_prepare_v2 (db, "DELETE FROM shapes WHERE shape_id=$1",
							-1, &stmt_shapedel, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare DELETE FROM shapes query";
		return;
	}
	if (sqlite3_prepare_v2 (db, "INSERT INTO shapes (shape_id,leg,segment_id,shape_dist_traveled) VALUES ($1,$2,$3,$4)", -1, &stmt_shapeins, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare INSERT query";
		return;
	}

	// Clean up debugging
	system ("rm -rf tmp/*");


	// Step through each segment and ... segment it further!
	std::cout << "\n * Splitting segments ";
	for (unsigned int segi=0; segi<segmentIDs.size (); segi++) {
		printf("\r * Splitting segments %*d%%",
			   3, (int) (100 * (segi+1) / segmentIDs.size ()));
		std::cout.flush ();

		int segment_id = segmentIDs[segi];

		// Get the shape points
		if (sqlite3_bind_int (stmt_shape, 1, segment_id) != SQLITE_OK) {
			std::cerr << "\r x Unable to bind query for segment " << segment_id << "\n";
			continue;
		}
		std::vector<gps::Coord> shapepts;
		double latmin = 0, latmax = 0, lngmin = 0, lngmax = 0;
		while (sqlite3_step (stmt_shape) == SQLITE_ROW) {
			shapepts.emplace_back (sqlite3_column_double (stmt_shape, 0),
								   sqlite3_column_double (stmt_shape, 1));
			gps::Coord& s = shapepts.back ();
			if (latmin == 0 || s.lat < latmin) latmin = s.lat;
			if (latmax == 0 || s.lat > latmax) latmax = s.lat;
			if (lngmin == 0 || s.lng < lngmin) lngmin = s.lng;
			if (lngmax == 0 || s.lng > lngmax) lngmax = s.lng;
		}
		sqlite3_reset (stmt_shape);

		// DEBUGGING - write original shape to file
		std::string dir ("tmp/segment_" + std::to_string (segment_id));
		system (("mkdir " + dir).c_str ());
		std::ofstream f1;
		f1.open ((dir + "/original.csv").c_str ());
		f1 << std::setprecision (10) << "lat,lng\n";
		for (auto& p: shapepts) f1 << p.lat << "," << p.lng << "\n";
		f1.close ();

		// Find nearby intersections
		// - if in box, find distance to nearest point on route
		std::vector<Int> ikeep;
		for (auto& ipt: intersections) {
			if (ipt.pos.lat < latmin || ipt.pos.lat > latmax ||
				ipt.pos.lng < lngmin || ipt.pos.lng > lngmax) continue;

			auto np = ipt.pos.nearestPoint (shapepts);

			// Keep the intersections that are close
			if (np.d < 40) ikeep.emplace_back (ipt.id, ipt.pos);
		}
		if (ikeep.size () == 0) continue;

		std::ofstream f2;
		f2.open ((dir + "/intersections.csv").c_str ());
		f2 << std::setprecision (10) << "id,lat,lng\n";
		for (auto& ik: ikeep) f2 << ik.id << "," << ik.pos.lat << "," << ik.pos.lng << "\n";
		f2.close ();

		// Now we simply travel along the route, splitting it whenever
		// come to an intersection.
		std::vector<int> x1, x2; // shape index, intersection index  (< 40m)
		for (unsigned int i=1; i<shapepts.size (); i++) {
			auto& p1 = shapepts[i-1], p2 = shapepts[i];
			if (p1 == p2) continue;
			std::vector<gps::Coord> pseg {p1, p2};
			double closest = 100;
			int cid = -1;
			for (auto& ipt: ikeep) {
				auto np = ipt.pos.nearestPoint (pseg);
				if (np.d < 40 && np.d < closest) {
					closest = np.d;
					cid = ipt.id;
				}
			}
			if (cid >= 0 && closest < 40) {
				x1.push_back (i-1);
				x2.push_back (cid);
			}
		}
		x1.push_back (0); // add a zero so we don't need a special end condition
		x2.push_back (0);
		if (x1.size () != x2.size ()) {
			std::cerr << "Something very, very terrible went wrong: "
				<< x1.size () << " + " << x2.size ();
			return;
		}

		std::vector<gps::Coord> path;
		std::vector<Split> splitpoints;
		for (unsigned int i=0; i<x1.size ()-1; i++) {
			// so long as shape index is less than 10 smaller than next index,
			// and segment index is the same as the next one, keep going
			if (x1[i] + 10 > x1[i+1] && x2[i] == x2[i+1]) {
				path.push_back (shapepts[x1[i]]);
			} else {
				// Otherwise, save this intersection
				path.push_back (shapepts[x1[i]]);
				path.push_back (shapepts[x1[i]+1]);
				gps::nearPt np;
				for (auto& ipt: ikeep) {
					// step through intersections until we find the one we're after
					if (ipt.id == x2[i]) {
						// then find the nearest point to it on the shape:
						np = ipt.pos.nearestPoint (path);
						if (np.d < 40) splitpoints.emplace_back (ipt.id, np.pt);
						break;
					}
				}
				path.clear ();
			}
		}
		// KEEP FOR DEBUGGING AND GRAPHS: R/test.R

		std::ofstream f3;
		f3.open ((dir + "/split_points.csv").c_str ());
		f3 << std::setprecision (10) << "id,lat,lng\n";
		for (auto& sp: splitpoints) f3 << sp.id << "," << sp.at.lat << "," << sp.at.lng << "\n";
		f3.close ();

		// Now just stream along the path looking for split points, and split!

		std::clog << "\n * locating " << splitpoints.size () << " splits";
		std::vector<int> segment_ids;
		std::vector<double> segment_lengths;
		segment_ids.reserve (splitpoints.size () + 1);
		segment_lengths.reserve (splitpoints.size () + 1);
		path.clear (); // we can reuse it!
		unsigned int i = 1; // shape index counter
		unsigned int si = segment_ids.size ();

		std::ofstream f4;
		f4.open ((dir + "/segment_shape.csv").c_str ());
		f4 << std::setprecision (10) << "segment_id,lat,lng\n";
		while (segment_ids.size () < splitpoints.size () + 1 && i < shapepts.size ()) {
			auto& p1 = shapepts[i-1], p2 = shapepts[i];
			path.push_back (shapepts[i-1]);
			if (path.size () == 1) {
				i++;
				continue;
			}
			double len=0;
			for (unsigned int j=1; j<path.size (); j++) len += path[j-1].distanceTo (path[j]);
			if (len < 5) {
				i++;
				continue;
			}

			if (p1.distanceTo (splitpoints[si].at) +
				splitpoints[si].at.distanceTo (p2) <= p1.distanceTo (p2) + 1 ||
				si == splitpoints.size ()) {
				// finalise segment
				int startid = 0, endid = 0;
				if (si > 0) startid = splitpoints[si-1].id;
				if (si < splitpoints.size ()) endid = splitpoints[si].id;

				// Find or Create new segment
				if (sqlite3_bind_int (stmt_segget, 1, startid) == SQLITE_OK &&
					sqlite3_bind_int (stmt_segget, 2, endid) == SQLITE_OK &&
					sqlite3_step (stmt_segget) == SQLITE_ROW) {
					segment_ids.push_back (sqlite3_column_int (stmt_segget, 0));
					segment_lengths.push_back (sqlite3_column_double (stmt_segget, 1));
				} else {
					// necessary to create a new segment!
					// - add split point to path (IF its not the last)
					if (si < splitpoints.size ()) {
						path.push_back (splitpoints[si].at);
					} else {
						// add remaining shape to path
						while (i < shapepts.size ()) {
							path.push_back (shapepts[i]);
							i++;
						}
					}

					// - create NEW segment in `segments` table
					double pathlen = 0;
					for (unsigned int j=1; j<path.size (); j++)
						pathlen += path[j-1].distanceTo (path[j]);

					if (startid > 0) {
						if (sqlite3_bind_int (stmt_segins, 1, startid) != SQLITE_OK) {
							std::cerr << "\n x Error binding values (startid)";
							sqlite3_errmsg (db);
							return;
						}
					} else {
						if (sqlite3_bind_null (stmt_segins, 1) != SQLITE_OK) {
							std::cerr << "\n x Error binding values (startnull)";
							sqlite3_errmsg (db);
							return;
						}
					}
					if (endid > 0) {
						if (sqlite3_bind_int (stmt_segins, 2, endid) != SQLITE_OK) {
							std::cerr << "\n x Error binding values (endid)";
							sqlite3_errmsg (db);
							return;
						}
					} else {
						if (sqlite3_bind_null (stmt_segins, 2) != SQLITE_OK) {
							std::cerr << "\n x Error binding values (endnull)";
							sqlite3_errmsg (db);
							return;
						}
					}
					if (sqlite3_bind_double (stmt_segins, 3, pathlen) != SQLITE_OK) {
						std::cerr << "\n x Error binding values (length)";
						sqlite3_errmsg (db);
						return;
					}
					if (sqlite3_step (stmt_segins) != SQLITE_DONE) {
						std::cerr << "\n x Error running INSERT new segment query: ";
						sqlite3_errmsg (db);
						return;
					}
					sqlite3_reset (stmt_segins);

					if (sqlite3_step (stmt_segID) != SQLITE_ROW) {
						std::cerr << "\n x Error fetching inserted ID";
						sqlite3_errmsg (db);
						return;
					}
					// - get ID of new segment
					int sid = sqlite3_column_int (stmt_segID, 0);
					segment_ids.push_back (sid);
					sqlite3_reset (stmt_segID);

					// - insert path into `segment_pt` table
					sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
					double pathdist = 0;
					for (unsigned int j=0; j<path.size (); j++) {
						if (sqlite3_bind_int (stmt_ptins, 1, sid) != SQLITE_OK) {
							std::cerr << "\n x Error binding segment ID";
							return;
						}
						if (sqlite3_bind_int (stmt_ptins, 2, j+1) != SQLITE_OK) {
							std::cerr << "\n x Error binding segment pt sequence";
							return;
						}
						if (sqlite3_bind_double (stmt_ptins, 3, path[j].lat) != SQLITE_OK ||
							sqlite3_bind_double (stmt_ptins, 4, path[j].lng) != SQLITE_OK) {
							std::cerr << "\n x Error binding path coordinate";
							return;
						}
						if (sqlite3_bind_double (stmt_ptins, 5, pathdist) != SQLITE_OK) {
							std::cerr << "\n x Error binding seg dist traveled";
							return;
						}
						if (sqlite3_step (stmt_ptins) != SQLITE_DONE) {
							std::cerr << "\n x Error executing insertion of point query";
							return;
						}
						sqlite3_reset (stmt_ptins);
						if (j + 1 < path.size ()) pathdist += path[j].distanceTo (path[j+1]);
					}
					segment_lengths.push_back (pathdist);
					sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);

					for (auto& p: path) f4 << sid << "," << p.lat << "," << p.lng << "\n";
				}
				sqlite3_reset (stmt_segget);



				// - clear path and start it at splitpoint
				path.clear ();
				path.push_back (splitpoints[si].at);

				std::clog << "\n   + Sement " << si + 1 << ": from ";
				if (si > 0) std::clog << startid;
				else std::clog << "start";
				std::clog << " to ";
				if (si < splitpoints.size ()) std::clog << endid;
				else std::clog << "end";

				si = segment_ids.size ();
				if (si == splitpoints.size () + 1) break;
			}
			i++;
		}
		f4.close ();

		if (segment_ids.size () == splitpoints.size () + 1) {
			// Convert all rows in SHAPES where segment_id = segment_id

			// - START transaction
			sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);

			// - get all SHAPES that include segment_id
			if (sqlite3_bind_int (stmt_shapegetid, 1, segment_id) != SQLITE_OK) {
				std::cerr << "\n x Unable to bind segment ID to query";
				return;
			}
			while (sqlite3_step (stmt_shapegetid) == SQLITE_ROW) {
				auto shapeid = (char*)sqlite3_column_text (stmt_shapegetid, 0);
				// - for each SHAPE, select shape_id, segment_id
				if (sqlite3_bind_text (stmt_shapeget, 1, shapeid, -1, SQLITE_STATIC) != SQLITE_OK) {
					std::cerr << "\n x Unable to bind shape id to query";
					return;
				}
				std::vector<int> shapesegs;
				std::vector<double> shapelens;
				while (sqlite3_step (stmt_shapeget) == SQLITE_ROW) {
					shapesegs.push_back (sqlite3_column_int (stmt_shapeget, 0));
					shapelens.push_back (sqlite3_column_double (stmt_shapeget, 1));
				}
				sqlite3_reset (stmt_shapeget);

				// - DELETE FROM shapes
				if (sqlite3_bind_text (stmt_shapedel, 1, shapeid, -1, SQLITE_STATIC) != SQLITE_OK) {
					std::cerr << "\n x Unable to bind shape id to delete query";
					return;
				}
				if (sqlite3_step (stmt_shapedel) != SQLITE_DONE) {
					std::cerr << "\n x Unable to run DELETE query";
					return;
				}
				sqlite3_reset (stmt_shapedel);

				// - INSERT INTO, replacing segment_id with segment_ids (and recreate leg/distances)
				int legi = 1;
				double shapedist = 0.0;
				for (unsigned int j=0; j<shapesegs.size (); j++) {
					if (shapesegs[j] == segment_id) {
						// CREATE MULTIPLE SEGMENTS
						// printf ("\n   + Row [%s, %d, %d, 0] -> ", shapeid, j+1, shapesegs[j]);
						for (unsigned int k=0; k<segment_ids.size (); k++) {
							int sid = segment_ids[k];
							// printf ("\n       [%*s, %*d, %*d, %*.1fm] (%.1fm), ",
							// 		28, shapeid, 3, legi, 5, sid, 7, shapedist, segment_lengths[k]);
							if (sqlite3_bind_text (stmt_shapeins, 1, shapeid, -1, SQLITE_STATIC) != SQLITE_OK) {
								std::cerr << "\n x Error binding shape id to insert query" <<
									sqlite3_errmsg (db);
								return;
							}
							if (sqlite3_bind_int (stmt_shapeins, 2, legi) != SQLITE_OK) {
								std::cerr << "\n x Error binding leg to insert query" <<
									sqlite3_errmsg (db);
								return;
							}
							if (sqlite3_bind_int (stmt_shapeins, 3, sid) != SQLITE_OK) {
								std::cerr << "\n x Error binding segment_id to query" <<
									sqlite3_errmsg (db);
								return;
							}
							if (sqlite3_bind_double (stmt_shapeins, 4, shapedist) != SQLITE_OK) {
								std::cerr << "\n x Error binding distance to query" <<
									sqlite3_errmsg (db);
							}
							if (sqlite3_step (stmt_shapeins) != SQLITE_DONE) {
								std::cerr << "\n x Error running INSERT query: " <<
									sqlite3_errmsg (db);
								return;
							}

							sqlite3_reset (stmt_shapeins);
							legi++;
							shapedist += segment_lengths[k];
						}
					} else {
						// JUST RECREATE THE OLD ONE (updated)
						// printf ("\n   o Segment %d, leg %d -> %d",
						// 		shapesegs[j], j+1, legi);
						if (sqlite3_bind_text (stmt_shapeins, 1, shapeid, -1, SQLITE_STATIC) != SQLITE_OK) {
							std::cerr << "\n x Error binding shape id to insert query" <<
								sqlite3_errmsg (db);
							return;
						}
						if (sqlite3_bind_int (stmt_shapeins, 2, legi) != SQLITE_OK) {
							std::cerr << "\n x Error binding leg to insert query" <<
								sqlite3_errmsg (db);
							return;
						}
						if (sqlite3_bind_int (stmt_shapeins, 3, shapesegs[j]) != SQLITE_OK) {
							std::cerr << "\n x Error binding segment_id to query" <<
								sqlite3_errmsg (db);
							return;
						}
						if (sqlite3_bind_double (stmt_shapeins, 4, shapedist) != SQLITE_OK) {
							std::cerr << "\n x Error binding distance to query" <<
								sqlite3_errmsg (db);
							return;
						}
						if (sqlite3_step (stmt_shapeins) != SQLITE_OK) {
							std::cerr << "\n x Error running INSERT query" <<
								sqlite3_errmsg (db);
							return;
						}

						sqlite3_reset (stmt_shapeins);
						legi++;
						shapedist += shapelens[j];
					}
				}
			}
			sqlite3_reset (stmt_shapegetid);


			// - COMMIT transation
			sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);
		} else {
			std::cerr << "\n x created the wrong number of segments (" << segment_ids.size () << ")";
		}

	}  // END while (step)
	std::cout << " - done!\n";

	sqlite3_finalize (stmt_shapegetid);
	sqlite3_finalize (stmt_shapeget);
	sqlite3_finalize (stmt_shapedel);
	sqlite3_finalize (stmt_shapeins);
	sqlite3_finalize (stmt_segget);
	sqlite3_finalize (stmt_segins);
	sqlite3_finalize (stmt_segID);
	sqlite3_finalize (stmt_ptins);
	sqlite3_finalize (stmt_shape);
	sqlite3_finalize (stmt_segs);
};



// /**
//  * Compute the shape distance traveled for each stop along a route.
//  * @param dbname The database to connect to
//  */
// void calculate_stop_distances (std::string& dbname) {
// 	std::string v = "54.27";
// 	gtfs::GTFS gtfs (dbname, v);
// 	std::cout << " * GTFS database loaded     ";
//
//
// 	sqlite3* db;
// 	if (sqlite3_open (dbname.c_str (), &db)) {
// 		fprintf(stderr, " * Can't open database: %s\n", sqlite3_errmsg(db));
// 		return;
// 	}
// 	std::cout << " \n* Connected to database ";
//
// 	sqlite3_stmt* stmt_upd;
// 	if (sqlite3_prepare_v2 (db, "UPDATE stop_times SET shape_dist_traveled=$1 WHERE trip_id IN (SELECT trip_id FROM trips WHERE route_id=$2) AND stop_id=$3",
// 		  					-1, &stmt_upd, 0) != SQLITE_OK) {
// 		std::cerr << " x Unable to prepare UPDATE query :(";
// 		return;
// 	}
//
// 	auto Nr = gtfs.get_routes ().size ();
// 	unsigned int Ni = 0;
// 	std::cout << "\n* Computing stop distances into trip "
// 	<< "(" << Nr << " routes)\n";
// 	// For each route
// 	for (auto& r: gtfs.get_routes ()) {
// 		Ni++;
// 		printf("  %*d%%\r", 3, (int)(100 * Ni / Nr));
// 		std::cout.flush ();
//
// 		std::shared_ptr<gtfs::Route> route = std::get<1> (r);
// 		std::cout << "\n   - looking at route " << route->get_id ();
// 		auto shape = route->get_shape ();
// 		if (!shape) {
// 			std::cout << " - no shape";
// 			continue; // no shape associated with this route
// 		}
//
// 		auto stops = route->get_stops ();
// 		if (stops.size () == 0) {
// 			std::cout << " - no stops";
// 			continue; // No stops - skip!
// 		}
//
// 		auto segments = shape->get_segments ();
// 		if (segments.size () == 0) {
// 			std::cout << " - no segments";
// 			continue;
// 		}
//
// 		std::cout << ", with " << stops.size () << " stops\n";
//
// 		std::ofstream f1;
// 		f1.open (("tmp/" + route->get_id () + ".csv").c_str ());
// 		f1 << std::setprecision(10) << "lat,lng\n";
// 		for (auto& st: stops) f1 << st.stop->get_pos ().lat << "," << st.stop->get_pos ().lng << "\n";
//
// 		// Travel along the route until the next stop is between STOP and STOP+1
// 		unsigned int si = 0;
// 		std::cout << "   - looking through " << segments.size () << " segments ...";
// 		for (auto& s: segments) {
// 			auto seg = s.segment;
// 			if (!seg) break;
// 			// std::cout << "\n     - segment " << s.segment->get_id ()
// 				// << ": " << s.shape_dist_traveled << " m";
// 			auto path = seg->get_path (); // sequence of ShapePt structs
// 			if (path.size () == 0) break;
// 			double dmin (s.shape_dist_traveled);
//
// 			// Check the first point in the segment (likely only for first stop)
// 			if (stops[si].stop->get_pos ().distanceTo (path[0].pt) < 1) {
// 				stops[si].shape_dist_traveled = dmin;// + path[0].seg_dist_traveled;
// 				printf("\n - Stop %*d [%*s]: %*d m\n",
// 					   3, si+1,
// 					   6, stops[si].stop->get_id ().c_str (),
// 					   6, (int)stops[si].shape_dist_traveled);
// 				si ++;
// 				if (si == stops.size ()) break;
// 			}
// 			// step through path
// 			for (unsigned int i=1; i<path.size (); i++) {
// 				if (path[i-1].pt == path[i].pt) continue;
// 				// ---Consider stop is AT the point if distance < 1m (rounding error!)
// 				// FIRST check stop I
// 				if (stops[si].stop->get_pos ().distanceTo (path[i].pt) < 1) {
// 					stops[si].shape_dist_traveled = dmin + path[i].seg_dist_traveled;
// 					printf("\n - Stop %*d [%*s]: %*d m\n",
// 						   3, si+1,
// 						   6, stops[si].stop->get_id ().c_str (),
// 						   6, (int)stops[si].shape_dist_traveled);
// 					si ++;
// 				} else if (stops[si].stop->get_pos ().alongTrackDistance (path[i-1].pt, path[i].pt) <
// 							path[i-1].pt.distanceTo (path[i].pt) &&
// 						   stops[si].stop->get_pos ().alongTrackDistance (path[i].pt, path[i-1].pt) <
// 						    // path[i-1].pt.distanceTo (path[i].pt) + 1 &&
// 							path[i].pt.distanceTo (path[i-1].pt) &&
// 						   abs(stops[si].stop->get_pos ().crossTrackDistanceTo (path[i-1].pt, path[i].pt)) < 30) {
// 					// std::cout << "\n  => stop: " << stops[si].stop->get_pos ()
// 					// 	<< " -> from path: " << path[i-1].pt << "->" << path[i].pt
// 					// 	<< "; CTD = " << stops[si].stop->get_pos ().crossTrackDistanceTo (path[i-1].pt, path[i].pt)
// 					// 	<< "; ATDs = " << stops[si].stop->get_pos ().alongTrackDistance (path[i-1].pt, path[i].pt)
// 					// 	<< " and " << stops[si].stop->get_pos ().alongTrackDistance (path[i].pt, path[i-1].pt) << "!";
// 					stops[si].shape_dist_traveled = dmin + path[i-1].seg_dist_traveled +
// 						stops[si].stop->get_pos ().alongTrackDistance (path[i-1].pt, path[i].pt);
// 					printf("\n - Stop %*d [%*s]?: %*d m - based on %dth point in segment shape, which is %.1f + %.1f = %.1fm into trip\n",
// 						   3, si+1,
// 						   6, stops[si].stop->get_id ().c_str (),
// 						   6, (int)stops[si].shape_dist_traveled,
// 						   i-1, dmin, path[i-1].seg_dist_traveled, dmin + path[i-1].seg_dist_traveled);
// 					si ++;
// 				}
// 				if (si+1 == stops.size ()) break;
// 			}
// 			if (si+1 == stops.size ()) break;
// 		}
// 		// Last stop is at end of line
// 		stops[si].shape_dist_traveled = segments.back ().shape_dist_traveled +
// 			segments.back ().segment->get_length ();
//
// 		if (si+1 != stops.size ()) {
// 			std::cout << "\n x didn't find all stops ...";
// 			break; // unable to get distance for all stops
// 		}
//
// 		// Now save the distances into the database for each trip
// 		sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
// 		std::cout << "\n +++ Storing in DB ...\n";
// 		for (auto& s: stops) {
// 			if (sqlite3_bind_double (stmt_upd, 1, s.shape_dist_traveled) != SQLITE_OK ||
// 				sqlite3_bind_text (stmt_upd, 2, route->get_id ().c_str (), -1, SQLITE_STATIC) != SQLITE_OK ||
// 				sqlite3_bind_text (stmt_upd, 3, s.stop->get_id ().c_str (), -1, SQLITE_STATIC) != SQLITE_OK) {
// 				std::cerr << "\n x Unable to bind parameters.";
// 				return;
// 			}
// 			if (sqlite3_step (stmt_upd) != SQLITE_DONE) {
// 				std::cerr << "\n x Unable to execute query: " << sqlite3_errmsg (db);
// 				return;
// 			}
// 			sqlite3_reset (stmt_upd);
//
// 			std::cout << "(" << s.shape_dist_traveled << ","
// 				<< route->get_id () << "," << s.stop->get_id () << "), ";
// 			std::cout.flush ();
// 		}
// 		sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);
// 		std::cout << " ... done!!!\n\n";
//
// 	}
// 	sqlite3_finalize (stmt_upd);
// 	sqlite3_close (db);
//
// 	// for (auto& s: gtfs.get_stops ()) {
// 	// 	std::shared_ptr<gtfs::Stop> stop = std::get<1> (s);
// 	// 	std::cout << "Stop " << stop->get_id () << ": " << stop->get_pos () << "\n";
// 	// };
// };
