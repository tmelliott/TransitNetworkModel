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

		std::cout << "Vehicle " << id << " has the current data:"
			<< "\n   * Trip ID: " << trip_id
			<< "\n   * Route ID: " << route_id
			<< "\n   * Stop Sequence: " << stop_sequence
			<< "\n   * Arrival Time: " << arrival_time
			<< "\n   * Departure Time: " << departure_time
			<< "\n   * Position: " << position
			<< "\n   * Timestamp: " << timestamp
			<< " (" << delta << " seconds since last update)"
			<< "\n\n";
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
		if (vp.has_trip ()) { // TripDescriptor -> (trip_id, route_id)
			if (vp.trip ().has_trip_id ()) newtrip = vp.trip ().trip_id () != trip_id;
			trip_id = vp.trip ().trip_id ();
			route_id = vp.trip ().route_id ();
		}
		if (vp.has_position ()) { // VehiclePosition -> (lat, lon)
			position = gps::Coord(vp.position ().latitude (),
								  vp.position ().longitude ());
		}
		if (vp.has_timestamp () && timestamp != vp.timestamp ()) {
			if (timestamp > 0) {
				delta = vp.timestamp () - timestamp;
			}
			timestamp = vp.timestamp ();
		}
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
		if (vp.has_trip ()) { // TripDescriptor -> (trip_id, route_id)
			if (vp.trip ().has_trip_id ()) newtrip = vp.trip ().trip_id () != trip_id;
			trip_id = vp.trip ().trip_id ();
			route_id = vp.trip ().route_id ();
		}
		// reset stop sequence if starting a new trip
		// if (newtrip) {
		// 	stop_sequence = 0;
		// 	arrival_time = 0;
		// 	departure_time = 0;
		// }
		if (vp.stop_time_update_size () > 0) {
			for (int i=0; i<vp.stop_time_update_size (); i++) {
				auto& stu = vp.stop_time_update (i);
				// only update stop sequence if it's greater than existing one
				if (stu.has_stop_sequence () && stu.stop_sequence () >= stop_sequence) {
					stop_sequence = stu.stop_sequence ();
					std::cout << "stu " << i << "(" << stop_sequence << ") ";
					if (stu.has_arrival () && stu.arrival ().has_time ()) {
						arrival_time = stu.arrival ().time ();
					}
					if (stu.has_departure () && stu.departure ().has_time ()) {
						departure_time = stu.departure ().time ();
					}
				}
			}
		}
		if (vp.has_timestamp () && timestamp != vp.timestamp ()) {
			if (timestamp > 0) {
				delta = vp.timestamp () - timestamp;
			}
			timestamp = vp.timestamp ();
		}
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
