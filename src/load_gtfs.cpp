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

	// STEP ONE:
	// run the load_gtfs.sql script to import the txt files -> database
	std::cout << " * Importing GTFS text files ... ";
	std::cout.flush();
	// system("cd " + dir + " && rm " + dbname + " && sqlite3 " + dbname + " < load_gtfs.sql");
	std::cout << "done.\n";

	// STEP TWO:
	// convert shapes -> segments
	std::cout << " * Converting shapes to segments ... ";
	convert_shapes (db);
	std::cout << "done.\n";

	return 0;
	// STEP THREE:
	// importing intersections.json and segmenting segments:
	std::cout << " * Importing intersections ... ";

	std::cout << "done.\n";

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
  sqlite3_stmt* stmt_seg_ins;
  sqlite3_stmt* stmt_get_seg_id;
        if (sqlite3_prepare_v2 (db, "SELECT DISTINCT shape_id FROM shapes_tmp LIMIT 5",
				-1, &stmt, 0) == SQLITE_OK) {
	  std::cout << "\n   - SELECT query prepared ";
	  
	  if (sqlite3_prepare_v2  (db, "INSERT INTO segments (length) VALUES (?); SELECT last_insert_rowid()", -1, &stmt_seg_ins, 0) == SQLITE_OK) {
	    std::cout << "\n   - INSERT query prepared ";
	  } else {
	    std::cerr << "\n   x Unable to prepare INSERT query ";
	  }
	  // if (sqlite3_prepare_v2 (db, "SELECT MAX(segment_id) FROM segments", -1, &stmt_get_seg_id, 0) == SQLITE_OK) {
	  //   std::cout << "\n   - SELECT segment_id query prepared ";
	  // } else {
	  //   std::cerr << "\n   x Unable to prepare SELECT segment_id query ";
	  // }
	  
	  while (sqlite3_step (stmt) == SQLITE_ROW) {
	    std::string shape_id = (char*)sqlite3_column_text (stmt, 0);
	    std::cout << "\n     + " << shape_id;
	    // LOAD SHAPE - COMPUTE LENGTH
	    double length = 0;
	    
	    // INSERT INTO segments (length) VALUES (?) - [length]
	    int segment_id;
	    if (sqlite3_bind_double (stmt_seg_ins, 1, length) &&
		sqlite3_step (stmt_seg_ins) == SQLITE_ROW) {
	      // if (sqlite3_step(stmt_get_seg_id) == SQLITE_ROW) {
	      // 	segment_id = sqlite3_column_int (stmt_get_seg_id, 1);
	      // }
	      segment_id = sqlite3_column_int (stmt_seg_ins, 1);
	    }
	    sqlite3_reset (stmt_seg_ins);
	    // sqlite3_reset (stmt_get_seg_id);
	    
	    // RETURN ID of new segment
	    
	    std::cout << " -> segment_id: " << segment_id;

	    // INSERT INTO shapes (shape_id, leg, segment_id, shape_dist_traveled)
	    //      VALUES (?, 0, ?, 0) - [shape_id, segment_id]
	    
	  }
	  std::cout << "\n   ";
	} else {
	  std::cerr << "unable to prepare query.\n";
	}
};
