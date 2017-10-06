#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <math.h>

#include "gtfs.h"

namespace gtfs {
	/**
	* Particle constructor.
	*
	* The ID is automatically selected from the parent vehicle. Initialized later.
	*
	* @param v the vehicle object pointer to which the particle belongs
	*/
	Particle::Particle (Vehicle* v) : id (v->allocate_id ()), vehicle (v) {

		// auto t = v->get_trip ();
		// if (!t) return;
		// auto r = t->get_route ();
		// if (!r) return;
		// auto s = r->get_shape ();
		// if (!s) return;
		// auto sg = s->get_segments ();
		// if (sg.size () == 0) return;

		// travel_times.resize (sg.size (), pTravelTime());
		// for (unsigned i=0; i<sg.size (); i++)
		// 	travel_times.emplace_back ();

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
		start = p.get_start ();
		latest = p.get_latest ();
		trajectory = p.get_trajectory ();
		velocity = p.get_velocity ();
		stop_times = p.get_stop_times ();
		travel_times = p.get_travel_times ();
		log_likelihood = 0;

		// Copy vehicle pointer
		vehicle = p.vehicle;
		parent_id = p.id;

		// Increment particle id
		id = p.vehicle->allocate_id ();
	};

	/**
	* Destructor for a particle.
	*/
	Particle::~Particle() {
		// std::clog << "\n ... deleting particle " << id;
	};


	// --- GETTERS

	/**
	* @return the particle's ID
	*/
	unsigned long Particle::get_id (void) const {
		return id;
	};

	/** @return logical, whether or not particle has a parent **/
	bool Particle::has_parent (void) const {
		return (bool)parent_id;
	}

	/**
	 * The parent_id is optional, so check it exists first.
	 * @return the parent particle's ID
	 */
	unsigned long Particle::get_parent_id (void) const {
		return parent_id.get ();
	};

	/** @return the particle's start time */
	uint64_t Particle::get_start (void) const {
		return start;
	};

	/** @return index of latest time point */
	int Particle::get_latest (void) const {
		return latest;
	};

	/** @return the particle's trajectory */
	std::vector<double> Particle::get_trajectory (void) const {
		return trajectory;
	};

	/** @return   the distance into trip (meters) */
	double Particle::get_distance ( void ) const {
		if (trajectory.size () == 0) return 0.0;
		return trajectory.back ();
	}

	/**@return   the velocity (meters per second) */
	double Particle::get_velocity (void) const {
		// begining/end of trip, velocity is 0
		// if (latest <= 0 || latest >= trajectory.size ()-1) return 0.0;
		int k = trajectory.size ()-1;
		if (k <= 0) return velocity;
		return trajectory[k] - trajectory[k-1];
	};

	/** @return the stops (arrival and dwell times) */
	std::vector<std::tuple<int,int>> Particle::get_stop_times (void) const {
		return stop_times;
	};

	/** @return the segments (queue and travel times) */
	std::vector<pTravelTime> Particle::get_travel_times (void) const {
		return travel_times;
	};

	const pTravelTime& Particle::get_travel_time (int i) const {
		return travel_times[i];
	};




	// --- METHODS

	/**
	* Initialize particle with distance etc.
	* @param dist  a distance that we want the particle to be *curently*
	* @param rng   a random number generator
	*/
	void Particle::initialize (double dist, sampling::RNG& rng) {

		auto t = vehicle->get_trip ();
		if (!t) return;
		auto r = t->get_route ();
		if (!r) return;
		auto s = r->get_shape ();
		if (!s) return;
		auto sg = s->get_segments ();
		if (sg.size () == 0) return;

		trajectory.clear ();
		trajectory.push_back (0);
		travel_times.clear ();
		travel_times.resize (sg.size ());
		// velocity = 0;
		int wait (sampling::exponential (1.0 / 20.0).rand (rng));
		if (dist == 0.0) {
			if (vehicle->get_stop_sequence () &&
			    vehicle->get_stop_sequence ().get () == 1) {
				// check if we have arrival time or departure time
				if (vehicle->get_arrival_time ()) {
					// allow waiting (+- 10seconds)
					// we hope the bus will depart around the scheduled departure time
					// (if > start AND not more than 20min away ...)
					start = vehicle->get_arrival_time ().get ();
				} else if (vehicle->get_departure_time ()) {
					start = vehicle->get_departure_time ().get () - 30;
					wait = std::max(0.0, 30 + rng.rnorm () * 5);
				}
			} else {
				start = vehicle->get_first_obs () - wait;
				wait += sampling::exponential (1.0 / 20.0).rand (rng);
			}
		}
		while (wait > 0) {
			trajectory.push_back (0);
			wait--;
		}
		latest = -1;

		mutate (rng, dist);

		// set start so that get_distance (ts-start) = dist.rand (rng);
		start = vehicle->get_timestamp ();
		latest = 0;
		if (dist > 0) {
			double d = get_distance ();
			velocity = get_velocity ();
			trajectory.clear ();
			trajectory.push_back (d);
		}
	}

	void Particle::mutate ( sampling::RNG& rng ) {
		mutate (rng, -1.0);
	}

	/**
	 * This function takes a trajectory, and re-randomises it from trajectory[latest]
	 * @param rng regerence to a random number generator
	 * @param f   a file to write particle trajectories to (for debugging...)
	 */
	void Particle::mutate ( sampling::RNG& rng, double dist ) {
		// start from point (d[latest], v[latest]), and project a /new/ path to
		// the end of the route


		auto trip = vehicle->get_trip ();
		if (!trip) return;
		auto route = trip->get_route ();
		if (!route) return;
		auto stops = route->get_stops ();
		if (stops.size () == 0) return;
		auto shape = route->get_shape ();
		if (!shape) return;
		auto segments = shape->get_segments ();
		// std::cout << "\n + Segments: ";
		// for (auto s: segments) std::cout << " - " << s.shape_dist_traveled;
		// std::cout.flush ();
		if (segments.size () == 0) return;

		// create trajectories
		double Dmax ( stops.back ().shape_dist_traveled );
		if (dist >= 0) Dmax = fmin(Dmax, dist);

		double sigmav (2.0);
		double amin (-5.0);
		double Vmax (30.0);
		double pi (0.5);
		double gamma (3);
		double tau (10);
		auto rtau = sampling::exponential (1 / tau);
		double rho (0.5);
		double theta (30);
		auto rtheta = sampling::exponential (1 / theta);

		double d (get_distance ()), v (get_velocity ());
		if (dist > 300) d = dist - 300.0; // so we're not generating so much useless stuff
		int J (stops.size ());
		int L (segments.size ());
		int j = 1, l = 1;
		// quickly find which segment the start location is in
		while (j < J && stops[j].shape_dist_traveled <= d) j++;
		while (l < L-1 && segments[l].shape_dist_traveled <= d) l++;

		double dmax;
		int pstops (-1); // does the particle stop at the next stop/intersection?
		while (d < Dmax &&
			   (latest == -1 || start + trajectory.size () < vehicle->get_timestamp ())) {
			// initial wait time
			if (v == 0 || vehicle->get_dmaxtraveled () >= 0) {
				if (rng.runif () < 0.5) {
					trajectory.push_back (d);
					continue;
				}
			}

			// figure out dmax at the start of each step
			if (l < L-1 && segments[l].shape_dist_traveled < stops[j].shape_dist_traveled) {
				dmax = segments[l].shape_dist_traveled;
				if (pstops == -1) pstops = rng.runif () < rho;
			} else {
				dmax = stops[j].shape_dist_traveled;
				if (pstops == -1) pstops = rng.runif () < pi;
			}
			double vmax, vmin = 0;
			vmax = (dmax - d) / (sqrt ((dmax - d) / -amin));
			if (pstops == 1 && vmax < Vmax) {
				velocity = sampling::uniform (vmin, vmax).rand (rng);
			} else {
				std::cout.flush ();
				auto rnorm = sampling::normal (v, sigmav);
				velocity = rnorm.rand (rng);
				while (velocity < vmin || velocity > Vmax) {
					velocity = rnorm.rand (rng);
				}
			}
			d += velocity; // dt = 1 second every time

			if (d >= dmax) {
				d = dmax;
				if (pstops == 1) v = 0; // only 0 if particle decides to stop

				int wait = 0;
				if (dmax == stops[j].shape_dist_traveled) {
					// stopping at BUS STOP
					j++;
					wait += pstops * (gamma + rtau.rand (rng));
				} else {
					if (travel_times[l].initialized) {
						travel_times[l].complete = true;
						std::cout.flush ();
					}
					l++;
					if (l >= L) {
						trajectory.push_back (d);
						return;
					}
					// stopping at INTERSECTION
					wait += pstops * rtheta.rand (rng);
					travel_times[l].initialized = true;
				}
				while (wait > 0 &&
				 	   (latest == -1 || start + trajectory.size () < vehicle->get_timestamp ())) {
					trajectory.push_back (d);
					wait--;
				}
				pstops = -1;
			}
			trajectory.push_back (d);
			if (travel_times[l].initialized && !travel_times[l].complete)
				travel_times[l].time++;
		}
	};


	/**
	 * Compute the likelihood of the particle
	 * given the bus's reported location
	 * and stop updates.
	 */
	void Particle::calculate_likelihood (void) {

		double nllhood = 0.0;
		double sigy   = 10.0;

		auto trip = vehicle->get_trip ();
		if (!trip) {
			log_likelihood = -INFINITY;
			return;
		};
		auto route = trip->get_route ();
		if (!route) {
			log_likelihood = -INFINITY;
			return;
		};
		auto shape = route->get_shape ();
		if (!shape) {
			log_likelihood = -INFINITY;
			return;
		};

		latest = trajectory.size () - 1;
		if (trajectory.size () == 0) latest = -1;

		if (latest >= 0) {
			gps::Coord x = get_coords ( get_distance (), shape );
			std::vector<double> z (x.projectFlat(vehicle->get_position ()));

			nllhood += log (2 * M_PI * sigy);
			nllhood += (pow(z[0], 2) + pow(z[1], 2)) / (2 * pow(sigy, 2));
		}

		log_likelihood -= nllhood;

	};


	// /**
	//  * Reset travel times once they've been used for network state.
	//  * @param i which time to update
	//  */
	// void Particle::reset_travel_time (unsigned i) {
	// 	// if (i < travel_times.size ()) {
	// 	// 	travel_times[i].initialized = false;
	// 	// }
	// };

	// /**
	//  * Calculate the expected time until arrival (ETA) for each future stop
	//  * along the route.
	//  */
	// void Particle::calculate_etas (void) {
	// 	// if (etas.size () > 0) {
	// 	// 	// std::cerr << "Particle already has ETAs; something went wrong!\n";
	// 	// 	return;
	// 	// }
	// 	//
	// 	// if (!vehicle->get_trip () || !vehicle->get_trip ()->get_route () ||
	// 	// 	vehicle->get_trip ()->get_route ()->get_stops ().size () == 0) {
	// 	// 	std::cerr << "Particle's vehicle doesn't has trip/route/stops. Cannot predict!\n";
	// 	// 	return;
	// 	// }
	// 	// // Seems OK - lets go!
	// 	// auto stops = vehicle->get_trip ()->get_route ()->get_stops ();
	// 	// if (stops.back ().shape_dist_traveled == 0) return;
	// 	//
	// 	// // only M-1 stops to predict (can't do the first one)
	// 	// etas.reserve (stops.size ());
	// 	// etas.emplace_back (0); // first one is always 0
	// 	// for (unsigned int i=1; i<stops.size (); i++) {
	// 	// 	// STOP INDEX is 1-based; stop 0-index of CURRENT is stop_index-1.
	// 	// 	if (stops[i].shape_dist_traveled <= distance) {
	// 	// 		etas.emplace_back (0);
	// 	// 	} else {
	// 	// 		etas.emplace_back (vehicle->get_timestamp () +
	// 	// 			(int)round((1 / velocity) * (stops[i].shape_dist_traveled - distance)));
	// 	// 	}
	// 	// }
	// };

}; // end namespace gtfs
