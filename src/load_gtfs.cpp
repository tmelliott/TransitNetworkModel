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

#include <boost/program_options.hpp>
#include <sqlite3.h>

#include "gps.h"

namespace po = boost::program_options;


int system (std::string const& s) { return system (s.c_str ()); }

static int callback(void *data, int argc, char **argv, char **azColName);
void convert_shapes (sqlite3* db);

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

	// // STEP ONE:
	// // run the load_gtfs.sql script to import the txt files -> database
	// std::cout << " * Importing GTFS text files ... ";
	// std::cout.flush();
	// auto so = system("cd " + dir + " && rm -f " + dbname + " && sqlite3 " + dbname + " < load_gtfs.sql");
	// std::cout << "done: " << so << "\n";

	// STEP TWO:
	// convert shapes -> segments
	std::cout << " * Converting shapes to segments ... ";
	// convert_shapes (db); // -- temporary dont let it run (though it should die since shapes_tmp not present)
	std::cout << "\n   ... done.\n";

	// STEP THREE:
	// importing intersections.json and segmenting segments:
	std::cout << " * Importing intersections ... ";

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
