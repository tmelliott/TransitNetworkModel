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
#include <memory>
#include <vector>
#include <string>
#include <time.h>

#include "gtfs-realtime.pb.h"
#include "sampling.h"
#include "gtfs.h"
#include "gps.h"

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

	std::vector<std::unique_ptr<gtfs::Vehicle> > vehicles;

	bool forever = false;

	while (forever) {
		{
			// Load GTFS feed -> vehicles
			//
			// -> triggers particle transition -> resample
		}

		{
			// Update road segments -> Kalman filter
		}

		{
			// Update ETA predictions
		}
	}

	{
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
			vp->resample();
			std::cout << "  -----------<<" << std::endl;
			for (auto& pr: vp->get_particles ()) {
				std::cout << " |- Particle " << pr.get_id ()
					<< " is a child of particle " << pr.get_parent_id () << std::endl;
			}
			std::cout << ">>----------- (resample)" << std::endl;
			vp->resample();
			std::cout << "  -----------<<" << std::endl;
			for (auto& pr: vp->get_particles ()) {
				std::cout << " |- Particle " << pr.get_id ()
					<< " is a child of particle " << pr.get_parent_id () << std::endl;
			}

			std::cout << std::endl;
		}

		std::cout << "\n";
	}

	{
		std::cout << "-------------- Testing Sampling:\n";
		sampling::RNG rng (time(NULL));

		std::cout << "       5 random uniform numbers: ";
		for (int i=0; i<5; i++) {
			std::cout << rng.runif () << " ";
		}
		std::cout << "\n  5 more random uniform numbers: ";
		for (int i=0; i<5; i++) {
			std::cout << rng.runif () << " ";
		}
		// std::cout << "\n 5 random uniform[5,10] numbers: ";
		// for (int i=0; i<5; i++) {
		// 	std::cout << rng.runif (5.0, 10.0) << " ";
		// }
		std::cout << "\n-------------------------------\n\n";
	}

	return 0;
}
