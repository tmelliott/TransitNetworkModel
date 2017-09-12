#include <iostream>
#include <fstream>

#include "gtfs.h"

namespace gtfs {
	/**
	* Create a Vehicle object with given ID.
	*
	* Vehicles are created with a default number of particles.
	*
	* @param id the ID of the vehicle as given in the GTFS feed
	*/
	Vehicle::Vehicle (std::string id) : Vehicle::Vehicle (id, 0) {};

	/**
	 * Create a vehicle with specified number of particles, and ID.
	 *
	 * @param id the ID of the vehicle as given in the GTFS feed
	 * @param n  integer specifying the number of particles to initialize
	 *           the vehicle with
	 */
	Vehicle::Vehicle (std::string id, unsigned int n) :
	id (id), n_particles (n), next_id (1) {
		particles.reserve(n_particles);
		for (unsigned int i=0; i<n_particles; i++) {
			particles.emplace_back(this);
		}
	};

	/**
	* Desctructor for a vehicle object, ensuring all particles are deleted too.
	*/
	Vehicle::~Vehicle(void) {};


	// --- SETTERS

	/**
	 * Specify the vehicles trip.
	 * @param tp A shared trip pointer
	 * @param t  Time of the first observation
	 */
	void Vehicle::set_trip (std::shared_ptr<Trip> tp, uint64_t t) {
		if (trip) {
			// step 1: finish off previous trip!

		}

		// step 2: set new trip and reset all of the particles ...
		trip = tp;
		first_obs = t;
		status = -1;
	};

	// --- GETTERS

	/** @return ID of vehicle */
	std::string Vehicle::get_id (void) const {
		return id;
	};

	/** @return vector of particle references (so they can be mofidied ...) */
	std::vector<gtfs::Particle>& Vehicle::get_particles (void) {
		return particles;
	};

	/** @return a pointer to the vehicle's trip object */
	const std::shared_ptr<Trip>& Vehicle::get_trip (void) const {
		return trip;
	}

	/** @return the vehicle's position */
	const gps::Coord& Vehicle::get_position (void) const {
		return position;
	};

	/** @return stop sequence of the vehicle */
	boost::optional<unsigned> Vehicle::get_stop_sequence (void) const {
		return stop_sequence;
	}
	/** @return arrival time at last stop */
	boost::optional<uint64_t> Vehicle::get_arrival_time (void) const {
		return arrival_time;
	};
	/** @return departure time at last stop */
	boost::optional<uint64_t> Vehicle::get_departure_time (void) const {
		return departure_time;
	};
	/** @return delay at last stop */
	boost::optional<int> Vehicle::get_delay (void) const {
		return delay;
	};

	/** @return the time the observation was last taken */
	uint64_t Vehicle::get_timestamp (void) const {
		return timestamp;
	};

	/** @return the timestamp of the first observation (of trip beginning...) */
	uint64_t Vehicle::get_first_obs (void) const {
		return first_obs;
	};

	// /** @return the vehicle's dwell times at all stops */
	// const std::vector<DwellTime>& get_dwell_times () const {
	// 	return dwell_times;
	// };
	//
	// /** @return the vehicle's dwell time at stop i */
	// const DwellTime* get_dwell_time (unsigned i) const {
	// 	if (i >= dwell_times.size ()) return nullptr;
	// 	return &dwell_times[i];
	// };
	// /** @return the vehicle's travel times along all segments */
	// const std::vector<vTravelTime>& get_travel_times () const {
	// 	return travel_times;
	// };
	//
	// /** @return the vehicle's travel time along segment i */
	// const vTravelTime* get_travel_time (unsigned i) const {
	// 	if (i >= travel_times.size ()) return nullptr;
	// 	return &travel_times[i];
	// };

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
		std::cout << "\n - Updating vehicle " << id << ":";
		if (!updated) return;
		if (newtrip) status = -1;
		switch (status) {
			case 0:
			{
				// just rolling along nicely - update + mutate
				std::cout << "\n + In progress";

				for (auto& p: particles) p.mutate (rng);
				double lmax = -INFINITY;
				for (auto& p: particles) {
					p.calculate_likelihood ();
					lmax = fmax(lmax, p.get_likelihood ());
				}
				std::cout << "\n > Max Likelihood = " << lmax;
				resample (rng);

				break;
			}
			case 1:
			case 2:
			case 3:
			{
				// just started a new trip, unsure whether its started or not
				// - if vehicle appears to be approaching stop, readjust t0's to match new distance
				// - else mutate + update; decrease state=2
				std::cout << "\n + Case " << status;

				for (auto& p: particles) p.mutate (rng);
				double lmax = -INFINITY;
				for (auto& p: particles) {
					p.calculate_likelihood ();
					lmax = fmax(lmax, p.get_likelihood ());
				}
				std::cout << "\n > Max Likelihood = " << lmax;
				resample (rng);

				bool allok = true;
				// do checking
				if (allok) {
					status--;
					break; // otherwise it'll initialize
				}
			}
			case -1:
			{
				// we are uninitialized - set up particles so they pass through the right place
				// - if arrival available, then start = arrival
				// - else if departure available, then start = departure - dwell[i] - noise
				// - else if bus is AT stop, then set t0 = timestamp - noise
				// > then set state = 0
				// - if none of these, then set t0's to match observed position...set state=3
				std::cout << "\n + Initializing particles";

				first_obs = timestamp;
				if (stop_sequence && stop_sequence.get () == 1) {
					std::cout << " (case 1) - " << first_obs;
					if (arrival_time) {
						// know approximately when the vehicle arrived at stop[0]
						first_obs = arrival_time.get ();
						std::cout << " (arr)";
					} else if (departure_time) {
						// know approximately when the vehicle departed
						first_obs = departure_time.get ();
						std::cout << " (dep)";
					}
					// go head and init particles
					for (auto& p: particles) p.initialize (0.0, rng);

					status = 0;
				} else if (position.distanceTo (trip->get_stoptimes ()[0].stop->get_pos ()) < 20) {
					// we're close enough to be considered at the stop
					std::cout << " (case 2)";
					status = 1;
					for (auto& p: particles) p.initialize (0.0, rng);
				} else {
					std::cout << " (case 3)";
					std::vector<double> init_range {100000.0, 0.0};
					auto shape = trip->get_route ()->get_shape ();
					for (auto& p: shape->get_path ()) {
						if (p.pt.distanceTo(this->position) < 100.0) {
							double ds (p.dist_traveled);
							if (ds < init_range[0]) init_range[0] = ds;
							if (ds > init_range[1]) init_range[1] = ds;
						}
					}
					printf("between %*.2f and %*.2f m", 8, init_range[0], 8, init_range[1]);
					if (init_range[0] > init_range[1]) {
						std::cout << "\n   -> unable to locate vehicle on route -> cannot initialize.";
						return;
					} else if (init_range[0] == init_range[1]) {
						init_range[0] = init_range[0] - 100;
						init_range[1] = init_range[1] + 100;
					}
					sampling::uniform udist (init_range[0], init_range[1]);
					for (auto& p: particles) p.initialize (udist.rand (rng), rng);
					status = 3;
				}
				for (auto& p: particles) p.calculate_likelihood ();
				break;
			}
		}

		// 1. resample
		// 2. mutate
		// 3. compute likelihood for the data that's there
		//    - position
		//    - arrival_time OR departure_time OR neither*
		// 4. set particle weights

		// 5. UNSET* arrival/departure time (so it's not reused!)


		// *perhaps only if arrival/departure_time < timestamp;
		//  just in case a position update is not quite recieved yet ...


		// THEN save travel times + dwell times for any segments completed
		//      or stops serviced since the last update


		//
		// // No particles near? Oh ...
		// // std::vector<double> lh;
		// double lhsum = 0;
		// // lh.reserve (particles.size ());
		// for (auto& p: particles) {
		// 	lhsum += exp(p.get_likelihood ());
		// 	// lh.push_back (p.get_likelihood ());
		// }
		// // std::cout << "\n Lhoods: ";
		// // for (auto& p: particles) std::cout << exp(p.get_likelihood ()) << ", ";
		// std::cout << "\n Sum(lhoods): " << lhsum;
		// // weights
		// if (lhsum == 0) return;
		// double sumwt2 = 0;
		// for (auto& p: particles) {
		// 	p.set_weight (exp(p.get_likelihood ()) / lhsum);
		// 	sumwt2 += pow(p.get_weight (), 2);
		// }
		// // std::cout << "\nWeights: ";
		// // for (auto& p: particles) std::cout << p.get_weight () << ", ";
		// // std::cout << "\n---------------";
		// std::cout << "\nSum Wt^2: " << sumwt2;
		// float Nth = 2 * particles.size () / 3;
		// std::cout << " -> " << (1 / sumwt2) << " (Nth = " << Nth << "): ";
		// if (1 / sumwt2 < Nth) {
		// 	std::cout << "resample\n>particle weights:";
		// 	for (auto& p: particles) std::cout << p.get_weight () << ", ";
		// 	// std::sort (particles.begin (), particles.end ());
		// 	std::cout << " - sorted - ";
		// 	resample (rng);
		// 	std::cout << "resample done.";
		// 	for (auto& p: particles) p.set_weight (1.0 / particles.size ());
		// 	std::cout << " (reweighted)";
		// }

		// std::cout << "\n---";
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
		// std::clog << "Updating vehicle location!\n";

		newtrip = true;
		updated = false;
		if (vp.has_trip ()) { // TripDescriptor -> (trip_id, route_id)
		  	if (vp.trip ().has_trip_id () && trip != nullptr)
				newtrip = vp.trip ().trip_id () != trip->get_id ();
			if (vp.trip ().has_trip_id () && newtrip) {
				std::string trip_id = vp.trip ().trip_id ();
				auto ti = gtfs.get_trip (trip_id);
				if (ti != nullptr) set_trip (ti, vp.timestamp ());
			}
		}
		if (vp.has_position ()) {
			position = gps::Coord(vp.position ().latitude (),
								  vp.position ().longitude ());
		}
		if (vp.has_timestamp () && timestamp != vp.timestamp ()) {
			timestamp = vp.timestamp ();
			updated = true;
		}
		if (newtrip) reset ();
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
		// std::clog << "Updating vehicle trip update!\n";
		if (vp.has_trip ()) { // TripDescriptor -> (trip_id, route_id)
		  	if (vp.trip ().has_trip_id () &&
				trip != nullptr &&
				vp.trip ().trip_id () != trip->get_id ()) {
				// If the TRIP UPDATE trip doesn't match vehicle position, ignore it.
				return;
			}
		}
		// // reset stop sequence if starting a new trip
		// if (newtrip) {
		// 	stop_sequence = 0;
		// 	arrival_time = 0;
		// 	departure_time = 0;
		// }
		if (vp.stop_time_update_size () > 0) { // TripUpdate -> StopTimeUpdates -> arrival/departure time
			for (int i=0; i<vp.stop_time_update_size (); i++) {
				auto& stu = vp.stop_time_update (i);
				// only update stop sequence if it's greater than existing one
				if (stu.has_stop_sequence () && stu.stop_sequence () >= stop_sequence) {
					if (stu.stop_sequence () > stop_sequence) {
						// necessary to reset arrival/departure time if stop sequence is increased
						arrival_time = boost::none;
						departure_time = boost::none;
					}
					stop_sequence = stu.stop_sequence ();
					if (stu.has_arrival () && stu.arrival ().has_time ()) {
						arrival_time = stu.arrival ().time ();
						if (stu.arrival ().has_delay ()) delay = stu.arrival ().delay ();
					}
					if (stu.has_departure () && stu.departure ().has_time ()) {
						departure_time = stu.departure ().time ();
						if (stu.departure ().has_delay ()) delay = stu.departure ().delay ();
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
	unsigned long Vehicle::allocate_id (void) {
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

		sampling::sample smp (lh);
		std::vector<int> pkeep (smp.get (n_particles, rng));

		// Move old particles into temporary holding vector
		std::vector<gtfs::Particle> old_particles = std::move(particles);

		// Copy new particles, incrementing their IDs (copy constructor does this)
		particles.reserve(n_particles);
		for (auto& i: pkeep) {
			particles.push_back(old_particles[i]);
		}
	};

	/**
	 * Reset vehicle's particles to zero-state.
	 */
	void Vehicle::reset (void) {
		// delta = 0;
		// timestamp = 0;
		// initialized = false;
		particles.clear ();
		particles.reserve(n_particles);
		for (unsigned int i=0; i<n_particles; i++) {
			particles.emplace_back(this);
		}
		status = -1;
	};

}; // end namespace gtfs
