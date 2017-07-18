#include <string>
#include <iostream>
#include <vector>
#include <math.h>

#include "gtfs.h"

namespace gtfs {
	/**
	* Particle constructor.
	*
	* The ID is automatically selected from the parent vehicle.
	* Values are computed based on the approximate location of the bus,
	* allowing for noise.
	* RNG required otherwise the particles would all be identical!
	*
	* @param v the vehicle object pointer to which the particle belongs
	*/
	Particle::Particle (Vehicle* v) :
		id (v->allocate_id ()),
		parent_id (0),
		distance (0),
		velocity (0),
		finished (false),
		stop_index (0),
		arrival_time (0),
		dwell_time (0),
		segment_index (0),
		queue_time (0),
		begin_time (0),
		log_likelihood (-INFINITY),
		vehicle (v) {

		// std::clog << " + Created particle for vehicle " << v->get_id ()
		// 	<< " with id = " << id << "\n";
		// if (vehicle && vehicle->get_trip () && vehicle->get_trip ()->get_route () &&
		// 	vehicle->get_trip ()->get_route ()->get_shape () &&
		// 	vehicle->get_trip ()->get_route ()->get_shape ()->get_segments ().size () > 0) {
		// 	travel_times.reserve (vehicle->get_trip ()->get_route ()
		// 							->get_shape ()->get_segments ().size ());
		// 	std::cout << "...";
		// 	for (auto& s: vehicle->get_trip ()->get_route ()
		// 					->get_shape ()->get_segments ()) {
		// 		travel_times.emplace_back (s.segment);
		// 	}
		// }
	};

	/**
	 * Particle copy constructor.
	 *
	 * Makes an almost exact copy of the referenced particle,
	 * but gives it a unique ID and sets the parent_id appropriately.
	 *
	 * @param p the parent particle to be copied
	 */
	Particle::Particle (const Particle &p) {
		// std::clog << " >+ Copying particle " << p.get_id () << " -> ";

		distance = p.get_distance ();
		velocity = p.get_velocity ();
		finished = p.is_finished ();
		stop_index = p.get_stop_index ();
		arrival_time = p.get_arrival_time ();
		dwell_time = p.get_dwell_time ();
		segment_index = p.get_segment_index ();
		queue_time = p.get_queue_time ();
		begin_time = p.get_begin_time ();
		travel_times = p.get_travel_times ();
		log_likelihood = p.get_likelihood ();

		// Copy vehicle pointer
		vehicle = p.vehicle;
		parent_id = p.id;


		// Increment particle id
		id = p.vehicle->allocate_id ();
		// std::clog << id << std::endl;
	};

	/**
	* Destructor for a particle.
	*/
	Particle::~Particle() {
		// std::clog << " - Particle " << id << " deleted." << std::endl;
	};


	// --- GETTERS

	/**
	* @return the particle's ID
	*/
	unsigned long Particle::get_id () const {
		return id;
	};

	/**
	 * @return the parent particle's ID
	 */
	unsigned long Particle::get_parent_id () const {
		return parent_id;
	};


	// --- METHODS

	/**
	 * Initialize particle with distance etc.
	 * @param dist  a uniform number generator for the particle's distance into trip
	 * @param speed a uniform number generator for the particle's velocity
	 * @param rng   a random number generator
	 */
	void Particle::initialize (sampling::uniform& dist, sampling::uniform& speed, sampling::RNG& rng) {
		distance = dist.rand (rng);
		velocity = speed.rand (rng);

		// which stop are we at?
		auto trip = vehicle->get_trip ();
		if (!trip) return;
		auto route = trip->get_route ();
		if (!route) return;
		auto stops = route->get_stops ();
		if (stops.size () == 0) return;
		auto shape = route->get_shape ();
		if (!shape) return;
		auto segments = shape->get_segments ();
		if (segments.size () == 0) return;

		for (unsigned int i=0; i<stops.size (); i++) {
			// once distance smaller than stop's, we're done
			if (distance < stops[i].shape_dist_traveled) break;
			stop_index = i+1; // 1-based index
		}

		travel_times.reserve (segments.size ());
		for (unsigned int i=0; i<segments.size (); i++) {
			if (distance >= segments[i].shape_dist_traveled) {
				segment_index = i; // 0-based index
			}
			travel_times.emplace_back (segments[i].segment);
		}

		// std::clog << "   - " << *this << " -> ";
		calculate_likelihood ();
		// std::clog << log_likelihood <<  "\n";
	}

	/**
	 * Move the particle according to speed,
	 * shape/segments/stops, and `delta`.
	 *
	 * @param rng a random number generator
	 */
	void Particle::transition (sampling::RNG& rng) {
		if (vehicle->get_delta () == 0 || finished) return;
		delta_t = vehicle->get_delta ();

		// std::clog << "   - " << *this << " -> ";

		// --- Three phases
		// PHASE ONE: initial wait time
		transition_phase1 (rng);

		// PHASE TWO: system noise
		transition_phase2 (rng);

		// PHASE THREE: transition forward
		transition_phase3 (rng);

		// std::clog << *this << " -> ";

		// Done with transition; compute likelihood and finish.
		calculate_likelihood ();
		// std::clog << log_likelihood <<  "\n";
	};

	/**
	 * Compute the likelihood of the particle
	 * given the bus's reported location
	 * and stop updates.
	 */
	void Particle::calculate_likelihood (void) {
		gps::Coord x = get_coords (distance, vehicle->get_trip ()->get_route ()->get_shape ());
		// std::cout << x;
		std::vector<double> z (x.projectFlat(vehicle->get_position ()));

		double llhood = 0.0;
		double sigy   = 10.0;

		double epsS = 20.0;
		double epsI = 30.0;

		if (arrival_time > 0 &&
			arrival_time + dwell_time >= vehicle->get_timestamp ()) {
			// particle at stop
			auto stop = vehicle->get_trip ()->get_stoptimes ()[stop_index-1].stop;
			if (vehicle->get_position ().distanceTo (stop->get_pos ()) < epsS) {
				llhood -= (log(M_PI) + 2 * log(epsS));
			} else {
				llhood = -INFINITY;
			}
		} else if (queue_time > 0 && begin_time == 0) {
			// particle at intersection
			if (segment_index == 0) llhood = -INFINITY;
			auto dx = vehicle->get_trip ()->get_route ()->get_shape ()
				->get_segments ()[segment_index].shape_dist_traveled;
			auto Int = vehicle->get_trip ()->get_route ()->get_shape ()
				->get_segments ()[segment_index].segment->get_from ();
			auto B2 = get_coords (dx - epsI, vehicle->get_trip ()->get_route ()
				->get_shape ());
			if (vehicle->get_position ().distanceTo (Int->get_pos ()) < epsI &&
				vehicle->get_position ().distanceTo (B2) < epsI) {
				double logA = log(2) + 2 * log(epsI) +
					log(acos(0.5) - acos(sqrt(3) / 4));
				llhood -= logA;
			}
		} else {
			// particle moving
			llhood -= log (2 * M_PI * sigy);
			llhood -= (pow(z[0], 2) + pow(z[1], 2)) / (2 * pow(sigy, 2));
		}

		log_likelihood = llhood;
	};

	/**
	 * Phase 1 of the particle's transition, involving determining initial
	 * wait conditions.
	 *
	 * If the bus is at a stop, ensure it waits long enough.
	 * Similarly for intersections, queue longer if needed.
	 * Otherwise, there's no wait time.
	 *
	 * @param rng reference to the random number generator
	 */
	void Particle::transition_phase1 (sampling::RNG& rng) {

		if (arrival_time > 0 &&
			arrival_time + dwell_time >= vehicle->get_timestamp ()) {
		// {^-----------------------^} == departure_time

			// --- Particle is stopped at the bus stop
			double gamma (6.0);
			sampling::exponential exptau (1.0 / 10.0);

			// min dwell time is 3 seconds
			if (dwell_time < gamma) dwell_time = gamma;
			dwell_time += exptau.rand (rng);
			delta_t = vehicle->get_timestamp () - (arrival_time + dwell_time);
		} else if (queue_time > 0 && begin_time == 0) {
			// --- Particle is queued at an intersection
			sampling::exponential expkappa (1.0 / 20.0);
			int qt = (int) expkappa.rand (rng);
			if (qt < delta_t) {
				// particle queues LESS than time remaining; goes through
				queue_time += qt;
				delta_t -= qt;
				begin_time = vehicle->get_timestamp () - delta_t;
			} else {
				// particle queues too long - remains at intersection
				queue_time += delta_t;
				delta_t = 0;
			}
		} else {
			// for now we'll just stay where we are with some probability
			// if not at stop/segment
			if (rng.runif () < 0.3) delta_t = 0;
		}

	};

	/**
	 * Phase 2 of the particle's transition, involving adding random
	 * system noise.
	 * Uses rejection sampling.
	 *
	 * @param rng reference to the random number generator
	 */
	void Particle::transition_phase2 (sampling::RNG& rng) {
		double sigv (5);
		double vel;
		bool reject (true);
		// while (vel <= 2 || vel >= 30) vel = rng.rnorm () * sigv + velocity;
		while (reject) {
			vel = velocity + rng.rnorm () * sigv;
			reject = vel < 2 || vel > 30;
		}
		velocity = vel;
	};

	/**
	 * Phase 3 of the particle's transition, involving movement
	 * until time's up.
	 *
	 * @param rng [description]
	 */
	void Particle::transition_phase3 (sampling::RNG& rng) {
		// double dmax;
		auto trip = vehicle->get_trip ();
		if (!trip) return;
		auto route = trip->get_route ();
		if (!route) return;
		auto shape = route->get_shape ();
		if (!shape) return;
		if (route->get_stops ().size () == 0) return;
		auto stops = route->get_stops ();

		if (shape->get_segments ().size () == 0) return;
		auto segments = shape->get_segments ();

		if (!segments.back ().segment) {
			finished = true;
			return;
		}

		int M (stops.size ());
		int L (segments.size());

		while (delta_t > 0 && !finished) {
			/**
			 * # NOTES
			 * STOPS are use 1-based indexing to match the GTFS feed
			 * Therefore, the FIRST stop is stop_index = 1, which is stops[0].
			 * Therefore, the ith stop is stop_index = i, which is stops[i-1].
			 * To make things more complicated, the (i+1)th stop is stop+index = i+1,
			 * 								which is stops[i]!!!
			 *
			 * However, segments are 0-indexed, so a particle with segment_index=1 is
			 * is on the SECOND (i+1th) segment.
			 */
			// distance of next stop, segment
			double Sd = stops[stop_index].shape_dist_traveled;
			double Rd;
			if (segment_index + 1 >= L) {
				// on last segment
				Rd = Sd;
			} else {
				Rd = segments[segment_index+1].shape_dist_traveled;
			}
			if (Sd <= Rd) { // Next up: STOP! (including if segmnet )
				double eta = (1 / velocity) * (Sd - distance);
				if (eta <= delta_t) {
					delta_t = delta_t - eta;
					travel_times[segment_index].time += eta;
					stop_index++;
					arrival_time = vehicle->get_timestamp () - delta_t;
					dwell_time = 0;
					distance = Sd;
					if (stop_index == M) {
						finished = true;
						travel_times[segment_index].complete = true;
						break;
					}
					double pi (0.5);
					double gamma (6.0);
					sampling::exponential exptau (1.0 / 10.0);
					if (rng.runif () < pi) dwell_time = (int) gamma + exptau.rand (rng);
					delta_t -= dwell_time;
				} else {
					travel_times[segment_index].time += delta_t;
					distance += velocity * delta_t;
					delta_t = 0;
				}
			} else {    // Next up: INTERSECTION!
				double eta = (1 / velocity) * (Rd - distance);
				if (eta <= delta_t) {
					delta_t = delta_t - eta;
					// std::cout << " > entering intersection; "
						// << "there are " << travel_times.size () << " times initialized ...\n";
					travel_times[segment_index].time += eta;
					travel_times[segment_index].complete = true;
					// arrive at next intersection
					segment_index++;
					travel_times[segment_index].initialized = true;
					queue_time = 0;
					begin_time = 0;
					distance = Rd;
					if (segment_index == L) {
						// actually this should never happen!
						finished = true;
						break;
					}
					double rho (0.5);
					sampling::exponential expkappa (1.0 / 20.0);
					if (rng.runif () < rho) queue_time = (int) expkappa.rand (rng);
					// max queue time till observation; allow next iteration to extend duration
					if (queue_time > delta_t) {
						queue_time = delta_t;
						delta_t = 0;
					} else {
						delta_t -= queue_time;
						begin_time = vehicle->get_timestamp () - delta_t;
					}
				} else {
					travel_times[segment_index].time += delta_t;
					distance += velocity * delta_t;
					delta_t = 0;
				}
			}
		}
	};

	/**
	 * Reset travel times once they've been used for network state.
	 * @param i which time to update
	 */
	void Particle::reset_travel_time (unsigned i) {
		if (i < travel_times.size ()) {
			travel_times[i].initialized = false;
		}
	};

	/**
	 * Calculate the expected time until arrival (ETA) for each future stop
	 * along the route.
	 */
	void Particle::calculate_etas (void) {
		if (etas.size () > 0) {
			// std::cerr << "Particle already has ETAs; something went wrong!\n";
			return;
		}

		if (!vehicle->get_trip () || !vehicle->get_trip ()->get_route () ||
			vehicle->get_trip ()->get_route ()->get_stops ().size () == 0) {
			std::cerr << "Particle's vehicle doesn't has trip/route/stops. Cannot predict!\n";
			return;
		}
		// Seems OK - lets go!
		auto stops = vehicle->get_trip ()->get_route ()->get_stops ();
		if (stops.back ().shape_dist_traveled == 0) return;

		// only M-1 stops to predict (can't do the first one)
		etas.reserve (stops.size ());
		etas.emplace_back (0); // first one is always 0
		for (unsigned int i=1; i<stops.size (); i++) {
			// STOP INDEX is 1-based; stop 0-index of CURRENT is stop_index-1.
			if (stops[i].shape_dist_traveled <= distance) {
				etas.emplace_back (0);
			} else {
				etas.emplace_back (vehicle->get_timestamp () +
					(int)round((1 / velocity) * (stops[i].shape_dist_traveled - distance)));
			}
		}
	};

}; // end namespace gtfs
