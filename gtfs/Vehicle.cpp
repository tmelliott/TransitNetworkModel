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

		// step 3: set all of the travel times
		travel_times.clear ();
		auto route = trip->get_route ();
		if (!route) return;
		auto shape = trip->get_route ()->get_shape ();
		if (!shape) return;
		auto segs = shape->get_segments ();
		if (segs.size () == 0) return;
		for (auto sg: segs) travel_times.emplace_back (sg.segment);

		auto stops = route->get_stops ();
		if (stops.size () == 0) return;

		arrival_times.resize (stops.size ());
		departure_times.resize (stops.size ());
		for (unsigned i=0; i<stops.size (); i++) {
			arrival_times[i] = 0;
			departure_times[i] = 0;
		}
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
	/**
	 * @param k index of stop
	 * @return the arrival time at stop k
	 */
	uint64_t Vehicle::get_arrival_time (unsigned k) const {
		return arrival_times[k];
	};
	/** @return departure time at last stop */
	boost::optional<uint64_t> Vehicle::get_departure_time (void) const {
		return departure_time;
	};	
	/**
	 * @param k index of stop
	 * @return the departure time at stop k
	 */
	uint64_t Vehicle::get_departure_time (unsigned k) const {
		return departure_times[k];
	};
	/** @return delay at last stop */
	boost::optional<int> Vehicle::get_delay (void) const {
		return delay;
	};

	/** @return the time the observation was last taken */
	uint64_t Vehicle::get_timestamp (void) const {
		return timestamp;
	};

	/** @return the time since the previous observation */
	int Vehicle::get_delta (void) const {
		return delta;
	};

	/** @return the timestamp of the first observation (of trip beginning...) */
	uint64_t Vehicle::get_first_obs (void) const {
		return first_obs;
	};

	/** @return the max dist bus could have traveled */
	double Vehicle::get_dmaxtraveled (void) const {
		return dmaxtraveled;
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

	/** @return the vehicle's travel times along all segments */
	const std::vector<TravelTime>& Vehicle::get_travel_times () const {
		return travel_times;
	};

	/** @return the vehicle's travel time along segment i */
	TravelTime* Vehicle::get_travel_time (unsigned i) {
		if (i >= travel_times.size ()) return nullptr;
		return &travel_times[i];
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
		if (!updated || finished) return;
		// std::clog << "\n - Updating vehicle " << id << ": ("
		// 	<< travel_times.size () << " segments)";
		updated = false;


		if (!trip) return;
		auto route = trip->get_route ();
		if (!route) return;
		auto shape = route->get_shape ();
		if (!shape) return;
		auto path = shape->get_path ();
		if (path.size () == 0) return;
		auto stops = route->get_stops ();
		if (stops.size () == 0) return;
				

		if (newtrip) status = -1;
		if (status >= 0) {
			std::clog << "\n + In progress, " << particles.size () << " particles... ";
			
			double dbar = 0.0;
			for (auto& p: particles) {
				// std::clog << "\n  >> ";
				// // for (auto d: p.get_trajectory ()) std::clog << d << ", ";
				// std::clog << " >> k = " << p.get_latest () << " = " << p.get_distance ();
				// if (p.get_latest () < 0) std::clog << " -> something wrong?";
				// else
				// 	std::clog << " -> trajectory.size = " << p.get_trajectory ().size ()
				// 		<< " -> trajectory[k] = " << p.get_trajectory ()[p.get_latest ()];

				dbar += p.get_distance ();
			}
			dbar = dbar / particles.size ();
			std::clog << "\n + Start distance = " << dbar << "m";

			auto sg = shape->get_segments ();

			// std::clog << "\n --- mutating particles ...";
			std::cout.flush ();
			for (auto& p: particles) {
				// std::clog << "\n Particle starting at " << p.get_distance () << "m ...";
				p.mutate (rng);
				// std::clog << " and ending at " << p.get_distance () << "m ...";
				// for (unsigned ti=0; ti<p.get_travel_times ().size (); ti++) {
					// std::clog << "\n  [" << ti << ", "
						// << sg[ti].shape_dist_traveled
						// << "m] - initialized: "
						// << (p.get_travel_time (ti).initialized ? "1" : "0")
						// << ", completed: " << (p.get_travel_time (ti).complete ? "1" : "0")
						// << ", time = " << p.get_travel_time (ti).time;
				// }
			}
			// std::clog << " done. Calculating position ...";

			dbar = 0.0;
			double dmax = 0.0, dmin = INFINITY;
			// std::clog << "\n Particle distances: ";
			for (auto& p: particles) {
				// std::clog << "[" << p.get_trajectory ().size () << "/"
				// 	<< p.get_latest () << "] ";
				// std::clog << p.get_distance () << ", ";
				dbar += p.get_distance ();
				dmax = fmax(dmax, p.get_distance ());
				dmin = fmin(dmin, p.get_distance ());
			}
			// std::clog << "\n";
			if (particles.size () > 0) {
				dbar = dbar / particles.size ();
				std::clog << " -> mutation -> " << dbar << "m";
				if (path.back ().dist_traveled - dbar < 50) {
					finished = true;
					return;
				}
			} else {
				std::clog << " -> mutation ... ummm no particles :(";
			}
			// std::clog << " done. Calculating likelihoods ...";

			// Check likelihoods are decent
			double lmax = -INFINITY, lsum = 0.0;
			int mult = 1;
			// max 10 iterations (that's, like, as sd of 10*5 = 50m!!)
			while (lsum == 0) {
				lsum = 0.0; // reset each time
				// std::clog << "\n > [" << mult << "]: ";
				for (auto& p: particles) {
					p.calculate_likelihood (mult);
					lmax = fmax(lmax, p.get_likelihood ());
					lsum += exp(p.get_likelihood ());
					status = 0;
					// std::clog << "(" << p.get_likelihood () << ", " << p.get_distance () << ") | ";
				}
				mult++;
				if (mult == 10) {
					status = 1;
					break;
				}
			}
			double maxl = - log(2 * M_PI * 5 * mult);
			std::clog << "\n > The max possible likelihood is: " << maxl;
			std::clog << "\n > Max Likelihood = " << lmax
				<< " (mult = " << mult << ")";

			for (auto& p: particles) {
				if (p.get_likelihood () == lmax) {
					if (dmax == p.get_distance ())
						std::clog << "\n >> WARNING: max likelihood belongs to max distance ... (" << dmax << ")";
					else if (dmin == p.get_distance ()) 
						std::clog << "\n >> WARNING: max likelihood belongs to min distance ...(" << dmin << ")";
					break;
				}
			}

			

			if (status == 1 && lmax < -1000) {
				std::clog << " -> RESET";

				double dmean = 0.0, dmaxwt = 0.0;
				for (auto& p: particles) {
					if (p.get_likelihood () == lmax) dmaxwt = p.get_distance ();
					dmean += p.get_distance ();

					// std::clog << "\n++ ";
					// for (auto d: p.get_trajectory ()) std::clog << d << ", ";
					// std::clog << "\n -> ts=" << get_timestamp () << ", start=" << p.get_start ()
					// 	<< ", trajectory size = " << p.get_trajectory ().size ()
					// 	<< ", distance[" << p.get_latest () << "] = " << p.get_distance ();

				}
				dmean /= particles.size ();
				std::clog << " - Distance of MaxWt pt = " << dmaxwt
					<< ", mean dist = " << dmean;

				reset ();
			} else if (lmax < -1000) {
				std::clog << " -> ANOTHER CHANCE";
				status = 1;
			} else {
				std::clog << " -> all ok";
			}

			// Remove arrival/departure times
			// once they've been used in the likelihood ...
			// if (arrival_time && arrival_time.get () <= timestamp)
			// 	arrival_time.reset ();
			// if (departure_time && departure_time.get () <= timestamp)
			// 	departure_time.reset ();

			// RESET travel times (at some point ...? but only the ones that have been used ...)
			

			// check that the variability of weights is sufficient ...
			if (status == 0) {
				std::clog << "\n -> Resampling ...";
				resample (rng);
				std::clog << " done.";
				// double Neff = 0.0;
				// for (auto& p: particles) Neff += exp(2 * p.get_likelihood () - 2 * log(lsum));
				// Neff = pow(Neff, -1);
				// if (Neff < 2.0 * particles.size () / 3.0) {
				// 	std::clog << " -> RESAMPLE";
				// } else {
				// 	std::clog << " -> ENOUGH VARIABLITY - NO NEED TO RESAMPLE";
				// }

				std::clog << "\nDealing with segments ...";
				// Check for finished segments ...
				unsigned prevseg = 0;
				for (unsigned i=0; i<travel_times.size (); i++) {
					if (travel_times[i].complete) prevseg = i+1;
				}

				if (prevseg == 0) {
					// Go through all the particles and find current segment ...
					unsigned curseg = travel_times.size ();
					for (auto& p: particles) {
						for (unsigned i=0; i<travel_times.size (); i++) {
							if (p.get_travel_time (i).initialized &&
								!p.get_travel_time (i).complete) {
								curseg = std::min(curseg, i);
								break;
							}
						}
					}
					// set all prior segments to complete
					for (unsigned i=0; i<curseg; i++) travel_times[i].complete = true;

					std::clog << "\n+ Vehicle is on segment " << curseg << " of "
						<< travel_times.size ();

				} else {
					std::clog << "\n+ Vehicle was on segment " << prevseg << " of "
							<< travel_times.size ();
					unsigned curseg = travel_times.size ();
					for (auto& p: particles) {
						for (unsigned i=0; i<travel_times.size (); i++) {
							if (p.get_travel_time (i).initialized &&
								!p.get_travel_time (i).complete) {
								curseg = std::min(curseg, i);
								break;
							}
						}
					}

					if (curseg == prevseg) {
						std::clog << " ... and still is!";
					} else if (curseg < prevseg) {
						// reset those travel times
						for (unsigned i=curseg; i<travel_times.size (); i++) travel_times[i].reset ();
					} else {
						std::clog << " ... and is now on segment " << curseg;
						for (unsigned i=prevseg; i<curseg; i++) {
							// get details for all intermediate segments
							if (travel_times[i].used) continue;
							double tbar = 0.0;
							int Np = 0;
							double tmin = INFINITY, tmax = 0.0;
							for (auto& p: particles) {
								auto tt = p.get_travel_time (i);
								if (tt.initialized && tt.complete) {
									if (tt.time == 0) std::clog << "ZERO ";
									tbar += tt.time;
									Np++;
									if (tt.time > tmax) tmax = tt.time;
									if (tt.time < tmin) tmin = tt.time;
								}
							}
							std::clog << "; tsum = " << tbar << ", N = " << Np
								<< "; range: [" << tmin << ", " << tmax << "]";
							if (Np > 0) {
								tbar /= Np;
								travel_times[i].set_time (round (tbar));
								std::clog << "\n -> Segment " << i << ": " << round (tbar) << "s";
							} else {
								travel_times[i].set_time (0.0);
								std::clog << " ... no particles with travel time";
							}
						}
					}
				}
				std::clog << "\n + Vehicle modeling complete.";
				// loop through
			}

			// do checking
			// if (allok) {
			// 	status = 0;
			// }
		}

		if (status == -1) {
			// we are uninitialized - set up particles so they pass through the right place
			// - if arrival available, then start = arrival
			// - else if departure available, then start = departure - dwell[i] - noise
			// - else if bus is AT stop, then set t0 = timestamp - noise
			// > then set state = 0
			// - if none of these, then set t0's to match observed position...set state=3
			std::clog << "\n + Initializing particles";

			first_obs = timestamp;
			finished = false;
			if (stop_sequence && stop_sequence.get () == 1) {
				std::clog << " (case 1) - " << first_obs;
				if (arrival_time) {
					// know approximately when the vehicle arrived at stop[0]
					first_obs = arrival_time.get ();
					std::clog << " (arr)";
				} else if (departure_time) {
					// know approximately when the vehicle departed
					first_obs = departure_time.get ();
					std::clog << " (dep)";
				}
				// go head and init particles
				for (auto& p: particles) p.initialize (0.0, rng);

				status = 0;
			} else if (position.distanceTo (trip->get_stoptimes ()[0].stop->get_pos ()) < 20 &&
					   stop_sequence) {
				// we're close enough to be considered at the stop
				std::clog << " (case 2)";
				status = 0;
				// std::clog << "\n -> Fetching stop " << stop_sequence.get () << " of " << stops.size ();
				// double dz = stops[stop_sequence.get () - 1].shape_dist_traveled;
				for (auto& p: particles) p.initialize (0, rng);
			} else {
				std::clog << " (case 3)";
				std::vector<double> init_range {100000.0, 0.0};
				auto shape = trip->get_route ()->get_shape ();
				for (auto& p: shape->get_path ()) {
					if (p.pt.distanceTo(this->position) < 100.0) {
						double ds (p.dist_traveled);
						if (ds < init_range[0]) init_range[0] = ds;
						if (ds > init_range[1]) init_range[1] = ds;
					}
				}
				double r1 (round(init_range[0] * 100.0) / 100.0),
					   r2 (round(init_range[1] * 100.0) / 100.0);
				std::clog << " between "
					<< r1 << " and " << r2 << " m";
				// char buff[200];
				// snprintf(buff, sizeof (buff), "between %*.2f and %*.2f m", 8, init_range[0], 8, init_range[1]);
				// std::clog << buff;
				if (init_range[0] > init_range[1]) {
					std::clog << "\n   -> unable to locate vehicle on route -> cannot initialize.";
					return;
				} else if (init_range[0] == init_range[1]) {
					init_range[0] = init_range[0] - 100;
					init_range[1] = init_range[1] + 100;
				}
				std::clog << " -> done.";
				sampling::uniform udist (init_range[0], init_range[1]);
				for (auto& p: particles) p.initialize (udist.rand (rng), rng);
				std::clog << "\n ++ particles ready";
				status = 0;
			}
			
			std::clog << "\n Loading particles ...";
			double dmean = 0.0;
			for (auto& p: particles) {
				p.calculate_likelihood ();
				dmean += p.get_distance ();
			}
			dmean /= particles.size ();
			std::clog << " loaded; Dbar = " << dmean << std::endl;
		}
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
			// first check if the bus has moved very far ...
			gps::Coord newpos;
			newpos = gps::Coord(vp.position ().latitude (),
								     vp.position ().longitude ());
			dmaxtraveled = -1.0;
  			if (position.initialized () && position.distanceTo (newpos) < 50) {
				// bus has traveled less than 50 metres ...
				dmaxtraveled = position.distanceTo (newpos) < 50;
			}
			position = newpos;
		}
		if (vp.has_timestamp () && timestamp != vp.timestamp ()) {
			// only set delta if timestamp was set
			if (vp.timestamp () < timestamp) {
				std::clog << "\n *** Weird... new observation is EARLIER than the last one ...\n";
				std::clog << " - CURRENT: " << timestamp
					<< "\n -     NEW: " << vp.timestamp ()
					<< "\n - DIFFERENCE = " << vp.timestamp () - timestamp << "\n";
				newtrip = true;
				timestamp = vp.timestamp ();
				delta = 0;
			} else {
				delta = timestamp == 0 ? 0 : vp.timestamp () - timestamp;
				timestamp = vp.timestamp ();
				updated = true;
				if (delta > 60 * 10) newtrip = true; // if it has been more than 10 minutes, reset.
			}
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
		} else {
			// doesn't have a trip ... ermmmmmmm
			return;
		}
		// // reset stop sequence if starting a new trip
		// if (newtrip) {
		// 	stop_sequence = 0;
		// 	arrival_time = 0;
		// 	departure_time = 0;
		// }
		
		if (arrival_times.size () == 0 || departure_times.size () == 0) {
			std::cout << "\n x This vehicle has no arrival/departure times ... ?";
			return;
		}
		if (vp.stop_time_update_size () > 0) { // TripUpdate -> StopTimeUpdates -> arrival/departure time
			for (int i=0; i<vp.stop_time_update_size (); i++) {
				auto& stu = vp.stop_time_update (i);
				// only update stop sequence if it's greater than existing one
				if (stu.has_stop_sequence () && stu.stop_sequence () >= stop_sequence) {
					auto sseq = stu.stop_sequence () - 1;
					// if (sseq > stop_sequence) {
					// 	// necessary to reset arrival/departure time if stop sequence is increased
					// 	arrival_times[] = boost::none;
					// 	departure_time = boost::none;
					// }
					stop_sequence = stu.stop_sequence ();
					if (stu.has_arrival () && stu.arrival ().has_time ()) {
						// std::cout << "- there are " << arrival_times.size () << "slots; placing in slot "
						// 	<< sseq << std::endl;

						arrival_times[sseq] = stu.arrival ().time ();
						if (stu.arrival ().has_delay ()) delay = stu.arrival ().delay ();
					}
					if (stu.has_departure () && stu.departure ().has_time ()) {
						departure_times[sseq] = stu.departure ().time ();
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
		// std::clog << "\nClearing " << particles.size () << " particles ...";
		// std::cout.flush ();
		particles.clear ();
		// std::clog << " done.\nReserving memory for particles ...";
		// std::cout.flush ();
		particles.reserve(n_particles);
		// std::clog << " done.\nInitializing new particles ...";
		// std::cout.flush ();
		for (unsigned int i=0; i<n_particles; i++) {
			particles.emplace_back(this);
		}
		// std::clog << " done.";
		status = -1;
		delta = 0;
	};

}; // end namespace gtfs
