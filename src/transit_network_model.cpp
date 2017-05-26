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

bool load_feed (std::unordered_map<std::string, std::unique_ptr<gtfs::Vehicle> > &vs,
				std::string &feed_file, int N, sampling::RNG &rng,
				gtfs::GTFS &gtfs);


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

	// Load the global GTFS database object:
	gtfs::GTFS gtfs (dbname, version);
	std::cout << " * Database loaded into memory:\n";
	std::cout << "   - " << gtfs.get_routes ().size () << " routes\n";
	std::cout << "   - " << gtfs.get_trips ().size () << " trips\n";
	std::cout << "   - " << gtfs.get_shapes ().size () << " shapes\n";

	for (auto si: gtfs.get_shapes ()) {
		auto s = si.second;
		std::cout << "Shape " << s->get_id () << " has "
			<< s->get_segments ().size () << " segments:\n";
	}


	/**
	 * An unordered map of vehicles.
	 *
	 * vehicles["VEHICLE_ID"] -> object for vehicle with ID VEHICLE_ID
	 */
	std::unordered_map<std::string, std::unique_ptr<gtfs::Vehicle> > vehicles;
	sampling::RNG rng;
	bool forever = true;

	// Initialize vehicles wiht _1
	std::vector<std::string> fs {"vehicle_locations.pbz", "trip_updates.pbz"};
	for (auto file: fs) {
		if ( ! load_feed (vehicles, file, N, rng, gtfs) ) {
			std::cerr << "Unable to read file.\n";
			return -1;
		}
	}

	while (forever) {
		forever = false;
		{
			// Load GTFS feed -> vehicles
			for (auto file: files) {
				if ( ! load_feed (vehicles, file, N, rng, gtfs) ) {
					std::cerr << "Unable to read file.\n";
					return -1;
				}
			}

			// -> triggers particle transition -> resample
			// for (auto& v: vehicles) v.second->update ();
		}

		{
			// Update road segments -> Kalman filter

		}

		{
			// Update ETA predictions

		}
	}

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
				std::string &feed_file, int N, sampling::RNG &rng,
				gtfs::GTFS &gtfs) {
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
		if (ent.has_vehicle ()) vs[vid]->update (ent.vehicle (), gtfs);
		if (ent.has_trip_update ()) vs[vid]->update (ent.trip_update (), gtfs);
	}
	std::cout << "\n";

	return true;
}
