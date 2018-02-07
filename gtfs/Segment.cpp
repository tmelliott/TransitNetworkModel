#include <iostream>

#include <gtfs.h>
#include <sqlite3.h>

namespace gtfs {

	/**
	 * Add travel time observations to a segment
	 * @param mean     mean of particles' travel times
	 * @param variance variance of particles' travel times
	 */
	void Segment::add_data (int mean, double var) {
		data.emplace_back (mean, var);
	};

	/**
	 * Perform EKF prediction step (X_{c|c-1}, P_{c|c-1}) to use for all the things
	 * @param t the new time to predict to
	 */
	void Segment::predict (time_t t) {
		double prior_mean = travel_time;
		if (length > 0) prior_mean = length / 10.0; // = travel time @ 10m/s ~= 30km/h
		double prior_var = pow(prior_mean, 2);

		int delta = t - timestamp;

		double pred_tt    = travel_time,
		       pred_ttvar = travel_time_var,
		       psi        = 0.001 * pred_ttvar / (pred_ttvar + prior_var);
		for (int i=0; i<delta; i++) {
			pred_tt += psi * (prior_mean - pred_tt);
		}

		double Fn = pow(1 - psi, delta);
		double Q  = prior_var * (1 - pow(1 - psi, 2 * delta));

		pred_ttvar = pow(Fn, 2) * pred_ttvar + Q;
	}

	/**
	 * Perform Kalman filter UPDATE on the segment, using data
	 */
	void Segment::update () {
		// if (data.size () == 0) return;
		// double Vhat, Phat = 3000;
		// Vhat = std::accumulate (data.begin (), data.end (), 0.0);
		// // for (auto& d: data) {
		// // 	Vhat += d;
		// // 	// Phat += pow(std::get<0>(d), 2) + fmax(3, std::get<1>(d));
		// // 	// std::cout << std::get<0>(d) << " (" << std::get<1>(d) << "); ";
		// // }
		// Vhat = Vhat / data.size ();
		// // Phat = Phat / data.size () - pow(Vhat, 2);
		// std::cout << " => " << travel_time << " (" << travel_time_var << ")";

		// if (timestamp == 0) {
		// 	travel_time = Vhat;
		// 	travel_time_var = Phat * 2.0;
		// 	timestamp = t;
		// } else {
		// 	// 10 here is system noise; needs to be adjusted for length =/
		// 	travel_time_var += 0.1 * (t - timestamp);
		// 	double K = travel_time_var / (travel_time_var + Phat);
		// 	travel_time += K * (Vhat - travel_time);
		// 	travel_time_var = (1.0 - K) * travel_time_var;
		// 	timestamp = t;
		// }
		// data.clear ();
		// std::cout << " ==> " << travel_time << " (" << travel_time_var << ")";
		// if (length > 0 && travel_time > 0) {
		// 	double speed = (length / 1000) / (travel_time / 60 / 60);
		// 	std::cout << " - " << length << "m - approx " << speed << "km/h";

		// }
	};

}; // end namespace gtfs
