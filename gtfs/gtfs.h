#ifndef GTFS_H
#define GTFS_H

#include <string>
#include <vector>

#include "gps.h"
#include "gtfs-realtime.pb.h"
#include "sampling.h"

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
	 *
	 * Vehicles are initialized with an ID, so it is not editable.
	 */
	class Vehicle {
	private:
		std::string id; /*!< ID of vehicle, as per GTFS feed */
		std::vector<Particle> particles; /*!< the particles associated with the vehicle */

		bool newtrip; /*!< if this is true, the next `update()` will reinitialise the particles */

	public:
		unsigned int n_particles; /*!< the number of particles that will be created in the next sample */
		unsigned long next_id; /*!< the ID of the next particle to be created */

		// GTFS Realtime Fields
		std::string trip_id;


		// Constructors, destructors
		Vehicle (std::string id);
		Vehicle (std::string id, unsigned int n);
		~Vehicle();

		// Getters
		std::string get_id () const;
		std::vector<Particle>& get_particles ();

		// Methods
		void update ( void );
		void update (const transit_realtime::VehiclePosition &vp);
		void update (const transit_realtime::TripUpdate &tu);
		unsigned long allocate_id ();
		void resample (sampling::RNG &rng);
	};


	/**
	 * Particle class
	 *
	 * A single "point estimate" of the unknown state of the transit vehicle,
	 * including its velocity.
	 */
	class Particle {
	private:
		unsigned long id;  /*!< a unique particle identifier */
		unsigned long parent_id;  /*!< unique identifier of the particle that spawned this one */

	public:
		Vehicle* vehicle;  /*!< pointer to the vehicle that owns this particle */

		// Constructors, destructors
		Particle (Vehicle* v);
		Particle (const Particle &p);
		~Particle ();  // destroy

		// Getters
		unsigned long get_id () const;
		// Vehicle* get_vehicle ();
		unsigned long get_parent_id () const;


		// Methods

	};

	gps::Coord get_coords (double distance, std::vector<gps::Coord> path);

}; // end GTFS namespace

#endif
