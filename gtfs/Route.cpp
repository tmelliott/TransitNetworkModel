#include <iostream>

#include <gtfs.h>

namespace gtfs {
	/**
	 * Constructor for a route object, taking an ID, short and long name, and a shape pointer.
	 * @return [description]
	 */
	Route::Route (
		std::string& id,
		std::string& short_name,
		std::string& long_name,
		Shape* shape
	) : id (id), route_short_name (short_name), route_long_name (long_name), shape (shape) {};


	// --- GETTERS

	/** @return a vector of trip pointers */
	std::vector<Trip*> Route::get_trips () const {
		return trips;
	};

	/** @return a shape pointer */
	Shape* Route::get_shape () const {
		return shape;
	};


	// --- SETTERS

	/**
	 * Add a trip to a route.
	 * @param trip a pointer to the new trip
	 */
	void Route::add_trip (Trip* trip) {
		trips.push_back (trip);
	};
};
