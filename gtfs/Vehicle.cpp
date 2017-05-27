#include <iostream>

#include "gtfs.h"

namespace gtfs {
	/**
	* Create a Vehicle object with given ID.
	*
	* Vehicles are created with a default number of particles.
	*
	* @param vehicle_id the ID of the vehicle as given in the GTFS feed
	*/
	Vehicle::Vehicle (std::string id, sampling::RNG &rng) : Vehicle::Vehicle (id, 10, rng) {};

	/**
	 * Create a vehicle with specified number of particles, and ID.
	 *
	 * @param id the ID of the vehicle as given in the GTFS feed
	 * @param n  integer specifying the number of particles to initialize
	 *           the vehicle with
	 */
	Vehicle::Vehicle (std::string id, unsigned int n, sampling::RNG &rng) :
	id (id), n_particles (n), next_id (1) {
		std::clog << " ++ Created vehicle " << id << std::endl;

		particles.reserve(n_particles);
		for (unsigned int i=0; i<n_particles; i++) {
			particles.emplace_back(this, rng);
		}
	};

	/**
	* Desctructor for a vehicle object, ensuring all particles are deleted too.
	*/
	Vehicle::~Vehicle() {
		std::clog << " -- Vehicle " << id << " deleted!!" << std::endl;
	};


	// --- SETTERS
	void Vehicle::set_trip (std::shared_ptr<Trip> tp) {
		trip = tp;
	};

	// --- GETTERS

	/** @return ID of vehicle */
	std::string Vehicle::get_id () const {
		return id;
	};

	/** @return vector of particle references (so they can be mofidied ...) */
	std::vector<gtfs::Particle>& Vehicle::get_particles () {
		return particles;
	};

	const std::shared_ptr<Trip>& Vehicle::get_trip () const {
		return trip;
	}

	/** @return time in seconds since the previous observation */
	int Vehicle::get_delta () const {
		return delta;
	};



	// --- METHODS

	void Vehicle::update ( void ) {
	    if (trip->get_route ()->get_short_name () != "274") return;
		std::clog << "Updating particles!\n";

		std::cout << "Vehicle " << id << " has the current data:";
		if (trip == nullptr) {
			std::cout << "\n   * Trip ID: null";
		} else {
			std::cout << "\n   * Trip ID: " << trip->get_id ();
			std::cout << ", Route #" << trip->get_route ()->get_short_name ();
			std::cout << ", Shape ID: " << trip->get_route ()->get_shape ()->get_id ();
		}
		std::cout << "\n   * Stop Sequence: " << stop_sequence
			<< "\n   * Arrival Time: " << arrival_time
			<< "\n   * Departure Time: " << departure_time
			<< "\n   * Position: " << position
			<< "\n   * Timestamp: " << timestamp
			<< " (" << delta << " seconds since last update)"
			<< "\n\n";

		// for (auto& p: particles) p.transition ();
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
	void Vehicle::update (const transit_realtime::VehiclePosition &vp, GTFS &gtfs) {
		std::clog << "Updating vehicle location!\n";

		newtrip = true; // always assume a new trip unless we know otherwise
		if (vp.has_trip ()) { // TripDescriptor -> (trip_id, route_id)
		  	if (vp.trip ().has_trip_id () && trip != nullptr) newtrip = vp.trip ().trip_id () != trip->get_id ();
			if (vp.trip ().has_trip_id () && newtrip) {
				std::string trip_id = vp.trip ().trip_id ();
				auto ti = gtfs.get_trip (trip_id);
				if (ti != nullptr) set_trip (ti);
			}
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
	void Vehicle::update (const transit_realtime::TripUpdate &vp, GTFS &gtfs) {
		std::clog << "Updating vehicle trip update!\n";
		newtrip = true;
		if (vp.has_trip ()) { // TripDescriptor -> (trip_id, route_id)
		  	if (vp.trip ().has_trip_id () && trip != nullptr) newtrip = vp.trip ().trip_id () != trip->get_id ();
			if (vp.trip ().has_trip_id () && newtrip) {
				std::string trip_id = vp.trip ().trip_id ();
				auto ti = gtfs.get_trip (trip_id);
				if (ti != nullptr) set_trip (ti);
			}
		}
		// reset stop sequence if starting a new trip
		if (newtrip) {
			stop_sequence = 0;
			arrival_time = 0;
			departure_time = 0;
		}
		if (vp.stop_time_update_size () > 0) { // TripUpdate -> StopTimeUpdates -> arrival/departure time
			for (int i=0; i<vp.stop_time_update_size (); i++) {
				auto& stu = vp.stop_time_update (i);
				// only update stop sequence if it's greater than existing one
				if (stu.has_stop_sequence () && stu.stop_sequence () >= stop_sequence) {
					if (stu.stop_sequence () > stop_sequence) {
						// necessary to reset arrival/departure time if stop sequence is increased
						arrival_time = 0;
						departure_time = 0;
					}
					stop_sequence = stu.stop_sequence ();
					if (stu.has_arrival () && stu.arrival ().has_time ()) {
						arrival_time = stu.arrival ().time ();
					}
					if (stu.has_departure () && stu.departure ().has_time ()) {
						departure_time = stu.departure ().time ();
					}
				}
			}
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
		sampling::sample smp (particles.size ());
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
