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

#include <boost/program_options.hpp>

#include "gtfs-realtime.pb.h"
#include "sampling.h"
#include "gtfs.h"
#include "gps.h"

namespace po = boost::program_options;

bool load_feed (std::unordered_map<std::string, std::unique_ptr<gtfs::Vehicle> > &vs,
				std::string &feed_file);


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
	std::string db;

	desc.add_options ()
		("files", po::value<std::vector<std::string> >(&files)->multitoken (),
			"GTFS Realtime protobuf feed files.")
		("database", po::value<std::string>(&db), "Database Connection to use.")
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
		std::cerr << "No file specified.\n";
		return -1;
	}


	// LOAD database into memory
	// ...

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
				if ( ! load_feed (vehicles, file) ) {
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
 * Load a feed message into vehicle object vector
 * @param vs   reference to vector of vehicle pointers
 * @param feed reference to feed
 */
bool load_feed (std::unordered_map<std::string, std::unique_ptr<gtfs::Vehicle> > &vs,
				std::string &feed_file) {
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
	for (auto& ent: feed.entity ()) {
		std::string vid = ent.vehicle().vehicle ().id ();
		if (vs.find (vid) == vs.end ()) {
			std::cout << "Creating new vehicle " << vid << "\n";
			vs.emplace (vid, std::unique_ptr<gtfs::Vehicle> (new gtfs::Vehicle (vid)));
		}

		if (ent.has_vehicle ()) vs[vid]->update (ent.vehicle ());
		if (ent.has_trip_update ()) vs[vid]->update (ent.trip_update ());
	}

	return true;
}
