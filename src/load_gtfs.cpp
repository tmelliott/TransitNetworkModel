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
		system ("cd .. && rm -f gtfs.db && sqlite3 gtfs.db < load_gtfs.sql && cp gtfs.db gtfs-backup1.db");
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
		// Temporary fix for the domestic airport problem
		sqlite3_exec (db, "DELETE FROM intersections WHERE intersection_id=708", NULL, NULL, NULL);
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
		system ("cp ../gtfs.db ../gtfs-backup3.db");
	} else {
		system ("cp ../gtfs-backup3.db ../gtfs.db");
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
	qry = "SELECT route_id, shape_id FROM routes "
		"WHERE shape_id NOT IN "
		"(select distinct shape_id from shape_segments)";
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
			"WHERE stop_id=$2 AND stop_sequence=$3 AND trip_id IN "
				"(SELECT trip_id FROM trips WHERE route_id=$4)";
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
	std::cout.flush ();

	sqlite3_stmt* nshapes;
	int Nshapes = 0;
	if (sqlite3_prepare_v2 (db, "SELECT count(*) FROM routes",
							-1, &nshapes, 0) == SQLITE_OK &&
		sqlite3_step (nshapes) == SQLITE_ROW) {
		Nshapes = sqlite3_column_int (nshapes, 0);
	}
	sqlite3_finalize (nshapes);

	std::cout << "\n * Loading Shapes ";
	std::cout.flush ();
	int Nsi = 1;
	while (sqlite3_step (select_routes) == SQLITE_ROW) {
		if (Nshapes > 0) {
			printf ("\r * Loading Shapes (%*d / %d) %*d%%", 4, Nsi, Nshapes, 3, (int) (100 * Nsi / Nshapes));
			std::cout.flush ();
			Nsi++;
		} else {

		}
		// fetch the shape
		routeid = (char*)sqlite3_column_text (select_routes, 0);
		shapeid = (char*)sqlite3_column_text (select_routes, 1);
		std::clog << "\n    - loading shape " << shapeid;
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
		std::clog << " - " << rstops.size () << " stops loaded.";

		// Figure out shape's segments ... use a function 'cause its complicated
		auto split_at = find_intersections (shape, intersections);
		std::clog << " -> " << split_at.size () << " split points";
		for (auto& s: split_at) {
			std::clog << std::setprecision (10) << "\n" << s.at.lat << "," << s.at.lng;
		}

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
		std::clog << " > formed " << segments.size () << " segments";


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
				if (sipt == p1) {
					rstops[stpi].shape_dist_traveled = dtrav;
					stpi++;
				} else if (sipt == p2) {
					rstops[stpi].shape_dist_traveled = dtrav + d12;
					stpi++;
				} else {
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
			}

			// SEGMENTS (repeat)
			if (segi < (int)segments.size () &&
				(segi == 1 || dtrav > seg_ds.back () + 50)) {
				// it's always going to be from an intersection ...
				// gps::Coord& sipt;
				if (!segments[segi].get_from ()) {
					std::cerr << "\n Something bad ........";
				}
				// auto sipt = segments[segi].get_from ()->get_pos ();
				auto sipt = split_at[segi-1].at;
				if (sipt == p2) {
					seg_ds[segi] = dtrav + d12;
					// std::clog << "\n  >>(1) segment "
						// << segi << " - " << seg_ds[segi];
					segi++;
				} else if (sipt == p1) {
					seg_ds[segi] = dtrav;
					// std::clog << "\n  >>(2) segment "
						// << segi << " - " << seg_ds[segi];
					segi++;
				// } else if (sipt.crossTrackDistanceTo (p1, p2) <= 10) {
				} else {
					auto np = sipt.nearestPoint (spth);
					// If point is too far away, move on.
					if (np.d <= 1) {
						// if point is AHEAD of p2, move on.
						double sd1 = sipt.alongTrackDistance (p1, p2),
							   sd2 = sipt.alongTrackDistance (p2, p1);
						if (sd1 < d12 && sd2 < d12 && sd1 >= 0 && sd2 >= 0) {
							seg_ds[segi] = dtrav + sd1;
							// std::clog << "\n  >>(3) segment "
							// 	<< segi << " - " << seg_ds[segi];
							segi++;
						}
					}
					// if (sd1 > d12) {
					// 	// do nothing
					// } else if (sd2 > d12) {
					// 	// if point is BEFORE p1, use p1
					// 	seg_ds[segi] = dtrav;
					// 	segi++;
					// } else {
					// 	// if point is between p1 and p2, find closest point
					// 	seg_ds[segi] = dtrav + sd1;
					// 	segi++;
					// }
				}
			}

			dtrav += d12;
		}
		rstops.back ().shape_dist_traveled = dtrav;
		std::clog << " - " << dtrav << "m long";

		std::clog << "\n   -> stops: ";
		for (auto& s: rstops) std::clog << s.shape_dist_traveled << ", ";
		std::clog << "\n   -> segments: ";
		for (auto& s: seg_ds) std::clog << s << ", ";

		sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
		for (unsigned int i=0; i<rstops.size (); i++) {
			if (sqlite3_bind_double (update_stops, 1, rstops[i].shape_dist_traveled) != SQLITE_OK ||
				sqlite3_bind_text (update_stops, 2, rstops[i].stop->get_id ().c_str (), -1, SQLITE_STATIC) != SQLITE_OK ||
				sqlite3_bind_int (update_stops, 3, i+1) != SQLITE_OK ||
				sqlite3_bind_text (update_stops, 4, routeid.c_str (), -1, SQLITE_STATIC) != SQLITE_OK) {
				std::cerr << "\n x Error binding values to update stops query: "
					<< sqlite3_errmsg (db);
				throw "Error binding values ...";
			}

			// printf("\n  > UPDATE stop_times SET shape_dist_traveled=%.2f WHERE stop_id='%s' AND stop_sequence=%d AND trip_id IN (SELECT trip_id FROM trips WHERE route_id='%s')", rstops[i].shape_dist_traveled, rstops[i].stop->get_id ().c_str (), i+1, routeid.c_str ());
			if (sqlite3_step (update_stops) != SQLITE_DONE) {
				std::cerr << "\n x Error running update stops query: "
					<< sqlite3_errmsg (db);
				throw "Error running query ...";
			}
			sqlite3_reset (update_stops);
		}

		if (seg_ds.size () == 1 || seg_ds.back () > 0) {
			for (unsigned int i=0; i<segments.size (); i++) {
				if (sqlite3_bind_text (insert_shapeseg, 1, shapeid.c_str (), -1, SQLITE_STATIC) != SQLITE_OK ||
					sqlite3_bind_int (insert_shapeseg, 2, segments[i].get_id ()) != SQLITE_OK ||
					sqlite3_bind_int (insert_shapeseg, 3, i+1) != SQLITE_OK ||
					sqlite3_bind_double (insert_shapeseg, 4, seg_ds[i]) != SQLITE_OK) {
					std::cerr << "\n x Error binding values to insert shape segments query: "
						<< sqlite3_errmsg (db);
					throw "Error binding values ...";
				}
				if (sqlite3_step (insert_shapeseg) != SQLITE_DONE) {
					std::cerr << "\n x Error running insert shape segments query: "
						<< sqlite3_errmsg (db);
					throw "Error running query ...";
				}
				sqlite3_reset (insert_shapeseg);
			}
		}
		sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);

		shapes.push_back (*shape);
		std::clog << "\n";
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
	// std::cout << " -> found " << ikeep.size () << " intersections.";

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
					if (np.d < 40) {
						// if (np.pt == shapepts[0] && np.d > 1) break;
						splitpts.emplace_back (it->get_id (), np.pt);
					};
					break;
				}
			}
			subpath.clear ();
		}
	}

	if (splitpts[0].at.distanceTo (shapepts[0]) < 40) {
		// too close to start
		splitpts.erase (splitpts.begin ());
	}

	return splitpts;
};
