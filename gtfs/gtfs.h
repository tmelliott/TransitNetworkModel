#ifndef GTFS_H
#define GTFS_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

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
    class GTFS;

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
	 * GTFS Schedule Information class
	 *
	 * This class is used to provide convenient access to the static
	 * GTFS data stored in memory. It contains the individual objects
	 * such as trips, routes, etc. all initializaed as interrelated classes.
	 * Initialized once, an object of this class can be passed by reference
	 * to any functions/classes that require access to the static
	 * GTFS data.
	 *
	 * Those functions that need access are expected to know what it is
	 * they need. For example a vehicle needs it's trip,
	 * ```
	 * auto trp = gtfs.trip (TRIP_ID); // A pointer to a trip object
	 * trp->get_route ()->get_short_name ();
	 * ```
	 */
	class GTFS {
	private:
	    std::string version_;  /*!< when initialized, the version is set. */
		std::string database_; /*!< the database file loaded into memory. */

		std::unordered_map<std::string, std::shared_ptr<Trip> > trips; /*!< A map of trip pointers */
		std::unordered_map<std::string, std::shared_ptr<Route> > routes; /*!< A map of route objects */
		std::unordered_map<std::string, std::shared_ptr<Shape> > shapes; /*!< A map of shape objects */
		std::unordered_map<std::string, std::shared_ptr<Segment> > segments; /*!< A map of segment objects */

	public:
		GTFS (std::string& dbname, std::string& v);

		std::shared_ptr<Trip> get_trip (std::string& t) const; // this is expected to be the most common
		std::shared_ptr<Route> get_route (std::string& r) const;
		std::shared_ptr<Shape> get_shape (std::string& s) const;

		std::unordered_map<std::string, std::shared_ptr<Route> > get_routes (void) { return routes; };
		std::unordered_map<std::string, std::shared_ptr<Trip> > get_trips (void) { return trips; };
		std::unordered_map<std::string, std::shared_ptr<Shape> > get_shapes (void) { return shapes; };

	};

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
		std::shared_ptr<Trip> trip;     /*!< the ID of the trip */
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
		void set_trip (std::shared_ptr<Trip> tp);

		// Getters
		std::string get_id () const;
		std::vector<Particle>& get_particles ();
		const std::shared_ptr<Trip>& get_trip () const;

		int get_delta () const;


		// Methods
		void update ( void );
		void update (const transit_realtime::VehiclePosition &vp, GTFS &gtfs);
		void update (const transit_realtime::TripUpdate &tu, GTFS &gtfs);
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
		std::vector<std::shared_ptr<Trip> > trips; /*!< vector of pointers to trips that belong to this route */
		std::string route_short_name;  /*!< short name of the route, e.g., 090, NEX */
		std::string route_long_name;   /*!< long name of the route, e.g., Westgate to Britomart */
		std::shared_ptr<Shape> shape;  /*!< pointer to the route's shape */

	public:
		// --- Constructor, destructor
		Route (std::string& id,
			   std::string& short_name,
			   std::string& long_name);
		Route (std::string& id,
			   std::string& short_name,
			   std::string& long_name,
			   std::shared_ptr<Shape> shape);

		// --- GETTERS
		const std::string& get_id (void) const { return id; };
		std::vector<std::shared_ptr<Trip> > get_trips () const;
		const std::string& get_short_name (void) const { return route_short_name; };
		const std::string& get_long_name (void) const { return route_long_name; };
		std::shared_ptr<Shape> get_shape () const;

		// --- SETTERS
		void add_trip (std::shared_ptr<Trip> trip);
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
	class Trip : public std::enable_shared_from_this<Trip> {
	private:
		std::string id;              /*!< the ID of the trip, as per GTFS */
		std::shared_ptr<Route> route;                /*!< a pointer back to the route */

	public:
		// Construtors etc.
		Trip (std::string& id, std::shared_ptr<Route> route);

		Trip (const Trip &t) {
			std::cout << "Copying trip ...\n";
		};

		// --- GETTERS
		std::string get_id (void) const { return id; };
		std::shared_ptr<Route> get_route (void) { return route; };

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
		Shape (std::string& id) : id (id) {};

		// --- GETTERS
		std::string get_id (void) const { return id; };

		// /** @return a vector of shape points for the entire shape. */
		// std::vector<gps::Coord> get_shape (void);

		// /** @return a vector of shape segments */
		const std::vector<ShapeSegment>& get_segments (void) const { return segments; };

		// --- SETTERS
		void add_segment (std::shared_ptr<Segment> segment, double distance);


		// --- METHODS
	};

	/**
	 * A struct corresponding legs of a shape with segments.
	 *
	 * The vector order == leg (0-based sequence).
	 */
	struct ShapeSegment {
		ShapeSegment (std::shared_ptr<Segment> segment, double distance) : segment (segment), shape_dist_traveled (distance) {};
		std::shared_ptr<Segment> segment; /*!< pointer to the segment */
		double shape_dist_traveled;       /*!< distance along route shape at beginning of this segment */
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
