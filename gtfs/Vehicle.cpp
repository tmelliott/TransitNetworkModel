#include <iostream>

#include "gtfs.h"

namespace gtfs {
	/**
	* Create a Vehicle object with given ID.
	*
	* Vehicles are created with a default number of particles.
	*
	* @param id the ID of the vehicle as given in the GTFS feed
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
	id (id), initialized (false), n_particles (n), next_id (1) {
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

	/**
	 * Specify the vehicles trip.
	 * @param tp A shared trip pointer
	 */
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

	/** @return a pointer to the vehicle's trip object */
	const std::shared_ptr<Trip>& Vehicle::get_trip () const {
		return trip;
	}

	/** @return time in seconds since the previous observation */
	int Vehicle::get_delta () const {
		return delta;
	};



	// --- METHODS

	/**
	 * Update the vehicle state after loading GTFS feeds.
	 *
	 * The vehicle's particles will be updated, which involves
	 * initialization if the vehicle is starting a new trip,
	 * otherwise the particles will be transitioned.
	 *
	 * Likelihoods are calculated and weighted resampling takes place.
	 *
	 * @param rng A random number generator
	 */
	void Vehicle::update ( sampling::RNG& rng ) {
		std::cout << "Vehicle ID: " << id
			<< " " << position
			<< " - ts=" << timestamp
			<< " (" << delta << " seconds since last observation)";
		if (newtrip) std::cout << " - newtrip";
		if (!initialized) std::cout << " - initialization required";
		std::cout << "\n";

		if (newtrip || !initialized) {
			std::cout << " * Initializing particles: ";

			// Detect initial range of vehicle's "distance into trip"
			// -- just rough, so find points on the route within 100m of the GPS position
			std::vector<double> init_range {100000.0, 0.0};
			auto shape = trip->get_route ()->get_shape ();
			for (auto& p: shape->get_path ()) {
				if (p.pt.distanceTo(this->position) < 100.0) {
					double ds (p.dist_traveled);
					if (ds < init_range[0]) init_range[0] = ds;
					if (ds > init_range[1]) init_range[1] = ds;
				}
			}
			printf("between %*.2f and %*.2f m\n", 8, init_range[0], 8, init_range[1]);

			if (init_range[0] > init_range[1]) {
				std::cout << "   -> unable to locate vehicle on route -> cannot initialize.\n";
				return;
			} else if (init_range[0] == init_range[1]) {
				init_range[0] = init_range[0] - 100;
				init_range[1] = init_range[1] + 100;
			}
			sampling::uniform udist (init_range[0], init_range[1]);
			sampling::uniform uspeed (2, 30);
			for (auto& p: particles) p.initialize (udist, uspeed, rng);

			initialized = true;
		} else if (delta > 0 ) {
			std::cout << " * Updating particles: " << delta << "s\n";

			double dbar = 0;
			double vbar = 0;
			for (auto& p: particles) {
				dbar += p.get_distance ();
				vbar += p.get_velocity ();
			}
			printf("   - Distance = %*.0fm, Velocity = %*.1fm/s.\n",
				   5, dbar / particles.size (), 5, vbar / particles.size ());

			for (auto& p: particles) p.transition (rng);
		} else {
			return;
		}

		// No particles near? Oh ...
		std::vector<double> lh;
		lh.reserve (particles.size ());
		for (auto& p: particles) lh.push_back (p.get_likelihood ());
		if (*std::max_element (lh.begin (), lh.end ()) < -10) {
			std::cout << "   - Reset vehicle (not close particles) "
				<< " - max likelihood = exp(" << *std::max_element (lh.begin (), lh.end ())
				<< ")\n";
			reset ();

			return;
		}

		// Resample them!
		std::cout << "   - Resampling ";
		resample (rng);
		std::cout << "\n";

		// Estimate parameters
		double dbar = 0;
		double vbar = 0;
		for (auto& p: particles) {
			dbar += p.get_distance ();
			vbar += p.get_velocity ();
		}
		printf("   - Distance = %*.0fm, Velocity = %*.1fm/s.\n",
			   5, dbar / particles.size (), 5, vbar / particles.size ());
	}

	/**
	 * Update the location of the vehicle object.
	 *
	 * This does NOT trigger a particle update, as we may need
	 * to also insert TripUpdates later.
	 * Check that the trip_id is the same, otherwise set `newtrip = false`
	 *
	 * @param vp   a vehicle position from the realtime feed
	 * @param gtfs a GTFS object containing the GTFS static data
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
			if (timestamp > 0 && timestamp < vp.timestamp ()) {
				delta = vp.timestamp () - timestamp;
			} else {
				delta = 0;
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
	 * @param gtfs a GTFS object containing the GTFS static data
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
		std::vector<double> lh;
		lh.reserve (particles.size ());
		for (auto& p: particles) lh.push_back (exp(p.get_likelihood ()));
		std::cout << " > Max likelihood: " << *std::max_element (lh.begin (), lh.end ());
		// if (*std::max_element (lh.begin (), lh.end ()) < exp(-10)) return;

		sampling::sample smp (lh);
		std::vector<int> pkeep (smp.get (n_particles, rng));

		// Move old particles into temporary holding vector
		std::vector<gtfs::Particle> old_particles = std::move(particles);

		// Copy new particles, incrementing their IDs (copy constructor does this)
		particles.reserve(n_particles);
		for (auto& i: pkeep) {
			particles.push_back(old_particles[i]);
		}
		std::cout << "\n";

	};

	/**
	 * Reset vehicle's particles to zero-state.
	 */
	void Vehicle::reset () {
		delta = 0;
		timestamp = 0;
		initialized = false;
		particles.clear ();
		particles.reserve(n_particles);
		for (unsigned int i=0; i<n_particles; i++) {
			particles.emplace_back(this);
		}
	};

}; // end namespace gtfs
