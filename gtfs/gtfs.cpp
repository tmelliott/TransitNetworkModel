#include <string>
#include <vector>

#include "gtfs.h"
#include "gps.h"

namespace gtfs {
	/**
	 * Get the coordinates of a point a given distance along a path.
	 * @param  distance total distance traveled along path, in meters
	 * @param  path     the path being traveled along
	 * @return          a coordinate object
	 */
	gps::Coord get_coords (double distance, std::vector<gps::Coord> path) {
		return gps::Coord (-33.2, 174.5);
	};
};
