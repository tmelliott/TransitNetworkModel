#include <iostream>

#include <gtfs.h>

namespace gtfs {
	void Shape::add_segment (std::shared_ptr<Segment> segment, double distance) {
		segments.emplace_back (segment, distance);
	};


}; // end namespace gtfs
