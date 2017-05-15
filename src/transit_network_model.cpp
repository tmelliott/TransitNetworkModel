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

	int i = 0;
	std::vector<gtfs::Vehicle> vehicles;

	while (i < 3) {
		std::cout << "=============== " << i << std::endl;
		vehicles.emplace_back("CXY" + std::to_string(i));
		gtfs::Vehicle *vp = &vehicles[0];
		std::cout << "Created vehicle " << vp->vehicle_id() << "." << std::endl;
		i++;
		std::cout << "---" << std::endl;
	}

	return 0;

}
