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
		if (var == 0) var = 100.0;
		data.emplace_back (mean, var);
	};

	/**
	 * Perform EKF prediction step (X_{c|c-1}, P_{c|c-1}) to use for all the things
	 * @param t the new time to predict to
	 */
	void Segment::predict (time_t t) {
		std::clog << "\n + Segment " << id << ": "
			<< "[" << round(travel_time * 100.0) / 100.0 << ", " << round(travel_time_var * 100.0) / 100.0 << "]";

		double prior_mean = travel_time;
		if (length > 0) {
			prior_mean = (double)length / 10.0; // = travel time @ 10m/s ~= 30km/h
		}
		if (timestamp == 0) {
			// We're just initiating this thing.
			travel_time = (prior_mean == 0) ? 100 : prior_mean;
			travel_time_var = pow(travel_time, 2);
			timestamp = t;
			std::clog << " -> initiatialised " 
				<< "[" << round(travel_time * 100.0) / 100.0 << ", " << round(travel_time_var * 100.0) / 100.0 << "]";
			return;
		}
		double prior_var = pow(prior_mean, 2);

		int delta = t - timestamp;
		double psi = 0.001 * travel_time_var / (travel_time_var + prior_var);
		double Fn = pow(1 - psi, delta);
		double Q  = prior_var * (1 - pow(1 - psi, 2 * delta));


		// The F(beta) function ...
		for (int i=0; i<delta; i++) {
			travel_time += psi * (prior_mean - travel_time);
		}
		travel_time_var = pow(Fn, 2) * travel_time_var + Q;

		timestamp = t;
		std::clog << " -> " 
			<< "[" << round(travel_time) << ", " << round(travel_time_var * 100) / 100.0 << "]";
	}

	/**
	 * Perform Kalman filter UPDATE on the segment, using data
	 */
	void Segment::update () {

		if (data.size () == 0) return;
		std::clog << "\n + Segment " << id << ":\n   - Data: ";

		double Bhat = 0.0, Ehat = 0.0;
		for (auto& d: data) {
			std::clog << std::get<0>(d) << " (" << std::get<1>(d) << "),";
			Bhat += std::get<0>(d);
			Ehat += pow(std::get<0>(d), 2) + std::get<1>(d);
		}
		std::clog << " => " << Bhat << ", " << Ehat;
		if (data.size () > 1) {
			Bhat /= (double)data.size ();
			Ehat /= (double)data.size ();
		}
		Ehat -= pow(Bhat, 2);
		std::clog << " => " << Bhat << " (" << Ehat << ")";
		data.clear (); // finished with the data.

		std::clog << "\n   - State: " << travel_time << " (" << travel_time_var << ")";

		double K = travel_time_var / (travel_time_var + Ehat);
		travel_time += K * (Bhat - travel_time);
		travel_time_var *= (1 - K);

		std::clog << " => " << travel_time << " (" << travel_time_var << ")";
		if (length > 0 && travel_time > 0) {
			double speed = (length / 1000) / (travel_time / 60 / 60);
			std::clog << " - " << length << "m - approx " << speed << "km/h";
		}
	};

}; // end namespace gtfs
