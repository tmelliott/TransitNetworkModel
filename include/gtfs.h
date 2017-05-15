#ifndef GTFS_H
#define GTFS_H

#include <string>
#include <iostream>
#include <memory>

/**
 * GTFS Namespace
 *
 * All aspects of the program refering to the GTFS information are in this namespace.
 *
 */
namespace gtfs {
	class Vehicle;
	class Particle;
};


/**
 * Transit vehicle class
 *
 * A representation of a physical transit vehicle (i.e., a bus),
 * including the most recent data associated with that vehicle
 * (GPS location, arrival/departure time, etc).
 */
class gtfs::Vehicle {
private:
	std::string vehicle_id_;
	std::vector<std::shared_ptr<gtfs::Particle> > particles_;

public:
	// Constructors, destructors
	Vehicle (std::string vehicle_id);
	~Vehicle();

	// Getters
	const std::string vehicle_id () const;
	std::vector<std::shared_ptr<gtfs::Particle> >& particles ();

	// Setters


	// Methods
	void resample ();
};


/**
 * Particle class
 *
 * A single "point estimate" of the unknown state of the transit vehicle,
 * including its velocity.
 */
class gtfs::Particle {
private:
	unsigned long particle_id_;
	// gtfs::Vehicle& vehicle_;

public:
	// Constructors, destructors
	Particle (int i);
	Particle (const gtfs::Particle &p); // copy
	Particle (gtfs::Particle&& p);  // move
	~Particle ();  // destroy

	// Getters
	const unsigned long particle_id () const;

	// Setters


	// Methods
	//
};


#include "gtfs-vehicle.h"
#include "gtfs-particle.h"



#endif
