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

		// stop_times.resize (sg.size (), {0, 0});
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
		start = p.get_start () + p.get_latest (); // p.get_start ();
		latest = 0; // -- start from the end of the trajectory - the other stuff doesn't matter!! p.get_latest ();
		// for (int k=0; k<=latest; k++) trajectory.push_back (p.get_distance (k));
		trajectory.push_back (p.get_distance ());
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
		if (latest < 0) {
			if (trajectory.size () == 0) return 0.0;
			return trajectory.back ();
		}
		// unsigned k = latest;
		return get_distance (latest);
	}

	/** @return distance at time start+k */
	double Particle::get_distance (unsigned k) const {
		if (trajectory.size () == 0) return 0.0;
		if (k < trajectory.size ()) return trajectory[k];
		std::clog << "\n *** NOTE: requesting " << k << " of " << trajectory.size ();
		return (trajectory.back ());
	};

	/**@return   the velocity (meters per second) */
	double Particle::get_velocity (void) const {
		// begining/end of trip, velocity is 0
		if (get_latest () < 1 || (int)trajectory.size () <= get_latest ()) return velocity;
		return trajectory[get_latest ()] - trajectory[get_latest () - 1];
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
		auto st = r->get_stops ();
		if (st.size () == 0) return;

		trajectory.clear ();
		start = 0;
		latest = -1;

        // manually reset them all ...
        travel_times.clear ();
		travel_times.resize (sg.size ());
        // for (auto& tt: travel_times) tt.reset ();

		stop_times.clear ();
		stop_times.resize (st.size ());
		for (unsigned i=0; i<st.size (); i++) stop_times.emplace_back (0, 0);
		velocity = 0;
		int wait = 0 ; //(sampling::exponential (1.0 / 20.0).rand (rng));
		double dx = dist;
		if (dist == 0.0) {
			if (vehicle->get_stop_sequence () &&
			    vehicle->get_stop_sequence ().get () == 1) {
				// check if we have arrival time or departure time
				if (vehicle->get_arrival_time ()) {
					// allow waiting (+- 10seconds)
					// we hope the bus will depart around the scheduled departure time
					// (if > start AND not more than 20min away ...)
					// start = vehicle->get_arrival_time ().get ();
					int wait = sampling::exponential (1.0 / 20.0).rand (rng);
				} else if (vehicle->get_departure_time ()) {
					// start = vehicle->get_departure_time ().get () - 30;
					wait = std::max(0.0, 30 + rng.rnorm () * 5);
				}
			} else {
				// start = vehicle->get_first_obs () - wait;
				wait += sampling::exponential (1.0 / 20.0).rand (rng);
			}
			trajectory.push_back (0.0);
			// std::clog << "0.0, ";
		} else if (dist > 300) {
			dx = dist - 300;
			trajectory.clear ();
			trajectory.push_back (dx);
			// std::clog << dx << ", ";
		}
		while (wait > 0) {
			trajectory.push_back (dx);
			// std::clog << dx << ", ";
			wait--;
		}

		// generate a (full) path, up to dist ...
		mutate (rng, dist);

		// then figure out which point is closest to that dist,
		// and set `k = ts - start`
		// while (latest < trajectory.size () && trajectory[latest] <= dist) latest++;
		latest = trajectory.size () - 1;
		start = vehicle->get_timestamp () - latest;

		// std::clog << "\n -> CURRENT = " << latest << " = " << trajectory[latest]
		// 	<< " -> get_distance () = " << get_distance ();

		// trajectory.resize (k+1);

		// set start so that get_distance (ts-start) = dist.rand (rng);
		// if (start == 0)
		// 	start = vehicle->get_timestamp ();
		// latest = 0;
		// if (dist > 0) {
		// 	double d = get_distance ();
		// 	velocity = get_velocity ();
		// 	// trajectory.clear ();
		// 	// trajectory.push_back (d);
		// }
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

		// std::clog << "\n ** start at " << get_distance () << ", (-mutate-to-" << dist << "-) ";

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

		// trip trajectory
		if (latest >= 0 && (int)trajectory.size () - 1 > get_latest ()) {
			// std::clog << "\n -> Resizing from " << trajectory.size ();
			trajectory.resize (get_latest () + 1);
			// std::clog << " to " << trajectory.size ();
		}

		// create trajectories
		double Dmax ( stops.back ().shape_dist_traveled );
		if (dist >= 0) Dmax = fmin(Dmax, dist);

		double sigmav (12.0);
		double amin (-5.0);
		double Vmax (30.0);
		double pi (0.5);
		double gamma (3.0);
		double tau (6.0);
		auto rtau = sampling::exponential (1 / tau);
		double rho (0.3);
		double theta (15);
		auto rtheta = sampling::exponential (1 / theta);

		double d (get_distance ()), v (get_velocity ());
		int J (stops.size ());    // the number of stops
		int L (segments.size ()); // the number of segments

        int j = 0, l = 0;
		// quickly find which segment the start location is in
		while (j < J-1 && d > stops[j+1].shape_dist_traveled) j++;
		while (l < L-1 && d > segments[l+1].shape_dist_traveled) l++;

        // so stop[j] = THE CURRENT STOP (i.e., most recently visited)
        //    stop[j+1] = THE NEXT (upcoming) STOP
        // and seg[l] = THE CURRENT SEGMENT INDEX
        //     seg[l+1] = THE NEXT INTERSECTION/SEGMENT ...

        // std::clog << " > on stop " << j << " of " << J << " and segment " << l << " of " << L << "...";

        if (d == 0) travel_times[0].initialized = true;

        double dmax;
		int pstops (-1); // does the particle stop at the next stop/intersection?
		while (d < Dmax &&
			   (latest == -1 || start + trajectory.size () < vehicle->get_timestamp () + 60)) {

			// initial wait time
			if (v == 0 || vehicle->get_dmaxtraveled () >= 0) {
				if (rng.runif () < 0.9) {
					trajectory.push_back (d);
					if (d == stops[j].shape_dist_traveled)
						std::get<1> (stop_times[j])++;

					if (d != stops[j].shape_dist_traveled &&
						d != segments[l].shape_dist_traveled &&
						travel_times[l].initialized && !travel_times[l].complete &&
                        (start == 0 || start + trajectory.size () < vehicle->get_timestamp ()))
						travel_times[l].time++;
					continue;
				}
			}


			// figure out dmax at the start of each step
            // it's either the NEXT stop, or the NEXT intersection
			if (l < L-1 && segments[l+1].shape_dist_traveled < stops[j+1].shape_dist_traveled) {
				dmax = segments[l+1].shape_dist_traveled;
				if (pstops == -1) pstops = rng.runif () < rho;
			} else {
				dmax = stops[j+1].shape_dist_traveled;
				if (pstops == -1) pstops = rng.runif () < pi;
			}
			double vmax = Vmax, vmin = 2;
			if (pstops) {
				// if particle going to stop, then restricted by either
				//   a, max speed OR max speed to slow down in time, whichever is lower
				vmax = std::fmin ( Vmax, (dmax - d) / sqrt ((dmax - d) / -amin) );
				// AND min speed is now 0
				vmin = 0;
			}

			auto rnorm = sampling::normal (0, sigmav);
			velocity = v + rnorm.rand (rng);
			int nattempt = 0;
			while (velocity < vmin || velocity > vmax) {
				if (nattempt > 20) {
					velocity = sampling::uniform (vmin, vmax).rand (rng);
				} else {
					velocity = v + rnorm.rand (rng) * nattempt / 10;
					nattempt++;
				}
			}

			d += velocity; // dt = 1 second every time

			if (d >= dmax) {
				d = dmax;
                trajectory.push_back (d);
                if (travel_times[l].initialized && !travel_times[l].complete &&
                    (start == 0 || start + trajectory.size () < vehicle->get_timestamp ()))
                    travel_times[l].time++;

				if (pstops == 1) v = 0; // only 0 if particle decides to stop

				int wait = 0;
				if (dmax == stops[j+1].shape_dist_traveled) {
					// stopping at NEXT BUS STOP (j+1)
					j++;
					std::get<0> (stop_times[j]) = trajectory.size ();
					if (j == J-1) break;

					wait += pstops * (gamma + rtau.rand (rng));
					std::get<1> (stop_times[j]) = wait;
				} else {
					// stopping at NEXT INTERSECTION (l+1)
                    if (travel_times[l].initialized && 
                        (start == 0 || start + trajectory.size () < vehicle->get_timestamp ())) {
                        travel_times[l].complete = true;
                    }
                    l++;
                    if (l == L-1) break;
                        
					wait += pstops * rtheta.rand (rng);
                    if (start == 0 || start + trajectory.size () < vehicle->get_timestamp ())
    					travel_times[l].initialized = true;
				}
				while (wait > 0 &&
				 	   (latest == -1 || start + trajectory.size () < vehicle->get_timestamp () + 60)) {
					trajectory.push_back (d);
					wait--;
				}
				pstops = -1;
			} else {
    			trajectory.push_back (d);
    			if (travel_times[l].initialized && !travel_times[l].complete &&
                    (start == 0 || start + trajectory.size () < vehicle->get_timestamp ()))
    				travel_times[l].time++;
            }
		}

        if (start > 0) {
    		// make sure the trajectory is long enough!
			latest = vehicle->get_timestamp () - start;
    		// while (start + trajectory.size () < vehicle->get_timestamp ())
    		while (trajectory.size () <= latest)
    			trajectory.push_back (d);

    		// then update latest
			// std::clog << "\n -> Latest = " << latest << ", length = " << trajectory.size ();
		}
	};


	/**
	 * Compute the likelihood of the particle
	 * given the bus's reported location
	 * and stop updates.
	 */
	void Particle::calculate_likelihood (void) { calculate_likelihood (1); };

	/**
	 * Compuate likelihood.
	 * @param mult GPS error multiplier
	 */
	void Particle::calculate_likelihood (int mult) {

		double nllhood = 0.0;
		double sigy = 5.0 * mult;
		double sigx = 10.0;

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

		// i.e., if ts == start, then latest = 0 which makes perfect sense <(o.o)>
		// latest = vehicle->get_timestamp () - start;
		// if (trajectory.size () == 0) latest = -1;
		if ((int)trajectory.size () <= latest) {
			// std::clog << "\n x Something is wrong ... not enough values - " 
			// 	<< trajectory.size () << " vs " << get_latest ();
		}

		if (get_latest () >= 0 && get_latest () < (int)trajectory.size ()) {
			gps::Coord x = get_coords ( get_distance (), shape );
			std::vector<double> z (x.projectFlat(vehicle->get_position ()));

			nllhood += log (2 * M_PI * sigy);
			nllhood += (pow(z[0], 2) + pow(z[1], 2)) / (2 * pow(sigy, 2));
		}

		// arrival/departure times ...
		if (false && stop_times.size () > 0 && vehicle->get_stop_sequence ()) {

			// indexing in PB is 1-based;
			unsigned parr = 0, pdep = 0, sj = 0;
			if (vehicle->get_stop_sequence ().get () > 0 && sj < stop_times.size ()) {
				sj = vehicle->get_stop_sequence ().get () - 1;
				parr = std::get<0> (stop_times[sj]);
				pdep = parr + std::get<1> (stop_times[sj]);
			} else {
				std::clog << "\n .. hmmm -> sj = " << sj << ", but only "
					<< stop_times.size () << " stops ...";
			}

			int varr = 0, vdep = 0;
			if (vehicle->get_arrival_time () && 
				vehicle->get_timestamp () >= vehicle->get_arrival_time (sj)) {
				varr = vehicle->get_arrival_time (sj) - start;
			}
			if (vehicle->get_departure_time () && 
				vehicle->get_timestamp () >= vehicle->get_departure_time (sj)) {
				vdep = vehicle->get_departure_time (sj) - start;
			}

			// Currently no way of dealing with intermediate stops that were
			// arrived at BETWEEN observations ... :( 
			if (varr > 0 && vdep > 0) {
				// vehicle has arrived & departed that stop ...
				if (parr > 0 && pdep > 0) {
					unsigned pdwell = pdep - parr,
					         vdwell = vdep - varr;
					int tdiff (parr - varr);
					// x | mu, sigma ~ N(mu, sigma)
					nllhood += 0.5 * log (2 * M_PI) + log (sigx);
					nllhood += pow (tdiff, 2) / (2 * pow (sigx, 2));
					// x | lambda ~ Exp(lambda), lambda = parameter (i.e., particle dwell time)
					nllhood += log (pdwell) + vdwell / pdwell;
				} else {
					nllhood = INFINITY;
				}
			} else if (varr > 0) {
				// vehicle has arrived, hasn't necessarily departed ...
				if (parr > 0) {
					int tdiff (parr - varr);
					// x | mu, sigma ~ N(mu, sigma)
					nllhood += 0.5 * log (2 * M_PI) + log (sigx);
					nllhood += pow (tdiff, 2) / (2 * pow (sigx, 2));
				} else {
					nllhood = INFINITY;
				}
			} else if (vdep > 0) {
				// vehicle has departed, don't know its arrival time ...
				if (pdep > 0) {
					int tdiff (pdep - vdep);
					// x | mu, sigma ~ N(mu, sigma)
					nllhood += 0.5 * log (2 * M_PI) + log (sigx);
					nllhood += pow (tdiff, 2) / (2 * pow (sigx, 2));
				} else {
					nllhood = INFINITY;
				}
			}
		}

		log_likelihood = -nllhood;

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

	/**
	 * Calculate the expected time until arrival (ETA) for each future stop
	 * along the route.
	 */
	void Particle::calculate_etas (sampling::RNG& rng) {
		// if (etas.size () > 0) {
		// 	std::cerr << "Particle already has ETAs; something went wrong!\n";
		// 	return;
		// }
		
		etas.clear ();
		if (finished) return;
		
		if (!vehicle->get_trip () || !vehicle->get_trip ()->get_route () ||
			vehicle->get_trip ()->get_route ()->get_stops ().size () == 0) {
			std::cerr << "Particle's vehicle doesn't has trip/route/stops. Cannot predict!\n";
			return;
		}
		// Seems OK - lets go!
		auto route = vehicle->get_trip ()->get_route ();
		if (!route) return;
		auto stops = route->get_stops ();
		if (stops.size () == 0 || stops.back ().shape_dist_traveled == 0) return;
		auto shape = route->get_shape ();
		if (!shape) return;
		auto segments = shape->get_segments ();
		if (segments.size () == 0 || segments.back ().shape_dist_traveled == 0) return;

		double distance = get_distance ();
		double vel = get_velocity ();
		int J (stops.size ());    // the number of stops
		int L (segments.size ()); // the number of segments
        int j, l;

		std::vector<double> seglens (L, 0);
		std::vector<double> segspeeds (L, 0);
		// std::clog << "\n *** Segments: [" << L << "]";
		for (l=0; l<L; l++) {
			double len;
			if (l < L-1) {
				len = segments[l+1].shape_dist_traveled - segments[l].shape_dist_traveled;
			} else {
				len = stops.back ().shape_dist_traveled - segments[l].shape_dist_traveled;
			}

			if (segments[l].segment && 
				segments[l].segment->get_timestamp () > 0) {
				vel = 0;
				int natt = 0;
				double trtime;
				while (vel <= 0 || vel > 30) {
					trtime = rng.rnorm () * segments[l].segment->get_travel_time_var () + 
						segments[l].segment->get_travel_time ();
					vel = len / trtime;
					natt++;
					if (natt == 20) vel = 12;
				}
			} else {
				vel = 0; //get_velocity ();
				while (vel <= 0 || vel > 30) {
					vel = rng.rnorm () * 5 + 15;
				}
			}
			seglens[l] = len;
			segspeeds[l] = vel;
			// std::clog << "\n [" << l << "]: " << len << "m long, "
				// << vel << "m/s";
		}
		
		// only M-1 stops to predict (can't do the first one)
		etas.resize (stops.size (), 0);

		l = 0;
		while (l < L-1 && distance > segments[l+1].shape_dist_traveled) l++;

		vel = segspeeds[l];
		// std::clog << "\n * ETAs for " << etas.size () << " stops... " 
		// 	<< vel << "m/s:";

		int tt = 0; // travel time up to last intersection
		for (j=0; j<J; j++) {
			if (stops[j].shape_dist_traveled <= distance) continue;

			// If the next stop is farther than the next intersection ...
			if (l < L-1 && stops[j].shape_dist_traveled > segments[l+1].shape_dist_traveled) {
				// creep forward ...
				l++;
				// std::clog << "\n -- intersection [" << segments[l].shape_dist_traveled << "] --";
				if (segments[l-1].shape_dist_traveled < distance) {
					tt = (segments[l].shape_dist_traveled - distance) / vel;
				} else {
					tt += seglens[l-1] / vel;
				}
				vel = segspeeds[l];
				// std::clog << " speed = " << vel;
			}

			double deltad;
			if (segments[l].shape_dist_traveled < distance) {
				deltad = stops[j].shape_dist_traveled - distance;
			} else {
				deltad = stops[j].shape_dist_traveled - segments[l].shape_dist_traveled;
			}
			etas[j] = vehicle->get_timestamp () + tt + round(deltad / vel);;

			// std::clog << "\n [" << j+1 << "/" << J << ", "
			// 	<< stops[j].shape_dist_traveled << "m]: "
			// 	// << (stops[j].shape_dist_traveled - distance) << "m away - "
			// 	<< tt << " + " << round(deltad / vel) << "s + "
			// 	<< vehicle->get_timestamp () << " = "
			// 	<< etas[j];
		}
	};

}; // end namespace gtfs
