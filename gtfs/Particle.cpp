#include <string>
#include <iostream>
#include <vector>

#include "gtfs.h"

namespace gtfs {
	/**
	* Particle constructor.
	*
	* The ID is automatically selected from the parent vehicle.
	* Values are computed based on the approximate location of the bus,
	* allowing for noise.
	* RNG required otherwise the particles would all be identical!
	*/
	Particle::Particle (Vehicle* v) : vehicle (v), parent_id (0) {
		id = v->allocate_id ();
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
	 * @param unif a uniform number generator with parameters a and b
	 * @param rng  a random number generator
	 */
	void Particle::initialize (sampling::uniform& dist, sampling::uniform& speed, sampling::RNG& rng) {
		distance = dist.rand (rng);
		velocity = speed.rand (rng);

		std::cout << "   - " << *this << " -> ";
		calculate_likelihood ();
		std::cout << log_likelihood <<  "\n";
	}

	/**
	 * Move the particle according to speed,
	 * shape/segments/stops, and `delta`.
	 */
	void Particle::transition (void) {
		if (vehicle->get_delta () == 0) return;
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
