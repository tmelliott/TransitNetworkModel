/**
 * \mainpage
 * A realtime model of a public transport network.
 *
 * An program which runs indefinitely, modeling the realtime state of
 * all vehicles in the transit network. These are in turn used to model
 * the realtime state of the network itself (road speeds),
 * and finally arrival time predictions made for each vehicle/stop combination.
 *
 * - transit_network_model.cpp
 * - load_gtfs.cpp
 *
 * @file
 * @author Tom Elliott <tom.elliott@auckland.ac.nz>
 * @version 0.0.1
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <time.h>
#include <sqlite3.h>

#include <boost/program_options.hpp>

#include "gtfs-realtime.pb.h"
#include "sampling.h"
#include "gtfs.h"
#include "gps.h"

namespace po = boost::program_options;

int load_gtfs_database (std::string dbname, std::string version,
						std::unordered_map<std::string, gtfs::Trip> *trips,
						std::unordered_map<std::string, gtfs::Route> *routes,
						std::unordered_map<std::string, gtfs::Shape> *shapes,
						std::unordered_map<std::string, gtfs::Segment> *segments);
bool load_feed (std::unordered_map<std::string, std::unique_ptr<gtfs::Vehicle> > &vs,
				std::string &feed_file, int N, sampling::RNG &rng);


/**
 * Transit Network Model: a realtime model running indefinitely (while (true) { ... })
 *
 * Cycles through latest vehicles in the Realtime Feed, and updates/creates accordingly.
 *
 * @param  argc number of command line arguments
 * @param  argv argument vector
 * @return int 0 (although this will never happen because of the while forever loop)
 */
int main (int argc, char* argv[]) {

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// PROGRAM COMMAND LINE ARGUMNETS
	po::options_description desc ("Allowed options");

	/** vehicle positions file */
	std::vector<std::string> files;
	/** database connection to use */
	std::string dbname;
	std::string version;
	/** number of particles per vehicle */
	int N;

	desc.add_options ()
		("files", po::value<std::vector<std::string> >(&files)->multitoken (),
			"GTFS Realtime protobuf feed files.")
		("database", po::value<std::string>(&dbname), "Database Connection to use.")
		("version", po::value<std::string>(&version), "Version number to pull subset from database.")
		("N", po::value<int>(&N)->default_value(1000), "Number of particles to initialize each vehicle.")
		("help", "Print this message and exit.")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count ("help")) {
		std::cout << desc << "\n";
		return 1;
	}

	if (!vm.count ("files")) {
		std::cerr << "No file specified.\nUse --files to specify protobuf feed files.\n";
		return -1;
	}

	if (!vm.count ("database")) {
	    std::cerr << "No database specified. Use --database to select a SQLIte database.\n";
		return -1;
	}
	if (!vm.count ("version")) {
		std::cout << "WARNING: version number not specified; entire database will be loaded!\n";
		version = "";
	}

	// Prepare a set of pointers to GTFS "tables"
	// Trip -> Route -> Shape -> Segment  [[ initialize in reverse order ]]
	std::unordered_map<std::string, gtfs::Segment> segments;
	std::unordered_map<std::string, gtfs::Shape> shapes;
	std::unordered_map<std::string, gtfs::Route> routes;
	std::unordered_map<std::string, gtfs::Trip> trips;

	// LOAD database into memory - returns -1 if it fails
	if ( load_gtfs_database (dbname, version, &trips, &routes, &shapes, &segments) ) {
	    std::cerr << " * Unable to load GTFS database.\n";
		return -1;
	}
	std::cout << " * Database loaded into memory:\n";

	std::cout << "   - " << routes.size () << " routes\n";
	std::cout << "   - " << trips.size () << " trips\n";


	return 0;
	/**
	 * An unordered map of vehicles.
	 *
	 * vehicles["VEHICLE_ID"] -> object for vehicle with ID VEHICLE_ID
	 */
	std::unordered_map<std::string, std::unique_ptr<gtfs::Vehicle> > vehicles;
	sampling::RNG rng;
	// bool forever = true;

	int i = 2;
	while (i>0) {
		{
			// Load GTFS feed -> vehicles
			for (auto file: files) {
				if ( ! load_feed (vehicles, file, N, rng) ) {
					std::cerr << "Unable to read file.\n";
					return -1;
				}
			}

			// -> triggers particle transition -> resample
			for (auto& v: vehicles) v.second->update ();
		}

		{
			// Update road segments -> Kalman filter

		}

		{
			// Update ETA predictions

		}
		i--;
	}

	return 0;
}


/**
 * Loads GTFS tables from a given GTFS database into memory.
 *
 * The tables are stored in unordered maps that can be accessed
 * by `trip_id` key.
 *
 * @param  dbname   [description]
 * @param  trips    [description]
 * @param  routes   [description]
 * @param  shapes   [description]
 * @param  segments [description]
 * @return          [description]
 */
int load_gtfs_database (std::string dbname,std::string version,
						std::unordered_map<std::string, gtfs::Trip> *trips,
						std::unordered_map<std::string, gtfs::Route> *routes,
						std::unordered_map<std::string, gtfs::Shape> *shapes,
						std::unordered_map<std::string, gtfs::Segment> *segments) {
    sqlite3 *db;
	if (sqlite3_open (dbname.c_str (), &db)) {
	    std::cerr << " * Can't open database: " << sqlite3_errmsg (db) << "\n";
		return -1;
	} else {
	    std::cout << " * Connected to database \"" << dbname << "\"\n";
	}

	std::string qry;

	// Load all gtfs `routes` into Routes*
	sqlite3_stmt* stmt_routes;
	qry = "SELECT route_id, route_short_name, route_long_name FROM routes";
	if (version != "") qry += " WHERE route_id LIKE '%_v" + version + "'";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &stmt_routes, 0) != SQLITE_OK) {
		std::cerr << " * Can't prepare query " << qry << "\n";
		return -1;
	}
	std::cout << " * [prepared] " << qry << "\n";
	while (sqlite3_step (stmt_routes) == SQLITE_ROW) {
		std::string route_id = (char*)sqlite3_column_text (stmt_routes, 0);
		std::string short_name = (char*)sqlite3_column_text (stmt_routes, 1);
		std::string long_name = (char*)sqlite3_column_text (stmt_routes, 2);
		routes->insert (make_pair (route_id, gtfs::Route(route_id, short_name, long_name)));
	}


	// Load all gtfs `trips` into Trips*
	sqlite3_stmt* stmt_trips;
	qry = "SELECT trip_id, route_id FROM trips";
	if (version != "") qry += " WHERE trip_id LIKE '%_v" + version + "'";
	if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &stmt_trips, 0) != SQLITE_OK) {
	    std::cerr << " * Can't prepare query " << qry << "\n";
		return -1;
	}
	std::cout << " * [prepared] " << qry << "\n";
	while (sqlite3_step (stmt_trips) == SQLITE_ROW) {
		// Load that trip into memory: [id, (route)]
		std::string trip_id = (char*)sqlite3_column_text (stmt_trips, 0);
		std::string route_id = (char*)sqlite3_column_text (stmt_trips, 1);
		auto route = routes->find ("00254-20170503090230_v54.6");
		// std::cout << typeid(route).name() << "\n";
		std::cout << typeid(route->second).name () << "\n";
		trips->emplace (trip_id, trip_id);
	}
	std::cout << "\n";


	return 0;
}



/**
 * Load a feed message into vehicle object vector
 * @param vs        reference to vector of vehicle pointers
 * @param feed_file reference to feed
 * @param N         the number of particle to initialze new vehicles with
 * @param rng       reference to a random number generator
 */
bool load_feed (std::unordered_map<std::string, std::unique_ptr<gtfs::Vehicle> > &vs,
				std::string &feed_file, int N, sampling::RNG &rng) {
	transit_realtime::FeedMessage feed;
	std::cout << "Checking for vehicle updates in feed: " << feed_file << " ... ";
	std::fstream feed_in (feed_file, std::ios::in | std::ios::binary);
	if (!feed_in) {
		std::cerr << "file not found!\n";
		return false;
	} else if (!feed.ParseFromIstream (&feed_in)) {
		std::cerr << "failed to parse GTFS realtime feed!\n";
		return false;
	} else {
		std::cout << "done -> " << feed.entity_size () << " updates loaded.\n";
	}

	// Cycle through feed entities and update associated vehicles, or create a new one.

	for (int i=0; i<feed.entity_size (); i++) {
		printf(" * Processing feed: %*d%%\r", 3, (int)(100 * (i+1) / feed.entity_size ()));
		std::cout.flush ();
		auto& ent = feed.entity (i);
		std::string vid;
		if (ent.has_trip_update () && ent.trip_update ().has_vehicle ()) {
			vid = ent.trip_update ().vehicle ().id ();
		} else if (ent.has_vehicle () && ent.vehicle ().has_vehicle ()) {
			vid = ent.vehicle().vehicle ().id ();
		}
		if (vs.find (vid) == vs.end ()) {
			// vehicle doesn't already exist - create it
			vs.emplace (vid, std::unique_ptr<gtfs::Vehicle> (new gtfs::Vehicle (vid, N, rng)));
		}
		if (ent.has_vehicle ()) vs[vid]->update (ent.vehicle ());
		if (ent.has_trip_update ()) vs[vid]->update (ent.trip_update ());
	}
	std::cout << "\n";

	return true;
}
