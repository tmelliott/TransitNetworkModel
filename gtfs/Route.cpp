#include <iostream>

#include <gtfs.h>

namespace gtfs {
	/**
	 * Constructor for a route object, taking an ID, short and long name, without a shape pointer.
	 * @param id         the route's ID
	 * @param short_name the route's short name
	 * @param long_name  the route's long name
	 */
	Route::Route (
		std::string& id,
		std::string& short_name,
		std::string& long_name
	) : id (id), route_short_name (short_name), route_long_name (long_name) {
		std::clog << " + Created route " << id << "\n";
	};
	/**
	 * Constructor for a route object, with a shape pointer.
	 * @param id         the route's ID
	 * @param short_name the route's short name
	 * @param long_name  the route's long name
	 * @param shape      pointer to the route's shape object
	 */
	Route::Route (
		std::string& id,
		std::string& short_name,
		std::string& long_name,
		std::shared_ptr<Shape> shape
	) : id (id), route_short_name (short_name), route_long_name (long_name), shape (shape) {};


	// --- GETTERS

	/** @return a vector of trip pointers */
	std::vector<std::shared_ptr<Trip> > Route::get_trips () const {
		return trips;
	};

	/** @return a shape pointer */
	std::shared_ptr<Shape> Route::get_shape () const {
		return shape;
	};


	// --- SETTERS

	/**
	 * Add a trip to a route.
	 * @param trip a pointer to the new trip
	 */
	void Route::add_trip (std::shared_ptr<Trip> trip) {
		trips.push_back (trip);
	};

	
};
