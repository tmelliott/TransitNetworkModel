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
	Vehicle::Vehicle (std::string id) : Vehicle::Vehicle (id, 10) {};

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

	void Vehicle::update ( sampling::RNG& rng ) {
	    if (trip->get_route ()->get_short_name () != "274") return;

		if (newtrip) {
			std::cout << position << " ";
			std::cout << " * Initializing particles: ";

			// Detect initial range of vehicle's "distance into trip"
			// -- just rough, so find points on the route within 100m of the GPS position
			std::vector<double> init_range {100000.0, 0.0};
			auto segs = trip->get_route ()->get_shape ()->get_segments ();
			for (auto& seg: segs) {
				double d (seg.shape_dist_traveled);
				for (auto& p: seg.segment->get_path ()) {
					if (p.pt.distanceTo(this->position) < 100.0) {
						double ds (d + p.seg_dist_traveled);
						if (ds < init_range[0]) init_range[0] = ds;
						if (ds > init_range[1]) init_range[1] = ds;
					}
				}
				printf("between %*.2f and %*.2f m\n", 8, init_range[0], 8, init_range[1]);
			}
			if (init_range[0] > init_range[1]) {
				std::cout << "   -> unable to locate vehicle on route -> cannot initialize.\n";
				return;
			}
			sampling::uniform udist (init_range[0], init_range[1]);
			sampling::uniform uspeed (0, 30);
			for (auto& p: particles) p.initialize (udist, uspeed, rng);
		} else {
			std::cout << " * Updating particles:\n";
		}

		// std::cout << "Vehicle " << id << " has the current data:";
		// if (trip == nullptr) {
		// 	std::cout << "\n   * Trip ID: null";
		// } else {
		// 	std::cout << "\n   * Trip ID: " << trip->get_id ();
		// 	std::cout << ", Route #" << trip->get_route ()->get_short_name ();
		// 	std::cout << ", Shape ID: " << trip->get_route ()->get_shape ()->get_id ();
		// }
		// std::cout << "\n   * Stop Sequence: " << stop_sequence
		// 	<< "\n   * Arrival Time: " << arrival_time
		// 	<< "\n   * Departure Time: " << departure_time
		// 	<< "\n   * Position: " << position
		// 	<< "\n   * Timestamp: " << timestamp
		// 	<< " (" << delta << " seconds since last update)"
		// 	<< "\n\n";

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
	 * to also insert VehiclevoidPositions later.
	 *
	 * @param vp a vehicle position from the realtime feed
	 */
	void Vehicle::update (const transit_realtime::TripUpdate &vp, GTFS &gtfs) {
		std::clog << "Updating vehicle trip update!\n";
		if (vp.has_trip ()) { // TripDescriptor -> (trip_id, route_id)
		  	if (vp.trip ().has_trip_id () &&
				trip != nullptr &&
				vp.trip ().trip_id () != trip->get_id ()) {
				// If the TRIP UPDATE trip doesn't match vehicle position, ignore it.
				return;
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
