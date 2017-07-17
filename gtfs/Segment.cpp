#include <iostream>

#include <gtfs.h>
#include <sqlite3.h>

namespace gtfs {

	/**
	 * Add travel time observations to a segment
	 * @param mean     mean of particles' travel times
	 * @param variance variance of particles' travel times
	 */
	void Segment::add_data (double mean, double variance) {
		data.emplace_back (mean, variance);
	};

	/**
	 * Perform Kalman filter update on the segment, using data
	 */
	void Segment::update (void) {
		if (data.size () == 0) return;
		for (auto& d: data) {
			std::cout << std::get<0>(d) << " (" << std::get<1>(d) << "); ";
		}

		data.clear ();
	};

}; // end namespace gtfs
