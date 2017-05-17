#include <string>
#include <iostream>
#include <memory>

#include "gtfs.h"

namespace gtfs {
	/**
	* Create a Vehicle object with given ID
	* @param vehicle_id the ID of the vehicle as given in the GTFS feed
	*/
	Vehicle::Vehicle (std::string id) : Vehicle::Vehicle (id, 5) {};

	Vehicle::Vehicle (std::string id, unsigned int n) :
	id (id), n_particles (n), next_id (1) {
		std::cout << " ++ Created vehicle " << id << std::endl;

		particles.reserve(n_particles);
		for (unsigned int i=0; i<n_particles; i++) {
			particles.emplace_back(this);
		}
	};

	/**
	* Desctructor for a vehicle object, ensuring all particles are deleted too.
	*/
	Vehicle::~Vehicle() {
		std::cout << " -- Vehicle " << id << " deleted!!" << std::endl;
	};

	// -- GETTERS

	/**
	* @return ID of vehicle
	*/
	std::string Vehicle::get_id () const {
		return id;
	};

	/**
	* @return vector of particle references (so they can be modifed...)
	*/
	std::vector<gtfs::Particle>& Vehicle::get_particles () {
		return particles;
	};




	// --- METHODS

	/**
	 * To ensure particles have unique ID's (within a vehicle),
	 * the next ID is incremended when requested by a new particle.
	 *
	 * @return the ID for the next particle.
	 */
	unsigned long Vehicle::allocate_id () {
		return next_id++;
	};

	/**
	 * Perform weighted resampling with replacement.
	 *
	 * Use the computed particle weights to resample, with replacement,
	 * the particles associated with the vehicle.
	 */
	void Vehicle::resample () {
		// How many of each particle to keep?
		std::vector<int> pkeep ({1, 3, 2, 1, 1});

		// Move old particles into temporary holding vector
		std::vector<gtfs::Particle> old_particles = std::move(particles);

		// Copy new particles, incrementing their IDs (copy constructor does this)
		particles.reserve(n_particles);
		for (auto& i: pkeep) {
			particles.push_back(old_particles[i]);
		}
	}

}; // end namespace gtfs
