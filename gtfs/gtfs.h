#ifndef GTFS_H
#define GTFS_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <inttypes.h>

#include <boost/optional.hpp>
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

    struct DwellTime;
    struct vTravelTime;
    struct TravelTime;

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

		// Pre-loaded
		std::unordered_map<std::string, std::shared_ptr<Stop> >
		stops;          /*!< A map of stop pointers */
		std::unordered_map<unsigned long, std::shared_ptr<Intersection> >
		intersections;  /*!< A map of intersection pointers */
		std::unordered_map<unsigned long, std::shared_ptr<Segment> >
		segments;       /*!< A map of segment pointers */

		// Loaded as required
		std::unordered_map<std::string, std::shared_ptr<Trip> > trips; /*!< A map of trip pointers */
		std::unordered_map<std::string, std::shared_ptr<Route> > routes; /*!< A map of route pointers */
		std::unordered_map<std::string, std::shared_ptr<Shape> > shapes; /*!< A map of shape pointers */

	public:
		GTFS (std::string& dbname);
		GTFS (std::string& dbname, std::string& v);
		void initialize (void);

        std::string& get_dbname (void) { return database_; };

		// --- Get individual objects
		std::shared_ptr<Stop> get_stop (std::string& s) const;
		std::shared_ptr<Intersection> get_intersection (unsigned int i) const;
		std::shared_ptr<Segment> get_segment (unsigned long s) const;

		// --- Get objects, and load if necessary
		std::shared_ptr<Trip> get_trip (std::string& t);
		std::shared_ptr<Route> get_route (std::string& r);
		std::shared_ptr<Shape> get_shape (std::string& s);



		// --- Get all objects ...

		/** @return an unordered map of Stop objects */
		std::unordered_map<std::string, std::shared_ptr<Stop> >
		get_stops (void) { return stops; };

		/** @return an unordered map of Intersection objects */
		std::unordered_map<unsigned long, std::shared_ptr<Intersection> >
		get_intersections (void) { return intersections; };

		/** @return an unordered map of Segment objects */
		std::unordered_map<unsigned long, std::shared_ptr<Segment> >
		get_segments (void) { return segments; };

		/** @return an unordered map of Trip objects */
		std::unordered_map<std::string, std::shared_ptr<Trip> >
		get_trips (void) { return trips; };

		/** @return an unordered map of Route objects */
		std::unordered_map<std::string, std::shared_ptr<Route> >
		get_routes (void) { return routes; };

		/** @return an unordered map of Shape objects */
		std::unordered_map<std::string, std::shared_ptr<Shape> >
		get_shapes (void) { return shapes; };

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

		bool newtrip; /*!< if this is true, the next `update()` will reinitialise the particles AFTER finishing!!! */

		// GTFS Realtime Fields
		std::shared_ptr<Trip> trip;     /*!< the ID of the trip */
		boost::optional<unsigned> stop_sequence;     /*!< the stop number of the last visited stop */
		boost::optional<uint64_t> arrival_time;   /*!< arrival time at last stop */
		boost::optional<uint64_t> departure_time; /*!< departure time at last stop */
		boost::optional<int> delay; /*!< arrival/departure delay at most recent stop */
		gps::Coord position;     /*!< last reported GPS position */
		uint64_t timestamp;      /*!< time of last (position) observation */
		uint64_t first_obs;      /*!< the time of the first observation for that trip; used to pin down start time */
		double approx_distance; /*!< approximate distance to determine if traveling correct direction */

		int status = -1;    /*!< 0 = traveling normally; 1-3 = initializing stage; -1 = uninitialized; */
		bool updated;  /*!< if true, need to run update/mutate */

        // std::vector<DwellTime> dwell_times;  /*!< vehicle's dwell times at stops */
        // std::vector<vTravelTime> travel_times; /*!< vehicle's travel times through segments */
	public:
		unsigned int n_particles; /*!< the number of particles that will be created in the next sample */
		unsigned long next_id;    /*!< the ID of the next particle to be created */

		// Constructors, destructors
		Vehicle (std::string id);
		Vehicle (std::string id, unsigned int n);
		~Vehicle(void);

		// Setters
		void set_trip (std::shared_ptr<Trip> tp, uint64_t t);

		// Getters
		std::string get_id (void) const;
		std::vector<Particle>& get_particles (void);
		const std::shared_ptr<Trip>& get_trip (void) const;
		boost::optional<unsigned> get_stop_sequence (void) const;
		boost::optional<uint64_t> get_arrival_time (void) const;
		boost::optional<uint64_t> get_departure_time (void) const;
		boost::optional<int> get_delay (void) const;
		const gps::Coord& get_position (void) const;
		uint64_t get_timestamp (void) const;
		uint64_t get_first_obs (void) const;

		int get_status (void) const { return status; };


		// const std::vector<DwellTime>& get_dwell_times () const;
		// const DwellTime* get_dwell_time (unsigned i) const;
		// const std::vector<vTravelTime>& get_travel_times () const;
		// const vTravelTime* get_travel_time (unsigned i) const;


		// Methods
		void update ( sampling::RNG& rng );
		void update (const transit_realtime::VehiclePosition &vp, GTFS &gtfs);
		void update (const transit_realtime::TripUpdate &tu, GTFS &gtfs);
		unsigned long allocate_id (void);
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
		boost::optional<unsigned long> parent_id;  /*!< unique identifier of the particle that spawned this one */

		uint64_t start;                    /*!< start time; trajectory indices are seconds after this */
		int latest = 0;                    /*!< index of the latest position update; only adjust trajectory after this */
        std::vector<double> trajectory;    /*!< particle's distance trajectory, from 0 seconds into trip until end */
        std::vector<std::tuple<int,int>> stops;        /*!< [arrival,dwell] time at each stop along route */
        std::vector<std::tuple<int,int>> segments;     /*!< [queue,travel] time at each intersection/segment along route */

		double velocity = 0.0;       /*!< the particles velocity at latest time */
		double log_likelihood = 0.0; /*!< the likelihood of the particle, given the data */
		double weight;               /*!< the weight of this particle (reset to null after resample) */

	public:
		Vehicle* vehicle;  /*!< pointer to the vehicle that owns this particle */


		// Constructors, destructors
		Particle (Vehicle* v);
		Particle (const Particle &p);
		~Particle ();

		// Getters
		unsigned long get_id (void) const;
		bool has_parent (void) const;
		unsigned long get_parent_id (void) const;

		uint64_t get_start (void) const;
		int get_latest (void) const;
		std::vector<double> get_trajectory (void) const;
		// double get_distance (uint64_t& t) const;
		// double get_distance (int k) const;
		double get_distance (void) const;
		double get_velocity (void) const;
		std::vector<std::tuple<int,int>> get_stops (void) const;
		std::vector<std::tuple<int,int>> get_segments (void) const;

		/** @return the particle's likelihood */
		const double& get_likelihood () const { return log_likelihood; };
		double get_weight () { return weight; };

		// /** @return ETAs for all future stops */
		// const std::vector<uint64_t>& get_etas (void) const { return etas; };
		// /** @return ETA for stop `i` */
		// const uint64_t& get_eta (int i) const { return etas[i]; };

		// Methods
		// void initialize (sampling::RNG& rng);
		void initialize (double dist, sampling::RNG& rng);
		void mutate ( sampling::RNG& rng );
		void mutate ( sampling::RNG& rng, double );
		void calculate_likelihood (void);
		void set_weight (double wt) { weight = wt; };

		// void transition (sampling::RNG& rng, std::ofstream* f);
		// void transition_phase1 (sampling::RNG& rng, std::ofstream* f);
		// void transition_phase2 (sampling::RNG& rng);
		// void transition_phase3 (sampling::RNG& rng, std::ofstream* f);
		// void reset_travel_time (unsigned i);
		// void calculate_etas (void);

        // Operators
        bool operator<(const Particle &p2) const {
            return weight > p2.weight;
        }
	};

	/**
	 * Print a particle object state.
	 * @param  os       the ostream to write to
	 * @param  p        the particle to print
	 * @return          an ostream instance
	 */
	inline std::ostream& operator<< (std::ostream& os, const Particle& p) {
		char buff [200];
		// sprintf(buff, "[%*.0f, %*.1f | %*d, %*" PRIu64 ", %*d | %*d, %*d, %*" PRIu64 "]",
		// 		8, p.get_distance (),  4, p.get_velocity (),
		// 		2, p.get_stop_index (), 13, p.get_arrival_time (), 4, p.get_dwell_time (),
		// 		2, p.get_segment_index (), 4, p.get_queue_time (), 13, p.get_begin_time ());

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
		 * Add a shape to a route.
		 * @param sh pointer to a Shape object.
		 */
		void add_shape (std::shared_ptr<Shape> sh) { shape = sh; };

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
		std::vector<ShapePt> path;
		std::vector<ShapeSegment> segments;

	public:
		/**
		 * Default constructor for a shape object.
		 * @param id the ID of the shape
		 */
		Shape (std::string& id) : id (id) {};

		/**
		 * Constructor for a shape with a path
		 * @param id   the ID of the shape
		 * @param path the path, sequence of cooridinates, for the shape
		 */
		Shape (std::string& id, std::vector<ShapePt>& path) : id (id), path (path) {};

		/**
		 * Constructor for a shape with path and segments.
		 * @param id       the ID of the shape
		 * @param path     the path, sequence of cooridinates, for the shape
		 * @param segments vector of segments making up the shape
		 */
		Shape (std::string& id, std::vector<ShapePt>& path, std::vector<ShapeSegment> segments) :
			id (id), path (path), segments (segments) {};

		// --- GETTERS
		/** @return the shape's ID */
		std::string get_id (void) const { return id; };

		/** @return a vector of points making up the shape's path */
		const std::vector<ShapePt>& get_path (void) const { return path; };

		/** @return a vector of shape segments */
		const std::vector<ShapeSegment>& get_segments (void) const { return segments; };

		// --- SETTERS
		void set_path (std::vector<ShapePt>& path);
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
		std::shared_ptr<Intersection> from_id;      /*!< pointer to the first intersection */
		std::shared_ptr<Intersection> to_id;        /*!< pointer to the last intersection */
		std::shared_ptr<Stop> start_at;     /*!< pointer to the first stop */
		std::shared_ptr<Stop> end_at;       /*!< pointer to the last stop */
		double length;               /*!< the length of this segment */
		int type;                    /*!< the type of segment, 1=int->int, 2=stop->int, 3=int->stop, 4=stop->stop */

		double travel_time = 0;             /*!< the mean speed along the segment */
		double travel_time_var = 0;         /*!< the variance of speed along the segment */
		uint64_t timestamp = 0;             /*!< updated at timestamp */

		std::vector<std::tuple<double,double> > data; /*!< estimates of travel time for recent vehicles */

	public:
		/**
		 * Constructor for a segment between two intersections
		 * @param id     The segment's ID
		 * @param from   pointer to the starting stop/intersection
		 * @param to     pointer to the ending stop/intersection
		 * @param length length of the segment, in meters
		 */
		Segment (unsigned long id,
				 std::shared_ptr<Intersection> from,
				 std::shared_ptr<Intersection> to,
				 double len) : id (id), from_id (from), to_id (to), length (len), type (1) {};
		/**
		 * Constructor for a segment from a stop to an intersection
		 * @param id     The segment's ID
		 * @param start  pointer to the starting stop
		 * @param end    pointer to the ending intersection
		 * @param length length of the segment, in meters
		 */
		Segment (unsigned long id,
				 std::shared_ptr<Stop> start,
				 std::shared_ptr<Intersection> to,
				 double len) : id (id), to_id (to), start_at (start), length (len), type (2) {};
		/**
		 * Constructor for a segment from an intersection to a stop
		 * @param id     The segment's ID
		 * @param start  pointer to the starting intersection
		 * @param end    pointer to the ending stop
		 * @param length length of the segment, in meters
		 */
		Segment (unsigned long id,
				 std::shared_ptr<Intersection> from,
				 std::shared_ptr<Stop> end,
				 double len) : id (id), from_id (from), end_at (end), length (len), type (3) {};
		/**
		 * Constructor for a segment between two stops (i.e., a route with no intersections ...)
		 * @param id     The segment's ID
		 * @param start  pointer to the starting stop
		 * @param end    pointer to the ending stop
		 * @param length length of the segment, in meters
		 */
		Segment (unsigned long id,
				 std::shared_ptr<Stop> start,
				 std::shared_ptr<Stop> end,
				 double len) : id (id), start_at (start), end_at (end), length (len), type (4) {};


		// --- GETTERS
		/** @return the segment's ID */
		const unsigned long& get_id (void) const { return id; };
		/** @return the length of the segment */
		double get_length (void) { return length; };

		/** @return the starting intersection */
		std::shared_ptr<Intersection> get_from (void) { return from_id; };
		/** @return the ending intersection */
		std::shared_ptr<Intersection> get_to (void) { return from_id; };
		/** @return the starting stop */
		std::shared_ptr<Intersection> get_start (void) { return from_id; };
		/** @return the ending stop */
		std::shared_ptr<Intersection> get_end (void) { return from_id; };
		/** @return the segment type (integer) */
		int get_type (void) const { return type; };
		/** @return logical, if the segment starts at an intersection */
		bool fromInt (void) { return type == 1 || type == 3; };
		/** @return logical, if the segment ends at an intersection */
		bool toInt (void) { return type == 1 || type == 2; };

		bool has_data (void) { return data.size () > 0; };
		bool is_initialized (void) { return timestamp > 0; };
        double get_travel_time (void) { return travel_time; };
        double get_travel_time_var (void) { return travel_time_var; };
        const uint64_t& get_timestamp (void) const { return timestamp; };

		// --- METHODS
		void add_data (double mean, double variance);
		void update (time_t t);
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
		 * @param distance the point's distance along the path
		 */
		ShapePt (gps::Coord pt, double distance) : pt (pt), dist_traveled (distance) {};
		/**
		 * Constructor without distance.
		 * @param pt GPS position of point.
		 */
		ShapePt (gps::Coord pt) : pt (pt) {};

		gps::Coord pt;             /*!< GPS coordinate of the point */
		double dist_traveled;      /*!< cumulative distance traveled along the shape */
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
		unsigned long id;       /*!< ID of the intersection */
		gps::Coord pos;         /*!< gps location of the intersection */
		std::string type;       /*!< the type of intersection */

		double delay;           /*!< the average delay at that intersection */
		double delay_var;       /*!< delay variability at that intersection */
		uint64_t timestamp;     /*!< updated at timestamp */

	public:
		Intersection (unsigned long id,
					  gps::Coord pos,
				  	  std::string& type);

		// --- GETTERS
		unsigned long get_id (void) const { return id; };
		const gps::Coord& get_pos (void) const { return pos; };
		const std::string& get_type (void) const { return type; };

		// --- METHODS

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


    /**
     * Dwell times for stops along the route
     */
    struct DwellTime {
        std::shared_ptr<Stop> stop; /*!< pointer to the stop */
        int time = 0;               /*!< time, in seconds, spent at stop */
		bool used = false;          /*!< true once model has used the value */

		int nparticle = 0;          /*!< the number of particles that have contributed */
		int nstop = 0;              /*!< the number of particles that stopped */
		float dwellsum = 0;         /*!< the sum of dwell times of particles */

		DwellTime () {};
		DwellTime (std::shared_ptr<Stop> s) : stop (s) {};

		void set_time (void) { time = dwellsum / nstop; };
		int get_time (void) {
			set_time ();
			used = true;
			return time;
		};
    };

	struct vTravelTime {
		std::shared_ptr<Segment> segment; /*!< pointer to the segment */
		int time = 0;                     /*!< the time spent traveling */
		bool used = false;                /*!< true once model has used the value */

		int nparticle = 0;                /*!< the number of particles that have contributed */
		float timesum = 0;                /*!< the sum of particle travel times */

		vTravelTime () {};
		vTravelTime (std::shared_ptr<Segment> seg) : segment (seg) {};

		void set_time (void) { time = timesum / nparticle; };
		int get_time (void) {
			set_time ();
			used = true;
			return time;
		};
	};

    /**
     * A recent travel time associated with a particle.
     *
     * Only consider travel times that have been initialized AND completed.
     */
    struct TravelTime {
        std::shared_ptr<Segment> segment; /*!< pointer to the segment */
        int time;                         /*!< time, in seconds, taken to travel segment */
		bool complete;                    /*!< true once particle has finished with the segment */
		bool initialized;                 /*!< true once particle starts AT THE BEGINNING of segment */

		TravelTime () : time (0), complete (false) , initialized (false) {};
		TravelTime (std::shared_ptr<Segment> seg) :
			segment (seg), time (0), complete (false), initialized (false) {};
    };


}; // end GTFS namespace

#endif
