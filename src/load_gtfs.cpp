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
	char *zErrMsg = 0;
   	int rc;
   	char *sql;
   	const char* data = "Callback function called";

	// database is already open
	sql = "SELECT * FROM trips LIMIT 5";
	rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
	if( rc != SQLITE_OK ){
      	fprintf(stderr, "SQL error: %s\n", zErrMsg);
      	sqlite3_free(zErrMsg);
   	} else {
      	fprintf(stdout, "done\n");
   	}
};
