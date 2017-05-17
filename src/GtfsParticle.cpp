#include <string>
#include <iostream>
#include <memory>
#include <vector>

#include "gtfs.h"

/**
* Particle constructor.
*
* The ID is automatically selected from the parent vehicle.
*/
gtfs::Particle::Particle (gtfs::Vehicle* v) : vehicle_ (v), parent_id_ (0) {
	unsigned long next = v->next_id_++;
	particle_id_ = next;
	std::cout << " + Created particle for vehicle " << v->vehicle_id ()
		<< " with id = " << particle_id_ << std::endl;
};

/**
 * Particle copy constructor.
 *
 * Makes an almost exact copy of the referenced particle,
 * but gives it a unique ID and sets the parent_id appropriately.
 *
 * @param p the parent particle to be copied
 */
gtfs::Particle::Particle (const gtfs::Particle &p) {
	std::cout << " >+ Copying particle " << p.particle_id () << " -> ";

	// Copy vehicle pointer
	vehicle_ = p.vehicle_;
	parent_id_ = p.particle_id_;

	// Increment particle id
	particle_id_ = p.vehicle_->next_id_++;
	std::cout << particle_id_ << std::endl;
};

/**
* Destructor for a particle.
*/
gtfs::Particle::~Particle() {
	std::cout << " - Particle " << particle_id_ << " deleted." << std::endl;
};


// --- GETTERS

/**
* @return the particle's ID
*/
const unsigned long gtfs::Particle::particle_id () const {
	return particle_id_;
};

/**
 * @return a pointer to the particle's vehicle
 */
gtfs::Vehicle* gtfs::Particle::vehicle () {
	return vehicle_;
}

/**
 * @return the parent particle's ID
 */
const unsigned long gtfs::Particle::parent_id () const {
	return parent_id_;
}
