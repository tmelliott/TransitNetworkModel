#include <iostream>

#include <gtfs.h>

namespace gtfs {
	// --- GETTERS

	// --- SETTERS

	/**
	 * Add a segment to a shape.
	 * @param segment  Pointer to the segment object.
	 * @param distance How far into the overall shape this segment begins at.
	 */
	void Shape::add_segment (std::shared_ptr<Segment> segment, double distance) {
		segments.emplace_back (segment, distance);
	};


}; // end namespace gtfs
