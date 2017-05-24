#include <iostream>

#include <gtfs.h>

namespace gtfs {
	/**
	 * Constructor for a trip object, taking an ID and a route pointer.
	 * @return [description]
	 */
	Trip::Trip (
		std::string& id,
		Route* route
	) : id (id), route (route) {
		std::clog << " + Created trip " << id << "\n";
		// Also going to point route to the trip
		route->add_trip (this);
	};


}; // end namespace gtfs
