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
#include <time.h>

#include <boost/program_options.hpp>

#include "gtfs-realtime.pb.h"
#include "sampling.h"
#include "gtfs.h"
#include "gps.h"

namespace po = boost::program_options;

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
	std::string vpf;

	desc.add_options ()
		("positions", po::value<std::string>(&vpf)->default_value("vehicle_locations.pb"),
			"Vehicle positions protobuf file.")
		("help", "Print this message and exit.")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count ("help")) {
		std::cout << desc << "\n";
		return 1;
	}

	// std::unique_ptr<gtfs::Vehicle> v;
	std::vector<std::unique_ptr<gtfs::Vehicle> > vehicles;
	sampling::RNG rng;

	bool forever = true;

	while (forever) {
		{
			// Load GTFS feed -> vehicles
			//
			// -> triggers particle transition -> resample
			transit_realtime::FeedMessage vp_feed;
			std::cout << "Checking for vehicle locations feed: " << vpf << " ... ";
			std::fstream vp_in (vpf, std::ios::in | std::ios::binary);
			if (!vp_in) {
				std::cerr << "file not found!\n";
				return -1;
			} else if (!vp_feed.ParseFromIstream (&vp_in)) {
				std::cerr << "failed to parse GTFS realtime feed!\n";
				return -1;
			} else {
				std::cout << "done -> " << vp_feed.entity_size () << " vehicle locations loaded.\n";
			}

			for (auto& vp: vp_feed.entity ()) {
				// std::cout << ".";
			}
			// std::cout << "\n";
		}

		{
			// Update road segments -> Kalman filter

		}

		{
			// Update ETA predictions

		}

		forever = false;
	}

	/**{
		for (int i=0; i<3; i++) {
			// Create unique vehicle object
			std::unique_ptr<gtfs::Vehicle> vp (new gtfs::Vehicle("CXY" + std::to_string(i)));

			// Work with pointer


			// Most pointer into vector
			vehicles.push_back(std::move(vp));

			std::cout << std::endl;
		}

		std::cout << std::endl;

		int i = 1;
		for (auto& vp: vehicles) {
			printf("Vehicle %d has id %s (%d particles).\n", i++, vp->get_id ().c_str(), (int) vp->get_particles().size ());
			for (auto& pr: vp->get_particles ()) {
				std::cout << " |- Particle " << pr.get_id () << std::endl;
			}
			std::cout << ">>----------- (resample)" << std::endl;
			vp->resample(rng);
			std::cout << "  -----------<<" << std::endl;
			for (auto& pr: vp->get_particles ()) {
				std::cout << " |- Particle " << pr.get_id ()
					<< " is a child of particle " << pr.get_parent_id () << std::endl;
			}
			std::cout << ">>----------- (resample)" << std::endl;
			vp->resample(rng);
			std::cout << "  -----------<<" << std::endl;
			for (auto& pr: vp->get_particles ()) {
				std::cout << " |- Particle " << pr.get_id ()
					<< " is a child of particle " << pr.get_parent_id () << std::endl;
			}

			std::cout << std::endl;
		}

		std::cout << "\n";
	}**/

	{

		// std::cout << "Making samples ...\n\n";

		// std::cout << "Non-weighted: ";
		sampling::sample smp (10);
		// auto si = smp.get (rng);
		// for (auto i: si) std::cout << i << ", ";
		// std::cout << "\n";

		// std::cout << "    Weighted: ";
		// sampling::sample smp2 ({0.8, 0.2, 0.3, 0.1, 1.0});
		// auto si2 = smp2.get (rng);
		// for (auto i: si2) std::cout << i << ", ";
		// std::cout << "\n\n";

	}

	return 0;
}
