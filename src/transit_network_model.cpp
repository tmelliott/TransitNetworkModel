#include <iostream>
#include <vector>

#include <gtfs.h>

/**
 * Transit Network Model: a realtime model running indefinitely (while (true) { ... })
 *
 * @param  argc number of command line arguments
 * @param  argv argument vector
 * @return      [description]
 */
int main (int argc, char* argv[]) {

	std::cout << "Hello World!" << std::endl;

	std::vector<gtfs::Vehicle> vehicles;

	for (int i=0; i<3; i++) {
		std::cout << "=============== " << i << std::endl;
		vehicles.emplace_back("CXY" + std::to_string(i));
		gtfs::Vehicle *vp = &vehicles[0];
		std::cout << "Created vehicle " << vp->vehicle_id() << "." << std::endl;
		std::cout << "---" << std::endl << std::endl;
	}

	return 0;

}
