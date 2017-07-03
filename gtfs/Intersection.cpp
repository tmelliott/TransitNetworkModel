#include <iostream>

#include <gtfs.h>
#include <sqlite3.h>

namespace gtfs {

	Intersection::Intersection (unsigned long id,
				  				gps::Coord pos,
				  				std::string& type) :
		id (id),
		pos (pos),
		type (type) {};


}; // end namespace gtfs
