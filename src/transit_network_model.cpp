#include <iostream>
#include <memory>
#include <vector>

#include <gtfs.h>

using namespace gtfs;

/**
 * Transit Network Model: a realtime model running indefinitely (while (true) { ... })
 *
 * @param  argc number of command line arguments
 * @param  argv argument vector
 * @return      [description]
 */
int main (int argc, char* argv[]) {

	std::vector<std::unique_ptr<Vehicle> > vehicles;

	for (int i=0; i<3; i++) {
		// Create "temporary" vehicle object
		std::unique_ptr<Vehicle> vp (new Vehicle("CXY" + std::to_string(i)));

		// Work with temporary copy
		std::cout << "Created vehicle " << vp->vehicle_id() << "." << std::endl;

		// Most copy into vector
		vehicles.push_back(std::move(vp));
	}

	std::cout << std::endl;

	int i = 1;
	for (auto& vp: vehicles) {
		printf("Vehicle %d has id %s.\n", i++, vp->vehicle_id().c_str());
		// std::cout <<  << v->vehicle_id();
	}

	std::cout << "\n";
	return 0;
}
