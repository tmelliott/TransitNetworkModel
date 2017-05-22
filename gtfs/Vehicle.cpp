#include <iostream>
#include <memory>

#include "gtfs.h"

namespace gtfs {
	/**
	* Create a Vehicle object with given ID.
	*
	* Vehicles are created with a default number of particles.
	*
	* @param vehicle_id the ID of the vehicle as given in the GTFS feed
	*/
	Vehicle::Vehicle (std::string id) : Vehicle::Vehicle (id, 5) {};

	/**
	 * Create a vehicle with specified number of particles, and ID.
	 *
	 * @param id the ID of the vehicle as given in the GTFS feed
	 * @param n  integer specifying the number of particles to initialize
	 *           the vehicle with
	 */
	Vehicle::Vehicle (std::string id, unsigned int n) :
	id (id), n_particles (n), next_id (1) {
		std::clog << " ++ Created vehicle " << id << std::endl;

		particles.reserve(n_particles);
		for (unsigned int i=0; i<n_particles; i++) {
			particles.emplace_back(this);
		}
	};

	/**
	* Desctructor for a vehicle object, ensuring all particles are deleted too.
	*/
	Vehicle::~Vehicle() {
		std::clog << " -- Vehicle " << id << " deleted!!" << std::endl;
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

	void Vehicle::update ( void ) {
		std::clog << "Updating particles!\n";
	}

	/**
	 * Update the location of the vehicle object.
	 *
	 * This does NOT trigger a particle update, as we may need
	 * to also insert TripUpdates later.
	 * Check that the trip_id is the same, otherwise set `newtrip = false`
	 *
	 * @param vp a vehicle position from the realtime feed
	 */
	void Vehicle::update (const transit_realtime::VehiclePosition &vp) {
		std::clog << "Updating vehicle location!\n";
	};

	/**
	 * Add Stop Time Updates to the vehicle object.
	 *
	 * This does NOT trigger a particle update, as we may need
	 * to also insert VehiclePositions later.
	 *
	 * @param vp a vehicle position from the realtime feed
	 */
	void Vehicle::update (const transit_realtime::TripUpdate &vp) {
		std::clog << "Updating vehicle trip update!\n";

	};

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
	void Vehicle::resample (sampling::RNG &rng) {
		// Re-sampler based on computed weights:
		sampling::sample smp ({0.1, 0.2, 0.1, 0.1, 0.5});
		std::vector<int> pkeep (smp.get (rng));

		// Move old particles into temporary holding vector
		std::vector<gtfs::Particle> old_particles = std::move(particles);

		// Copy new particles, incrementing their IDs (copy constructor does this)
		particles.reserve(n_particles);
		for (auto& i: pkeep) {
			particles.push_back(old_particles[i]);
		}
	};

}; // end namespace gtfs
