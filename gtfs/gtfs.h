#ifndef GTFS_H
#define GTFS_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <inttypes.h>

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

	// Some parameters

	// class params {
	// 	double pi;
	// 	double gamma;
	// 	double tau;
	//
	// };


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
		std::string database_; /*!< the database file loaded into memory. */
		std::string version_;  /*!< when initialized, the version is set. */

		std::unordered_map<std::string, std::shared_ptr<Trip> > trips; /*!< A map of trip pointers */
		std::unordered_map<std::string, std::shared_ptr<Route> > routes; /*!< A map of route pointers */
		std::unordered_map<std::string, std::shared_ptr<Shape> > shapes; /*!< A map of shape pointers */
		std::unordered_map<unsigned long, std::shared_ptr<Segment> > segments; /*!< A map of segment pointers */
		std::unordered_map<std::string, std::shared_ptr<Stop> > stops;  /*!< A map of stop pointers */

	public:
		GTFS (std::string& dbname, std::string& v);

		// Get individual objects
		std::shared_ptr<Trip> get_trip (std::string& t) const;
		std::shared_ptr<Route> get_route (std::string& r) const;
		std::shared_ptr<Shape> get_shape (std::string& s) const;
		std::shared_ptr<Segment> get_segment (unsigned long s) const;
		std::shared_ptr<Stop> get_stop (std::string& s) const;

		// Get all objects ...
		/** @return an unordered map of Route objects */
		std::unordered_map<std::string, std::shared_ptr<Route> > get_routes (void) { return routes; };
		/** @return an unordered map of Trip objects */
		std::unordered_map<std::string, std::shared_ptr<Trip> > get_trips (void) { return trips; };
		/** @return an unordered map of Shape objects */
		std::unordered_map<std::string, std::shared_ptr<Shape> > get_shapes (void) { return shapes; };
		/** @return an unordered map of Segment objects */
		std::unordered_map<unsigned long, std::shared_ptr<Segment> > get_segments (void) { return segments; };
		/** @return an unordered map of Stop objects */
		std::unordered_map<std::string, std::shared_ptr<Stop> > get_stops (void) { return stops; };

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
		bool initialized;        /*!< logical; set to TRUE once particles have successfully been initialized */

	public:
		unsigned int n_particles; /*!< the number of particles that will be created in the next sample */
		unsigned long next_id;    /*!< the ID of the next particle to be created */

		// Constructors, destructors
		Vehicle (std::string id);
		Vehicle (std::string id, unsigned int n);
		~Vehicle();

		// Setters
		void set_trip (std::shared_ptr<Trip> tp);

		// Getters
		std::string get_id () const;
		std::vector<Particle>& get_particles ();
		const std::shared_ptr<Trip>& get_trip () const;
		/** Return the vehicle's position */
		const gps::Coord& get_position () const { return position; };

		/** @return the time the observation was last taken */
		const uint64_t& get_timestamp () const { return timestamp; };
		int get_delta () const;
		/** @return logical, if FALSE it means the vehicle needs to be initialized */
		bool is_initialized () const { return initialized; };


		// Methods
		void update ( sampling::RNG& rng );
		void update (const transit_realtime::VehiclePosition &vp, GTFS &gtfs);
		void update (const transit_realtime::TripUpdate &tu, GTFS &gtfs);
		unsigned long allocate_id ();
		void resample (sampling::RNG &rng);
		void reset (void);
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
		bool finished;            /*!< set to TRUE once particle reaches end of route */

		int stop_index;           /*!< stop index (1-based to match GTFS feed, {1, ..., M}) */
		uint64_t arrival_time;    /*!< arrival time at stop `stop_index` */
		int dwell_time;           /*!< dwell time at stop `stop_index`*/
		                          // departure_time = arrival_time + dwell_time

		int segment_index;        /*!< segment index (0-based, {0, ..., L-1}) */
		int queue_time;           /*!< cumulative time spent queuing at intersction `segment_index` */
		uint64_t begin_time;      /*!< time at which bus started along segment `segment_index` */

		int delta_t;              /*!< time this particular particle has left to travel */
		double log_likelihood;    /*!< the likelihood of the particle, given the data */

		std::vector<uint64_t> etas; /*!< ETA at all future stops. Previous stops are simply 0. */

	public:
		Vehicle* vehicle;  /*!< pointer to the vehicle that owns this particle */


		// Constructors, destructors
		Particle (Vehicle* v);
		Particle (const Particle &p);
		~Particle ();

		// Getters
		unsigned long get_id () const;
		unsigned long get_parent_id () const;

		/** @return the particle's distance into trip */
		const double& get_distance () const { return distance; };
		/** @return the particle's velocity */
		const double& get_velocity () const { return velocity; };
		/** @return true if the particle is at end of route; false otherwise */
		bool is_finished () const { return finished; };
		/** @return the particle's stop index */
		int get_stop_index () const { return stop_index; };
		/** @return the particle's arrival time at stop `stop_index` */
		const uint64_t& get_arrival_time () const { return arrival_time; };
		/** @return the particle's dwell time at stop `stop_index` */
		int get_dwell_time () const { return dwell_time; };
		/** @return the particle's segment index */
		int get_segment_index () const {return segment_index; };
		/** @return the particle's queue time at segment `segment_index` */
		int get_queue_time () const {return queue_time; };
		/** @return the particle's begin time at segment `segment_index` */
		const uint64_t& get_begin_time () const { return begin_time; };

		/** @return the particle's remaining travel time */
		int get_delta_t () const { return delta_t; };
		/** @return the particle's likelihood */
		const double& get_likelihood () const { return log_likelihood; };

		/** @return ETAs for all future stops */
		const std::vector<uint64_t>& get_etas (void) const { return etas; };
		/** @return ETA for stop `i` */
		const uint64_t& get_eta (int i) const { return etas[i]; };

		// Methods
		void initialize (sampling::uniform& unif, sampling::uniform& speed, sampling::RNG& rng);
		void transition (sampling::RNG& rng);
		void transition_phase1 (sampling::RNG& rng);
		void transition_phase2 (sampling::RNG& rng);
		void transition_phase3 (sampling::RNG& rng);
		void calculate_likelihood (void);

		void calculate_etas (void);
	};

	/**
	 * Print a particle object state.
	 * @param  os       the ostream to write to
	 * @param  p        the particle to print
	 * @return          an ostream instance
	 */
	inline std::ostream& operator<< (std::ostream& os, const Particle& p) {
		char buff [200];
		sprintf(buff, "[%*.0f, %*.1f, %*d, %*" PRIu64 ", %*d]",
				8, p.get_distance (),  4, p.get_velocity (),  2, p.get_stop_index (),
				13, p.get_arrival_time (), 4, p.get_dwell_time ());

		return os << buff;
	};

	gps::Coord get_coords (double distance, std::shared_ptr<Shape> shape);


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
		std::vector<RouteStop> stops;  /*!< vector of route stops + distance into shape */

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
		/** @return the route's ID */
		const std::string& get_id (void) const { return id; };
		std::vector<std::shared_ptr<Trip> > get_trips () const;
		/** @return the route's short name */
		const std::string& get_short_name (void) const { return route_short_name; };
		/** @return the route's long name */
		const std::string& get_long_name (void) const { return route_long_name; };
		/** @return the route's shape */
		std::shared_ptr<Shape> get_shape () const;
		/** @return the route's stops so that they're modifiable (incl. distance into trip) */
		std::vector<RouteStop>& get_stops () { return stops; };


		// --- SETTERS
		void add_trip (std::shared_ptr<Trip> trip);

		/**
		 * Add stops to a route.
		 * @param s a vector of RouteStop structs.
		 */
		void add_stops (std::vector<RouteStop>& s) { stops = s; };
	};

	/**
	 * A struct representing a route:stop combination.
	 */
	struct RouteStop {
		std::shared_ptr<Stop> stop;  /*!< a pointer to the relevant stop */
		double shape_dist_traveled;  /*!< how far along the route this stop is */

		/**
		 * Constructor for a RouteStop struct, without a distance (set to 0).
		 */
		RouteStop (std::shared_ptr<Stop> stop) : stop (stop), shape_dist_traveled (0) {};

		/**
		* Constructor for a RouteStop struct.
		*/
		RouteStop (std::shared_ptr<Stop> stop, double d) : stop (stop), shape_dist_traveled (d) {};
	};

	/**
	 * An object of this class represents a unique TRIP in the GTFS schedule.
	 *
	 * A trip is an instance of a route that occurs at a specific time of day.
	 * It has a sequence of stop times.
	 */
	class Trip : public std::enable_shared_from_this<Trip> {
	private:
		std::string id;                    /*!< the ID of the trip, as per GTFS */
		std::shared_ptr<Route> route;      /*!< a pointer back to the route */
		std::vector<StopTime> stoptimes;   /*!< a vector of stop times for the trip */

	public:
		// Construtors etc.
		Trip (std::string& id, std::shared_ptr<Route> route);

		// Trip (const Trip &t) {
		// 	std::cout << "Copying trip ...\n";
		// };

		// --- GETTERS
		/** @return the trip's ID */
		std::string get_id (void) const { return id; };
		/** @return a pointer to the trip's route */
		std::shared_ptr<Route> get_route (void) { return route; };
		/** @return vector of StopTime structs for the trip */
		const std::vector<StopTime>& get_stoptimes (void) const { return stoptimes; };

		// --- METHODS
		/**
		 * Add Stop Times to a trip
		 * @param st vector of StopTime structs
		 */
		void add_stoptimes (std::vector<StopTime>& st) {
			stoptimes = st;
		}
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
		/**
		 * Default constructor for a shape object.
		 * @param id the ID of the shape
		 */
		Shape (std::string& id) : id (id) {};

		// --- GETTERS
		/** @return the shape's ID */
		std::string get_id (void) const { return id; };

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
		/**
		 * Default constructor for a Shape Segment.
		 *
		 * This is simply a relationship between a GTFS SHAPE and it's segment information.
		 * That is, it's a 'pivot table'.
		 *
		 * @param segment  the segment ID
		 * @param distance how far into the overall shape this segment starts at
		 */
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
		std::shared_ptr<Intersection> start_at;      /*!< pointer to the first intersection */
		std::shared_ptr<Intersection> end_at;        /*!< pointer to the last intersection */
		std::vector<ShapePt> path;   /*!< vector of shape points */
		double length;               /*!< the length of this segment */

		double velocity;             /*!< the mean speed along the segment */
		double velocity_var;         /*!< the variance of speed along the segment */
		uint64_t timestamp;          /*!< updated at timestamp */

	public:
		/**
		 * The default constructor for a segment.
		 * @param id     the segment's ID
		 * @param start  the intersection at the beginning of the segment
		 * @param end    the intersection at the end of the segment
		 * @param path   a vector of shape points defining the shape of the segment
		 * @param length the length of the segment
		 */
		Segment (unsigned long id,
				 std::shared_ptr<Intersection> start,
				 std::shared_ptr<Intersection> end,
				 std::vector<ShapePt>& path,
				 double length) : id (id), start_at (start), end_at (end), path (path), length (length) {};
		// --- GETTERS
		/** @return the segment's ID */
		const unsigned long& get_id (void) const { return id; };
		/** @return a vector of shape points defining the segment's shape */
		const std::vector<ShapePt>& get_path (void) const { return path; };
		/** @return the length of the segment */
		double get_length (void) { return length; };

		// --- METHODS
		// void update (void);
	};

	/**
	 * A struct describing a single shape point.
	 */
	struct ShapePt {
		/**
		 * Default constructor for a shape point,
		 * which defines a GPS location and how far into the shape that point is.
		 *
		 * @param pt       GPS position of the point
		 * @param distance the distance into the segment of the point
		 */
		ShapePt (gps::Coord pt, double distance) : pt (pt), seg_dist_traveled (distance) {};
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
		Stop (std::string& id,
			  gps::Coord& pos);

		// --- GETTERS
		/** @return the stop's ID */
		const std::string& get_id (void) const { return id; };
		/** @return the stop's GPS position */
		const gps::Coord& get_pos (void) const { return pos; };

		// --- METHODS

	};


	/**
	 * A struct representing an instance of a trip arriving at a stop.
	 */
	struct StopTime {
		std::shared_ptr<Stop> stop;          /*!< pointer to the stop */
		boost::posix_time::time_duration arrival_time;   /*!< the scheduled arrival time */
		boost::posix_time::time_duration departure_time; /*!< the scheduled departure time */
		bool layover;        /*!< if true, the stop is a layover and we assume
	   				              the bus doesn't leave until the
								  scheduled departure time */

		/** Constructor for a StopTime struct */
		StopTime (std::shared_ptr<Stop> stop,
				  std::string& arrival,
				  std::string& departure) : stop (stop) {
			arrival_time = boost::posix_time::duration_from_string (arrival);
			departure_time = boost::posix_time::duration_from_string (departure);
		};
	};


}; // end GTFS namespace

#endif
