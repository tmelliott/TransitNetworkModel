#include <string>
#include <iostream>
#include <vector>

#include "gtfs.h"

namespace gtfs {
	/**
	* Particle constructor.
	*
	* The ID is automatically selected from the parent vehicle.
	*/
	Particle::Particle (Vehicle* v) : vehicle (v), parent_id (0) {
		id = v->allocate_id ();
		std::cout << " + Created particle for vehicle " << v->get_id ()
			<< " with id = " << id << std::endl;
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
		std::cout << " >+ Copying particle " << p.get_id () << " -> ";

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
		std::cout << " - Particle " << id << " deleted." << std::endl;
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

}; // end namespace gtfs
