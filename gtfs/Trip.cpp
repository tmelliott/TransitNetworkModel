#include <iostream>

#include <gtfs.h>

namespace gtfs {
	/**
	 * Constructor for a trip object, taking an ID and a route pointer.
	 * @return [description]
	 */
	Trip::Trip (
		std::string& id,
		std::shared_ptr<Route> route
	) : id (id), route (route) {
		std::clog << " + Created trip " << id << "\n";
		// Also going to point route to the trip
		// route->add_trip (shared_from_this ());
	};


}; // end namespace gtfs
