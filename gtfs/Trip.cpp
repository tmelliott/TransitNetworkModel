#include <iostream>

#include <gtfs.h>

namespace gtfs {
	/**
	 * Constructor for a route object, taking an ID, short and long name, and a shape pointer.
	 * @return [description]
	 */
	Trip::Trip (
		std::string& id
	) : id (id) {
		std::clog << " + Created trip " << id << "\n";
	};
	Trip::Trip (
		std::string& id,
		Route* route
	) : id (id), route (route) {
		std::clog << " + Created trip " << id << "\n";
		// Also going to point route to the trip
	};




}; // end namespace gtfs
