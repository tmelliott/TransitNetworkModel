#include <iostream>

#include <gtfs.h>
#include <sqlite3.h>

namespace gtfs {

	/**
	 * Add travel time observations to a segment
	 * @param mean     mean of particles' travel times
	 * @param variance variance of particles' travel times
	 */
	void Segment::add_data (int mean) {
		data.push_back (mean);
	};

	/**
	 * Perform Kalman filter update on the segment, using data
	 */
	void Segment::update (time_t t) {
		if (data.size () == 0) return;
		double Vhat, Phat = 5.0;
		Vhat = std::accumulate (data.begin (), data.end (), 0.0);
		// for (auto& d: data) {
		// 	Vhat += d;
		// 	// Phat += pow(std::get<0>(d), 2) + fmax(3, std::get<1>(d));
		// 	// std::cout << std::get<0>(d) << " (" << std::get<1>(d) << "); ";
		// }
		Vhat = Vhat / data.size ();
		// Phat = Phat / data.size () - pow(Vhat, 2);
		std::cout << " => " << travel_time << " (" << travel_time_var << ")";

		if (timestamp == 0) {
			travel_time = Vhat;
			travel_time_var = Phat * 2.0;
			timestamp = t;
		} else {
			// 10 here is system noise; needs to be adjusted for length =/
			travel_time_var += 0.1 * (t - timestamp);
			double K = travel_time_var / (travel_time_var + Phat);
			travel_time += K * (Vhat - travel_time);
			travel_time_var = (1.0 - K) * travel_time_var;
			timestamp = t;
		}
		data.clear ();
		std::cout << " ==> " << travel_time << " (" << travel_time_var << ")";
		if (length > 0 && travel_time > 0) {
			double speed = (length / 1000) / (travel_time / 60 / 60);
			std::cout << " - approx " << speed << "km/h";

		}
	};

}; // end namespace gtfs
