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
	Particle::Particle (Vehicle* v) : vehicle (v), parent_id (0) {
		id = v->allocate_id ();
		std::clog << " + Created particle for vehicle " << v->get_id ()
			<< " with id = " << id << "\n";
		distance = 0;
		velocity = 0;
		finished = false;
		stop_index = 0;
		arrival_time = 0;
		dwell_time = 0;
		segment_index = 0;
		queue_time = 0;
		begin_time = 0;
		log_likelihood = -INFINITY;
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

		std::clog << "   - " << *this << " -> ";

		double sigv (2);
		double vel = 0.0;
		while (vel <= 0 || vel >= 30) vel = rng.rnorm () * sigv + velocity;
		velocity = vel;

		double dmax = vehicle->get_trip ()->get_route ()
			->get_shape ()->get_segments ().back ().segment->get_length ();
		distance = distance + velocity * vehicle->get_delta ();
		if (distance >= dmax) {
			distance = dmax;
			finished = true;
		}
		std::clog << *this << " -> ";

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
		double sigy   = 5;
		llhood -= log (2 * M_PI * sigy);
		llhood -= (pow(z[0], 2) + pow(z[1], 2)) / (2 * pow(sigy, 2));

		log_likelihood = llhood;
	};

}; // end namespace gtfs
