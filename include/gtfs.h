#ifndef GTFS_H
#define GTFS_H

#include <string>
#include <iostream>

namespace gtfs {
	class Vehicle;
	class Particle;

	class Vehicle {
	private:
		std::string vehicle_id_;

	public:
		Vehicle (std::string vehicle_id) :
			vehicle_id_ (vehicle_id) {}

		/**
		 * Getter for vehicle_id
		 * @return ID of vehicle, std::string
		 */
		const std::string vehicle_id () const {
			return vehicle_id_;
		};

		~Vehicle(void);
	};

	/**
	 * Destructor for vehicle class => deletes all associated particles.
	 */
	Vehicle::~Vehicle(void) {
		std::cout << "Vehicle " << vehicle_id_ << " deleted." << std::endl;
	}
};

#endif
