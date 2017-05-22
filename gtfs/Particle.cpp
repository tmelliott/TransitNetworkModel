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
	Particle::Particle (Vehicle* v, sampling::RNG &rng) : vehicle (v), parent_id (0) {
		id = v->allocate_id ();
		std::clog << " + Created particle for vehicle " << v->get_id ()
			<< " with id = " << id;

		// Set up some random number generating devices
		sampling::uniform unif {0, 30};
		// initialize with defaults etc.
		distance = 0;
		velocity = unif.rand (rng);

		std::clog << " (" << distance << ", " << velocity << ")\n";
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

		// Copy vehicle pointer
		vehicle = p.vehicle;
		parent_id = p.id;

		// Increment particle id
		id = p.vehicle->allocate_id ();
		std::cout << id << std::endl;
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

	};

}; // end namespace gtfs
