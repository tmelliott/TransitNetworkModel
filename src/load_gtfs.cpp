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

#include <boost/program_options.hpp>
#include <sqlite3.h>

#include "gps.h"

#include "json.hpp"

namespace po = boost::program_options;


int system (std::string const& s) { return system (s.c_str ()); }
void convert_shapes (sqlite3* db);
void import_intersections (sqlite3* db, std::vector<std::string> files);

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
	std::cout << "Loading GTFS data into database `" << dbname << "`\n";
	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;

	rc = sqlite3_open ((dir + "/" + dbname).c_str(), &db);
	if (rc) {
		fprintf(stderr, " * Can't open database: %s\n", sqlite3_errmsg(db));
      	return(0);
	} else {
    	fprintf(stderr, " * Opened database successfully\n");
	}

	// STEP TWO:
	// convert shapes -> segments
	std::cout << " * Converting shapes to segments ... ";
	// convert_shapes (db); // -- temporary dont let it run (though it should die since shapes_tmp not present)
	std::cout << "\n   ... done.\n";

	// STEP THREE:
	// importing intersections.json and segmenting segments:
	std::cout << " * Importing intersections ... ";
	std::vector<std::string> files {//dir + "/data/intersections_trafficlights.json",
									dir + "/data/intersections_roundabouts.json"};
	import_intersections (db, files);
	std::cout << "done.\n";

	// !! --- TOM: before you do anything, BACKUP gtfs.db -> gtfs.db-backup

	return 0;
	// Get all segments, and split into more segments
	for (int i=0;i<1000;i++) {
		printf(" * Segmenting shapes ... %*d%%\r", 3, (i+1)/1000 * 100);
		std::cout.flush ();


	}

	std::cout << " * Segmenting shapes ... done.\n";



	return 0;
}


static int callback(void *data, int argc, char **argv, char **azColName) {
	int i;
   	fprintf(stderr, "%s\n", (const char*)data);
   	for(i=0; i<argc; i++){
      	printf("   + %s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   	}
   	printf("\n");
   	return 0;
};

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
			printf("     + Shape %*d of %d (%*d%%) \r", 4, n, nshapes, 3, (int)(100 * (++n) / nshapes));
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
		for (int i=1; i<path.size(); i++) {
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
		for (int i=0; i<path.size (); i++) {
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

	std::ofstream f("ints.csv");
	f << "lat,lng\n";
	for (auto i: ints) f << i.lat << "," << i.lng << "\n";
	f.close ();

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

	f.open("distmat.txt");
	for (auto r: distmat) {
		for (auto c: r) f << c << " ";
		f << "\n";
	}
	f.close ();

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

	f.open("dmat.txt");
	for (auto r: dmat) {
		for (auto c: r) f << c << " ";
		f << "\n";
	}
	f.close ();

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
		for (int j=0; j<xi.size (); j++) {
			for (int k=0; k<N; k++) {
				if (dmat[k][xi[j]]) xj.push_back (k);
			}
		}
		if (xj.size () > 0) {
			for (int j=0; j<xj.size (); j++) for (int k=0; k<N; k++) dmat[xj[j]][k] = false;
			y.emplace_back (xj);
		}
	}
	std::cout << "   + Identifying clusters ... done.\n";

	// Compute means and unify intersection clusters
	std::cout << "   + Create new intersections in the middle of clusters ... ";
	std::cout.flush ();
	for (int i=0; i<y.size (); i++) {
		auto newint = gps::Coord(0, 0);
		for (int j=0; j<y[i].size (); j++) {
			newint.lat += ints[wkeep[y[i][j]]].lat;
			newint.lng += ints[wkeep[y[i][j]]].lng;
		}
		newint.lat = newint.lat / y[i].size ();
		newint.lng = newint.lng / y[i].size ();
		ints[wkeep[y[i][1]]] = newint;
		for (int j=1; j<y[i].size (); j++) {
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
