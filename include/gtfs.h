#ifndef GTFS_H
#define GTFS_H

#include <string>
#include <iostream>
#include <memory>
#include <vector>

/**
 * GTFS Namespace
 *
 * All aspects of the program refering to the GTFS information are in this namespace.
 *
 */
namespace gtfs {
	class Vehicle;
	class Particle;


	/**
	 * Transit vehicle class
	 *
	 * A representation of a physical transit vehicle (i.e., a bus),
	 * including the most recent data associated with that vehicle
	 * (GPS location, arrival/departure time, etc).
	 */
	class Vehicle {
	private:
		std::string vehicle_id_;
		unsigned int n_particles_;
		std::vector<Particle> particles_;
		unsigned long next_id_;

		friend class Particle; // Particle has access to vehicle.

	public:
		// Constructors, destructors
		Vehicle (std::string vehicle_id);
		~Vehicle();

		// Getters
		const std::string& vehicle_id () const;
		const unsigned int& n_particles () const;
		std::vector<Particle>& particles ();
		unsigned long& next_id ();

		// Setters
		void n_particles (unsigned int n);

		// Methods
		void resample ();
	};


	/**
	 * Particle class
	 *
	 * A single "point estimate" of the unknown state of the transit vehicle,
	 * including its velocity.
	 */
	class Particle {
	private:
		unsigned long particle_id_;
		Vehicle* vehicle_;
		unsigned long parent_id_;

	public:
		// Constructors, destructors
		Particle (Vehicle* v);
		Particle (const Particle &p);
		~Particle ();  // destroy

		// Getters
		const unsigned long particle_id () const;
		Vehicle* vehicle ();
		const unsigned long parent_id () const;

		// Setters


		// Methods

	};

}; // end GTFS namespace

#endif
