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

		std::clog << " + Created particle for vehicle " << v->get_id ()
			<< " with id = " << id << "\n";
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
		std::clog << " >+ Copying particle " << p.get_id () << " -> ";

		distance = p.get_distance ();
		velocity = p.get_velocity ();
		finished = p.is_finished ();
		stop_index = p.get_stop_index ();
		arrival_time = p.get_arrival_time ();
		dwell_time = p.get_dwell_time ();
		segment_index = p.get_segment_index ();
		queue_time = p.get_queue_time ();
		begin_time = p.get_begin_time ();
		log_likelihood = p.get_likelihood ();

		// Copy vehicle pointer
		vehicle = p.vehicle;
		parent_id = p.id;


		// Increment particle id
		id = p.vehicle->allocate_id ();
		std::clog << id << std::endl;
	};

	/**
	* Destructor for a particle.
	*/
	Particle::~Particle() {
		std::clog << " - Particle " << id << " deleted." << std::endl;
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

		for (unsigned int i=0; i<stops.size (); i++) {
			// once distance smaller than stop's, we're done
			if (distance < stops[i].shape_dist_traveled) break;
			stop_index = i+1;
		}

		std::clog << "   - " << *this << " -> ";
		calculate_likelihood ();
		std::clog << log_likelihood <<  "\n";
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

		std::clog << "   - " << *this << " -> ";

		// --- Three phases
		// PHASE ONE: initial wait time
		transition_phase1 (rng);

		// PHASE TWO: system noise
		transition_phase2 (rng);

		// PHASE THREE: transition forward
		transition_phase3 (rng);


		std::clog << *this << " -> ";

		// Done with transition; compute likelihood and finish.
		calculate_likelihood ();
		std::clog << log_likelihood <<  "\n";
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
		double sigy   = 10;
		llhood -= log (2 * M_PI * sigy);
		llhood -= (pow(z[0], 2) + pow(z[1], 2)) / (2 * pow(sigy, 2));

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
		// for now we'll just stay where we are with some probability
		if (rng.runif () < 0.3) delta_t = 0;
	};

	/**
	 * Phase 2 of the particle's transition, involving adding random
	 * system noise.
	 *
	 * @param rng reference to the random number generator
	 */
	void Particle::transition_phase2 (sampling::RNG& rng) {
		double sigv (5);
		double vel = 0.0;
		while (vel <= 0 || vel >= 30) vel = rng.rnorm () * sigv + velocity;
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
		if (shape->get_segments ().size () == 0) return;
		auto stops = route->get_stops ();

		if (!shape->get_segments ().back ().segment) {
		// 	dmax = shape->get_segments ().back ().segment->get_length ();
		// } else {
			finished = true;
			return;
		}

		int M (stops.size ());
		int L (shape->get_segments ().size());

		while (delta_t > 0 && !finished) {
			/**
			 * # NOTES
			 * STOPS are use 1-based indexing to match the GTFS feed
			 * Therefore, the FIRST stop is stop_index = 1, which is stops[0].
			 * Therefore, the ith stop is stop_index = i, which is stops[i-1].
			 * To make things more complicated, the (i+1)th stop is stop+index = i+1,
			 * 								which is stops[i]!!!
			 */
			if (true) { // Next up: STOP!
				double eta = (1 / velocity) * (stops[stop_index].shape_dist_traveled - distance);
				// std::clog << "\n Next stop: " << stops[stop_index].shape_dist_traveled
				// 	<< "m into trip; ETA: " << eta << "\n";
				if (eta <= delta_t) {
					delta_t = delta_t - eta;
					stop_index++;
					arrival_time = vehicle->get_timestamp () - delta_t;
					dwell_time = 0;
					distance = stops[stop_index-1].shape_dist_traveled;
					if (stop_index == M) {
						finished = true;
						break;
					}

				} else {
					distance += velocity * delta_t;
					delta_t = 0;
				}
			} else {    // Next up: INTERSECTION!

			}
		}
	};

}; // end namespace gtfs
