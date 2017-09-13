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
	* The ID is automatically selected from the parent vehicle.
	* Values are computed based on the approximate location of the bus,
	* allowing for noise.
	* RNG required otherwise the particles would all be identical!
	*
	* @param v the vehicle object pointer to which the particle belongs
	*/
	Particle::Particle (Vehicle* v) :
		id (v->allocate_id ()),
		// parent_id (0),
		// distance (0),
		// velocity (0),
		// finished (false),
		// stop_index (0),
		// arrival_time (0),
		// dwell_time (0),
		// segment_index (0),
		// queue_time (0),
		// begin_time (0),
		// log_likelihood (-INFINITY),
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

		start = p.get_start ();
		latest = p.get_latest ();
		// only copy up to latest
		// for (int i=0; i<=latest; i++) trajectory.emplace_back (p.get_distance (i), p.get_velocity (i));
		trajectory = p.get_trajectory ();
		velocity = p.get_velocity ();
		stops = p.get_stops ();
		segments = p.get_segments ();
		log_likelihood = 0;  // p.get_likelihood ();

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
		// std::clog << " - Particle " << id << " deleted." << std::endl;
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

	// /**
	//  * Computes the particle's distance into trip at the given time point.
	//  * @param  t time
	//  * @return   the distance into trip (meters)
	//  */
	// double Particle::get_distance (uint64_t& t) const {
	// 	unsigned k = std::max(0, (int)(t - start));
	// 	if (k >= trajectory.size ()) k = trajectory.size () - 1;
	// 	return trajectory[k];
	// };
	//
	// /**
	// * Computes the particle's distance into trip at the given time point.
	// * @param  k time (since start)
	// * @return   the distance into trip (meters)
	// */
	// double Particle::get_distance (int k) const {
	// 	if (k < 0) k = 0;
	// 	if (k > trajectory.size ()) k = trajectory.size () - 1;
	// 	return trajectory[k];
	// };

	double Particle::get_distance ( void ) const {
		if (trajectory.size () == 0) return 0.0;
		return trajectory.back ();
	}

	// /** @return the distance into trip of particle at latest update */
	// double Particle::get_distance (void) const {
	// 	return get_distance (latest);
	// };

	/**
	* Computes the particle's velocity at the given time point.
	* @param  k time (since start)
	* @return   the velocity (meters per second)
	*/
	double Particle::get_velocity (void) const {
		// begining/end of trip, velocity is 0
		// if (latest <= 0 || latest >= trajectory.size ()-1) return 0.0;
		int k = trajectory.size ()-1;
		if (k <= 0) return velocity;
		return trajectory[k] - trajectory[k-1];
	};

	// /** @return the particle's speed at latest update */
	// double Particle::get_velocity (void) const {
	// 	return get_velocity (latest);
	// };

	/** @return the stops (arrival and dwell times) */
	std::vector<std::tuple<int,int>> Particle::get_stops (void) const {
		return stops;
	};

	/** @return the segments (queue and travel times) */
	std::vector<std::tuple<int,int>> Particle::get_segments (void) const {
		return segments;
	};




	// --- METHODS

	// /**
	//  * Initialize a particle at the start of the route.
	//  * @param rng a random number generator
	//  */
	// void Particle::initialize (sampling::RNG& rng) {
	// 	*this->initialize (0.0, rng);
	// };

	/**
	* Initialize particle with distance etc.
	* @param dist  a distance that we want the particle to be *curently*
	* @param rng   a random number generator
	*/
	void Particle::initialize (double dist, sampling::RNG& rng) {

		trajectory.clear ();
		trajectory.emplace_back (0);
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
			trajectory.emplace_back (0);
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
			trajectory.emplace_back (d);
		}
	}

	void Particle::mutate ( sampling::RNG& rng ) {
		return mutate (rng, -1.0);
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
		if (segments.size () == 0) return;

		// create trajectories
		double Dmax (vehicle->get_trip ()->get_route ()->get_stops ().back ().shape_dist_traveled);
		if (dist >= 0) Dmax = fmin(Dmax, dist);

		double sigmav (2.0);
		double amin (-5.0);
		double Vmax (30.0);
		double pi (0.5);
		double gamma (6);
		double tau (10);
		auto rtau = sampling::exponential (1 / tau);
		double rho (0.5);
		double theta (20);
		auto rtheta = sampling::exponential (1 / theta);

		// std::vector<std::tuple<double,double> > newtraj;
		// for (int i=0; i<=latest; i++) newtraj.push_back (trajectory[i]);
		// trajectory.clear ();
		// for (auto tr: newtraj) trajectory.push_back (tr);
		// trajectory = newtraj;
		// newtraj.clear ();

		double d (get_distance ()), v (get_velocity ());
		if (dist > 300) d = dist - 300.0; // so we're not generating so much useless stuff
		int J (stops.size ());
		int L (segments.size ());
		int j = 1, l = 1;
		// quickly find which segment the start location is in=
		while (stops[j].shape_dist_traveled <= d && j < J) j++;
		while (segments[l].shape_dist_traveled <= d && l < L) l++;

		// std::cout << "\nParticle - trajectory length: " << trajectory.size ()
		// << ", latest = " << latest << ", DMAX = " << Dmax
		// << " ==> distance = " << d << " , speed = " << v;
		// std::cout.flush ();

		double dmax;
		int pstops (-1); // does the particle stop at the next stop/intersection?
		while (d < Dmax &&
			   (latest == -1 || start + trajectory.size () < vehicle->get_timestamp ())) {
			// initial wait time
			if (v == 0 || vehicle->get_dmaxtraveled () >= 0) {
				if (rng.runif () < 0.8) {
					trajectory.push_back (d);
					continue;
				}
			}

			// figure out dmax at the start of each step
			if (l < L && segments[l].shape_dist_traveled < stops[j].shape_dist_traveled) {
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
				// increment relevant index

				int wait = 0;
				if (dmax == segments[l].shape_dist_traveled) {
					l++;
					wait += pstops * rtheta.rand (rng);
				} else {
					// stopping at intersection
					j++;
					wait += pstops * (gamma + rtau.rand (rng));
				}
				while (wait > 0 &&
				 	   (latest == -1 || start + trajectory.size () < vehicle->get_timestamp ())) {
					trajectory.push_back (d);
					wait--;
				}
				pstops = -1;
			}
			trajectory.push_back (d);
		}

		// std::cout << " - done: length now " << trajectory.size ();

	};


	/**
	 * Compute the likelihood of the particle
	 * given the bus's reported location
	 * and stop updates.
	 */
	void Particle::calculate_likelihood (void) {


		double nllhood = 0.0;
		double sigy   = 10.0;

		// std::cout << "\n Start: " << start << "; ts: " << vehicle->get_timestamp ()
			// << " -> ";
		latest = trajectory.size () - 1;
		if (trajectory.size () == 0) latest = -1;
		// start = vehicle->get_timestamp () - latest;
		// if (latest < 0) latest = 0;
		// if (latest >= trajectory.size ()) latest = trajectory.size () - 1;
		// std::cout << "latest = " << latest;

		gps::Coord x = get_coords (
			get_distance (),
			vehicle->get_trip ()->get_route ()->get_shape () );
		std::vector<double> z (x.projectFlat(vehicle->get_position ()));

		nllhood += log (2 * M_PI * sigy);
		nllhood += (pow(z[0], 2) + pow(z[1], 2)) / (2 * pow(sigy, 2));

		log_likelihood -= nllhood;

		// double epsS = 20.0;
		// double epsI = 30.0;
		//
		// double sigDelay = 3.0; // error in seconds for arrival/departure times
		//
		// // Likelihood part 1: position
		// // if (arrival_time > 0 &&
		// // 	arrival_time + dwell_time >= vehicle->get_timestamp ()) {
		// if (at_stop) {
		// 	// particle at stop
		// 	auto stop = vehicle->get_trip ()->get_stoptimes ()[stop_index-1].stop;
		// 	if (vehicle->get_position ().distanceTo (stop->get_pos ()) < epsS) {
		// 		nllhood += (log(M_PI) + 2 * log(epsS));
		// 	} else {
		// 		nllhood = INFINITY;
		// 	}
		// // } else if (queue_time > 0 && begin_time == 0) {
		// } else if (at_intersection) {
		// 	// particle at intersection
		// 	if (segmen				t_index == 0)
		// 		nllhood = INFINITY;
		// 	auto dx = vehicle->get_trip ()->get_route ()->get_shape ()
		// 		->get_segments ()[segment_index].shape_dist_traveled;
		// 	// ----B2------B1|--------
		// 	// 00001111111111100000000 <- keep or not?
		// 	auto B1 = vehicle->get_trip ()->get_route ()->get_shape ()
		// 		->get_segments ()[segment_index].segment->get_from ()->get_pos ();
		// 	auto B2 = get_coords (dx - epsI,
		// 						  vehicle->get_trip ()->get_route ()->get_shape ());
		// 	double 		// while (trajectory.size () > latest+1) {
		// 	trajectory.pop_back ();
		// }a12 = vehicle->get_position ().alongTrackDistance (B1, B2);
		// 	double a21 = vehicle->get_position ().alongTrackDistance (B2, B1);
		// 	if (a12 <= epsI && a21 <= epsI &&
		// 		vehicle->get_position ().crossTrackDistanceTo (B1, B2) <= epsI) {
		// 		// particle is OK
		// 	} else {
		// 		nllhood = INFINITY;
		// 	}
		// 	// if (vehicle->get_position ().distanceTo (Int->get_pos ()) < epsI &&
		// 	// 	vehicle->get_position ().distanceTo (B2) < epsI) {
		// 	// 	double logA = log(2) + 2 * log(epsI) +
		// 	// 		log(acos(0.5) - acos(sqrt(3) / 4));
		// 	// 	llhood -= logA;
		// 	// }
		// } else {
		// 	// particle moving
		// 	nllhood += log (2 * M_PI * sigy);
		// 	nllhood += (pow(z[0], 2) + pow(z[1], 2)) / (2 * pow(sigy, 2));
		// }
		//
		// // int vsi = vehicle->get_stop_sequence ();
		// // if (vsi >= 0) {
		// // 	if (vsi > stop_index) {
		// // 		nllhood = INFINITY;
		// // 	} else if (vsi < stop_index) {
		// // 		if (arrival_time > fmax(vehicle->get_arrival_time (),
		// // 							 	vehicle->get_departure_time ())) {
		// // 			nllhood = INFINITY;
		// // 		}
		// // 	} else {
		// // 		auto Ta = vehicle->get_arrival_time ();
		// // 		auto Td = vehicle->get_departure_time ();
		// //
		// // 		if (Ta > 0 && Td > 0) {
		// // 			if (dwell_time == 0) {
		// // 				nllhood = INFINITY;
		// // 			} else {
		// // 				auto fn1 = sampling::normal (arrival_time, sigDelay);
		// // 				auto fn2 = sampling::normal (dwell_time, sigDelay);
		// // 				nllhood -= fn1.pdf_log (Ta) + fn2.pdf_log (Td - Ta);
		// // 			}
		// // 		} else if (Ta > 0) {
		// // 			auto fn = sampling::normal (arrival_time + dwell_time, sigDelay);
		// // 			nllhood -= fn.pdf_log (Ta);
		// // 		} else if (Td > 0) {
		// // 			auto fn = sampling::normal (arrival_time + dwell_time, sigDelay);
		// // 			nllhood -= fn.pdf_log (Td);
		// // 		}
		// // 	}
		// // }
		//
	};

	// /**
	//  * Phase 1 of the particle's transition, involving determining initial
	//  * wait conditions.
	//  *
	//  * If the bus is at a stop, ensure it waits long enough.
	//  * Similarly for intersections, queue longer if needed.
	//  * Otherwise, there's no wait time.
	//  *
	//  * @param rng reference to the random number generator
	//  */
	// void Particle::transition_phase1 (sampling::RNG& rng, std::ofstream* f) {
	//
	// 	// if (at_stop) {
	// 	// 	// --- Particle is stopped at the bus stop
	// 	// 	double gamma (6.0);
	// 	// 	sampling::exponential exptau (1.0 / 10.0);
	// 	//
	// 	// 	// min dwell time is 3 seconds
	// 	// 	if (dwell_time < gamma) dwell_time = gamma;
	// 	// 	dwell_time += (int) exptau.rand (rng);
	// 	// 	delta_t = vehicle->get_timestamp () - (arrival_time + dwell_time);
	// 	// 	if (delta_t > 0) {
	// 	// 		at_stop = false;
	// 	// 		if (f)
	// 	// 			*f << id << "," << vehicle->get_trip ()->get_id () << ","
	// 	// 				<< arrival_time + dwell_time << "," << distance << ","
	// 	// 				<< "depart" << "," << parent_id << "," << log_likelihood << ","
	// 	// 				<< weight << "\n";
	// 	// 	}
	// 	// } else if (at_intersection) {
	// 	// 	// --- Particle is queued at an intersection
	// 	// 	sampling::exponential expkappa (1.0 / 20.0);
	// 	// 	int qt = (int) expkappa.rand (rng);
	// 	// 	if (qt < delta_t) {
	// 	// 		// particle queues LESS than time remaining; goes through
	// 	// 		queue_time += qt;
	// 	// 		delta_t -= qt;
	// 	// 		begin_time = vehicle->get_timestamp () - delta_t;
	// 	// 		at_intersection = false;
	// 	// 		if (f)
	// 	// 			*f << id << "," << vehicle->get_trip ()->get_id () << ","
	// 	// 				<< begin_time << "," << distance << ","
	// 	// 				<< "begin" << "," << parent_id << "," << log_likelihood << ","
	// 	// 				<< weight << "\n";
	// 	// 	} else {
	// 	// 		// particle queues too long - remains at intersection
	// 	// 		queue_time += delta_t;
	// 	// 		delta_t = 0;
	// 	// 	}
	// 	// } else {
	// 	// 	// for now we'll just stay where we are with some probability
	// 	// 	// if not at stop/segment
	// 	// 	// if (rng.runif () < 0.1) {
	// 	// 	// 	sampling::exponential expz (1.0 / delta_t);
	// 	// 	// 	int wait = (int) expz.rand (rng);
	// 	// 	// 	travel_times[segment_index].time += std::min(delta_t, wait);
	// 	// 	// 	delta_t -= wait;
	// 	// 	// 	if (f)
	// 	// 	// 		*f << id << "," << vehicle->get_trip ()->get_id () << ","
	// 	// 	// 			<< vehicle->get_timestamp () - std::max(0, delta_t) << "," << distance << ","
	// 	// 	// 			<< "wait" << "," << parent_id << "," << log_likelihood << ","
	// 	// 	// 			<< weight << "\n";
	// 	// 	// }
	// 	// }
	//
	// };
	//
	// /**
	//  * Phase 2 of the particle's transition, involving adding random
	//  * system noise.
	//  * Uses rejection sampling.
	//  *
	//  * @param rng reference to the random number generator
	//  */
	// void Particle::transition_phase2 (sampling::RNG& rng) {
	// 	// double sigv (2);
	// 	// double vel;
	// 	// bool reject (true);
	// 	// while (reject) {
	// 	// 	vel = velocity + rng.rnorm () * sigv;
	// 	// 	reject = vel < 2 || vel > 30;
	// 	// }
	// 	// velocity = vel;
	// };
	//
	// /**
	//  * Phase 3 of the particle's transition, involving movement
	//  * until time's up.
	//  *
	//  * @param rng reference to the random number generator
	//  */
	// void Particle::transition_phase3 (sampling::RNG& rng, std::ofstream* f) {
	// 	// // double dmax;
	// 	// auto trip = vehicle->get_trip ();
	// 	// if (!trip) return;
	// 	// auto route = trip->get_route ();
	// 	// if (!route) return;
	// 	// auto shape = route->get_shape ();
	// 	// if (!shape) return;
	// 	// if (route->get_stops ().size () == 0) return;
	// 	// auto stops = route->get_stops ();
	// 	//
	// 	// if (shape->get_segments ().size () == 0) return;
	// 	// auto segments = shape->get_segments ();
	// 	//
	// 	// if (!segments.back ().segment) {
	// 	// 	finished = true;
	// 	// 	return;
	// 	// }
	// 	//
	// 	// int M (stops.size ());
	// 	// int L (segments.size());
	// 	//
	// 	// while (delta_t > 0 && !finished) {
	// 	// 	/**
	// 	// 	 * # NOTES
	// 	// 	 * STOPS are use 1-based indexing to match the GTFS feed
	// 	// 	 * Therefore, the FIRST stop is stop_index = 1, which is stops[0].
	// 	// 	 * Therefore, the ith stop is stop_index = i, which is stops[i-1].
	// 	// 	 * To make things more complicated, the (i+1)th stop is stop+index = i+1,
	// 	// 	 * 								which is stops[i]!!!
	// 	// 	 *
	// 	// 	 * However, segments are 0-indexed, so a particle with segment_index=1 is
	// 	// 	 * is on the SECOND (i+1th) segment.
	// 	// 	 */
	// 	// 	// distance of next stop, segment
	// 	// 	double Sd = stops[stop_index].shape_dist_traveled;
	// 	// 	double Rd;
	// 	// 	if (segment_index + 1 >= L) {
	// 	// 		// on last segment
	// 	// 		Rd = stops.back ().shape_dist_traveled;
	// 	// 	} else {
	// 	// 		Rd = segments[segment_index+1].shape_dist_traveled;
	// 	// 	}
	// 	// 	if (Sd <= Rd) { // Next up: STOP! (including if segment)
	// 	// 		double eta = (1 / velocity) * (Sd - distance);
	// 	// 		if (eta <= delta_t) {
	// 	// 			delta_t = delta_t - eta;
	// 	// 			travel_times[segment_index].time += eta;
	// 	// 			stop_index++;
	// 	// 			at_stop = true;
	// 	// 			arrival_time = vehicle->get_timestamp () - delta_t;
	// 	// 			dwell_time = 0;
	// 	// 			distance = Sd;
	// 	// 			if (f)
	// 	// 				*f << id << "," << vehicle->get_trip ()->get_id () << ","
	// 	// 					<< arrival_time << "," << distance << ","
	// 	// 					<< "arrive" << "," << parent_id << "," << log_likelihood << ","
	// 	// 					<< weight << "\n";
	// 	// 			if (stop_index == M) {
	// 	// 				finished = true;
	// 	// 				travel_times[segment_index].complete = true;
	// 	// 				delta_t = 0;
	// 	// 				break;
	// 	// 			}
	// 	// 			double pi (0.5);
	// 	// 			double gamma (6.0);
	// 	// 			sampling::exponential exptau (1.0 / 10.0);
	// 	// 			if (rng.runif () < pi) dwell_time = (int) gamma + exptau.rand (rng);
	// 	// 			delta_t -= dwell_time;
	// 	// 			at_stop = delta_t <= 0;
	// 	//
	// 	// 			if (!at_stop && f)
	// 	// 				*f << id << "," << vehicle->get_trip ()->get_id () << ","
	// 	// 					<< arrival_time + dwell_time << "," << distance << ","
	// 	// 					<< "depart" << "," << parent_id << "," << log_likelihood << ","
	// 	// 					<< weight << "\n";
	// 	// 		} else {
	// 	// 			travel_times[segment_index].time += delta_t;
	// 	// 			distance += velocity * delta_t;
	// 	// 			delta_t = 0;
	// 	// 			if (f)
	// 	// 				*f << id << "," << vehicle->get_trip ()->get_id () << ","
	// 	// 					<< vehicle->get_timestamp () << "," << distance << ","
	// 	// 					<< "travel" << "," << parent_id << "," << log_likelihood << ","
	// 	// 					<< weight << "\n";
	// 	// 		}
	// 	// 	} else {    // Next up: INTERSECTION!
	// 	// 		double eta = (1 / velocity) * (Rd - distance);
	// 	// 		if (eta <= delta_t) {
	// 	// 			delta_t = delta_t - eta;
	// 	// 			// std::cout << "\n > entering intersection; "
	// 	// 			// 	<< "there are " << travel_times.size () << " times initialized ...";
	// 	// 			travel_times[segment_index].time += eta;
	// 	// 			travel_times[segment_index].complete = true;
	// 	// 			// std::cout << "\n > segment " << segment_index
	// 	// 			// 	<< " traversed in " << travel_times[segment_index].time << "s";
	// 	// 			// arrive at next intersection
	// 	// 			segment_index++;
	// 	// 			at_intersection = true;
	// 	// 			travel_times[segment_index].initialized = true;
	// 	// 			queue_time = 0;
	// 	// 			begin_time = 0;
	// 	// 			distance = Rd;
	// 	// 			if (f)
	// 	// 				*f << id << "," << vehicle->get_trip ()->get_id () << ","
	// 	// 					<< vehicle->get_timestamp () - delta_t << "," << distance << ","
	// 	// 					<< "end" << "," << parent_id << "," << log_likelihood << ","
	// 	// 					<< weight << "\n";
	// 	// 			if (segment_index == L) {
	// 	// 				// actually this should never happen!
	// 	// 				finished = true;
	// 	// 				break;
	// 	// 			}
	// 	// 			double rho (0.5);
	// 	// 			sampling::exponential expkappa (1.0 / 20.0);
	// 	// 			if (rng.runif () < rho) queue_time = (int) expkappa.rand (rng);
	// 	// 			// max queue time till observation; allow next iteration to extend duration
	// 	// 			if (queue_time > delta_t) {
	// 	// 				queue_time = delta_t;
	// 	// 				delta_t = 0;
	// 	// 			} else {
	// 	// 				delta_t -= queue_time;
	// 	// 				begin_time = vehicle->get_timestamp () - delta_t;
	// 	// 				at_intersection = false;
	// 	// 				if (f)
	// 	// 					*f << id << "," << vehicle->get_trip ()->get_id () << ","
	// 	// 						<< begin_time << "," << distance << ","
	// 	// 						<< "begin" << "," << parent_id << "," << log_likelihood << ","
	// 	// 						<< weight << "\n";
	// 	// 			}
	// 	// 		} else {
	// 	// 			travel_times[segment_index].time += delta_t;
	// 	// 			distance += velocity * delta_t;
	// 	// 			delta_t = 0;
	// 	// 			if (f)
	// 	// 				*f << id << "," << vehicle->get_trip ()->get_id () << ","
	// 	// 					<< vehicle->get_timestamp () << "," << distance << ","
	// 	// 					<< "travel" << "," << parent_id << "," << log_likelihood << ","
	// 	// 					<< weight << "\n";
	// 	// 		}
	// 	// 	}
	// 	// }
	// };

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
