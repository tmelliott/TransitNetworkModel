#include <iostream>

#include <gtfs.h>
#include <sqlite3.h>

namespace gtfs {

	/**
	 * The default constructor for a segment.
	 * @param id     the segment's ID
	 * @param start  the intersection at the beginning of the segment
	 * @param end    the intersection at the end of the segment
	 * @param path   a vector of shape points defining the shape of the segment
	 * @param length the length of the segment
	 */
	Segment::Segment (unsigned long id,
			 		  std::shared_ptr<Intersection> start,
					  std::shared_ptr<Intersection> end,
					  std::vector<ShapePt>& path,
					  double length)
		: id (id), start_at (start), end_at (end), path (path), length (length) {};

}; // end namespace gtfs
