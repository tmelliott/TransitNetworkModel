#ifndef GTFS_H
#define GTFS_H

#include <string>
#include <vector>
#include <memory>

#include "boost/date_time/posix_time/posix_time.hpp"

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
	class Route;
	struct RouteStop;
	class Trip;
	class Shape;
	class ShapeSegment;
	class Segment;
	struct ShapePt;
	class Intersection;
	class Stop;
	struct StopTime;

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

		// GTFS Realtime Fields
		Trip* trip;     /*!< the ID of the trip */
		unsigned int stop_sequence;       /*!< the stop number of the last visited stop */
		uint64_t arrival_time;   /*!< arrival time at last stop */
		uint64_t departure_time; /*!< departure time at last stop */
		gps::Coord position;     /*!< last reported GPS position */

		uint64_t timestamp;      /*!< time of last observation */
		int delta;               /*!< time since previous observation */

	public:
		unsigned int n_particles; /*!< the number of particles that will be created in the next sample */
		unsigned long next_id;    /*!< the ID of the next particle to be created */

		// Constructors, destructors
		Vehicle (std::string id, sampling::RNG &rng);
		Vehicle (std::string id, unsigned int n, sampling::RNG &rng);
		~Vehicle();

		// Setters
		void set_trip (std::string trip_id);

		// Getters
		std::string get_id () const;
		std::vector<Particle>& get_particles ();
		Trip* get_trip ();

		int get_delta () const;


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
		unsigned long id;         /*!< a unique particle identifier */
		unsigned long parent_id;  /*!< unique identifier of the particle that spawned this one */

		double distance;          /*!< distance into trip */
		double velocity;          /*!< velocity (m/s) */

		int stop_index;           /*!< stop index (1-based to match GTFS feed, {1, ..., M}) */
		uint64_t arrival_time;    /*!< arrival time at stop `stop_index` */
		int dwell_time;           /*!< dwell time at stop `stop_index`*/
		                          // departure_time = arrival_time + dwell_time

		int segment_index;        /*!< segment index (0-based, {0, ..., L-1}) */
		int queue_time;           /*!< cumulative time spent queuing at intersction `segment_index` */
		uint64_t begin_time;      /*!< time at which bus started along segment `segment_index` */

		double log_likelihood;    /*!< the likelihood of the particle, given the data */

	public:
		Vehicle* vehicle;  /*!< pointer to the vehicle that owns this particle */


		// Constructors, destructors
		Particle (Vehicle* v, sampling::RNG &rng);
		Particle (const Particle &p);
		~Particle ();

		// Getters
		unsigned long get_id () const;
		unsigned long get_parent_id () const;

		// Methods
		void transition (void);
		void calculate_likelihood (void);
	};

	gps::Coord get_coords (double distance, std::vector<gps::Coord> path);


	/**
	 * An object of this class represents a unique ROUTE in the GTFS schedule.
	 *
	 * A route is a journey from an origin to a destination by a certain
	 * sequence of stops. It has a shape and a sequence of stops.
	 */
	class Route {
	private:
		std::string id;                /*!< the ID of this route, as in the GTFS schedule */
		std::vector<Trip*> trips;      /*!< vector of pointers to trips that belong to this route */
		std::string route_short_name;  /*!< short name of the route, e.g., 090, NEX */
		std::string route_long_name;   /*!< long name of the route, e.g., Westgate to Britomart */
		Shape* shape;                  /*!< pointer to the route's shape */

	public:
		// --- Constructor, destructor
		Route (std::string& id,
			   std::string& short_name,
			   std::string& long_name);
		Route (std::string& id,
			   std::string& short_name,
			   std::string& long_name,
			   Shape* shape);

		// --- GETTERS
		std::string get_id (void) const { return id; };
		std::vector<Trip*> get_trips () const;
		Shape* get_shape () const;

		// --- SETTERS
		void add_trip (Trip* trip);
	};

	/**
	 * A struct representing a route:stop combination.
	 */
	struct RouteStop {
		Stop* stop;                  /*!< a pointer to the relevant stop */
		double shape_dist_traveled;  /*!< how far along the route this stop is */
	};

	/**
	 * An object of this class represents a unique TRIP in the GTFS schedule.
	 *
	 * A trip is an instance of a route that occurs at a specific time of day.
	 * It has a sequence of stop times.
	 */
	class Trip {
	private:
		std::string id;              /*!< the ID of the trip, as per GTFS */
		Route* route;                /*!< a pointer back to the route */

	public:
		// Construtors etc.
		Trip (std::string& id);      /*!< constructor for a trip without a route!? */
		Trip (std::string& id, Route* route);

		// --- GETTERS
		std::string get_id (void) const { return id; };
		Route* get_route (void) { return route; };

		// --- METHODS

	};

	/**
	 * An object of this class represents the path a vehicles takes
	 * from origin to destination.
	 *
	 * The path is made up of one or more segments, and each is
	 * associated with an initial distance traveled up until the
	 * beginning of that segment.
	 */
	class Shape {
	private:
		std::string id;
		std::vector<ShapeSegment> segments;

	public:
		// --- GETTERS

		// /** @return a vector of shape points for the entire shape. */
		// std::vector<gps::Coord> get_shape (void);

		// /** @return a vector of shape segments */
		// std::vector<ShapeSegment> get_segments (void);


		// --- METHODS
	};

	/**
	 * A struct corresponding legs of a shape with segments.
	 *
	 * The vector order == leg (0-based sequence).
	 */
	struct ShapeSegment {
		Segment* segment;            /*!< pointer to the segment */
		double shape_dist_traveled;  /*!< distance along route shape at beginning of this segment */
	};

	/**
	 * An object of this class represents a vehicles path
	 * between two intersections.
	 *
	 * The segment has a mean and variance of "speed" along it.
	 * At the beginning and end of routes, segments are defined
	 * from the first stop to the first intersection,
	 * and from the last intersection to the last stop, respectively.
	 * Each segment has a length, and consists of a sequence of
	 * gps::Coord's which are in turn associated with a cumulative distance.
	 */
	class Segment {
	private:
		unsigned long id;            /*!< ID of the segment - should be unique and autoincrement ... */
		Intersection* start_at;      /*!< pointer to the first intersection */
		Intersection* end_at;        /*!< pointer to the last intersection */
		std::vector<ShapePt> path;   /*!< vector of shape points */
		double length;               /*!< the length of this segment */

		double velocity;             /*!< the mean speed along the segment */
		double velocity_var;         /*!< the variance of speed along the segment */
		uint64_t timestamp;          /*!< updated at timestamp */

	public:
		// --- GETTERS


		// --- METHODS
		// void update (void);
	};

	/**
	 * A struct describing a single shape point.
	 */
	struct ShapePt {
		gps::Coord pt;             /*!< GPS coordinate of the point */
		double seg_dist_traveled;  /*!< cumulative distance traveled along the shape */
	};

	/**
	 * An object of this class represents an intersection.
	 *
	 * Intersections are (for now) either traffic lights or roundabouts,
	 * but in future will encompass all physical intersections.
	 * Each intersection will eventually get a mean and variance
	 * for delay at that intersection to represent the typical
	 * delay at that intersection.
	 */
	class Intersection {
	private:
		enum IntersectionType {
			traffic_light,
			roundabout,
			uncontrolled
		};

		unsigned long id;       /*!< ID of the intersection */
		gps::Coord pos;         /*!< gps location of the intersection */
		IntersectionType type;  /*!< the type of intersection */

		double delay;           /*!< the average delay at that intersection */
		double delay_var;       /*!< delay variability at that intersection */
		uint64_t timestamp;     /*!< updated at timestamp */

	public:
		// --- GETTERS


		// --- METHODS
		// void update (void);
	};

	/**
	 * An object of this class represents a physical stop.
	 *
	 * Stops have a position on a map, as well as dwell time parameters
	 * which can be implemented in the model at a later date
	 * to help differentiate busy and remote stops.
	 */
	class Stop {
	private:
		std::string id;    /*!< the ID of the stop, as per GTFS */
		gps::Coord pos;    /*!< GPS location of the stop */

		double dwell;      /*!< mean dwell time at this stop */
		double dwell_var;  /*!< variance of dwell time at this stop */
		uint64_t timestamp;     /*!< updated at timestamp */

	public:
		// --- GETTERS


		// --- METHODS
		// void update (void);
	};


	/**
	 * An object of the class represents an instance of a trip arriving at a stop.
	 *
	 * NOTE: this may be converted to a struct:
	 * 		 {string stop_id, uint64_t arrival_time, uint64_t departure_time, bool layover}.
	 */
	struct StopTime {
		Stop* stop;          /*!< pointer to the stop */
		boost::posix_time::time_duration arrival_time;   /*!< the scheduled arrival time */
		boost::posix_time::time_duration departure_time; /*!< the scheduled departure time */
		bool layover;        /*!< if true, the stop is a layover and we assume
	   				              the bus doesn't leave until the
								  scheduled departure time */
	};


}; // end GTFS namespace

#endif
