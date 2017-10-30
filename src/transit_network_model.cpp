/**
 * \mainpage
 * A realtime model of a public transport network.
 *
 * An program which runs indefinitely, modeling the realtime state of
 * all vehicles in the transit network. These are in turn used to model
 * the realtime state of the network itself (road speeds),
 * and finally arrival time predictions made for each vehicle/stop combination.
 *
 * - transit_network_model.cpp
 * - load_gtfs.cpp
 *
 * @file
 * @author Tom Elliott <tom.elliott@auckland.ac.nz>
 * @version 0.0.1
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <memory>
#include <vector>
#include <numeric>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <time.h>
#include <sqlite3.h>

#include <stdio.h>
#include <chrono>
#include <ctime>
#include <thread>

#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include "gtfs-realtime.pb.h"
#include "gtfs-network.pb.h"
#include "gtfs-eta.pb.h"
#include "sampling.h"
#include "gtfs.h"
#include "gps.h"

namespace po = boost::program_options;

bool load_feed (std::unordered_map<std::string, std::unique_ptr<gtfs::Vehicle> > &vs,
				std::string &feed_file, int N, sampling::RNG &rng,
				gtfs::GTFS &gtfs, time_t *filetime);
// bool write_etas (std::unique_ptr<gtfs::Vehicle>& v, std::string &eta_file);
void time_start (std::clock_t& clock, std::chrono::high_resolution_clock::time_point& wall);
void time_end (std::clock_t& clock, std::chrono::high_resolution_clock::time_point& wall);


/**
 * Transit Network Model: a realtime model running indefinitely (while (true) { ... })
 *
 * Cycles through latest vehicles in the Realtime Feed, and updates/creates accordingly.
 *
 * @param  argc number of command line arguments
 * @param  argv argument vector
 * @return int 0 (although this will never happen because of the while forever loop)
 */
int main (int argc, char* argv[]) {

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// PROGRAM COMMAND LINE ARGUMENTS
	po::options_description desc ("Allowed options");

	/** vehicle positions file */
	std::vector<std::string> files;
	/** database connection to use */
	std::string dbname;
	// std::string version;
	/** number of particles per vehicle */
	int N;
	/** number of cores to use */
	int numcore;

	int csvout;

	desc.add_options ()
		("files", po::value<std::vector<std::string> >(&files)->multitoken (),
			"GTFS Realtime protobuf feed files.")
		("database", po::value<std::string>(&dbname)->default_value("../gtfs.db"), "Database Connection to use.")
		// ("version", po::value<std::string>(&version), "Version number to pull subset from database.")
		("N", po::value<int>(&N)->default_value(1000), "Number of particles to initialize each vehicle.")
		("numcore", po::value<int>(&numcore)->default_value(1), "Number of cores to use.")
		("csv", po::value<int>(&csvout)->default_value(0), "Setting to 1 will cause all particles and their ETAs to be written to PARTICLES.csv and ETAs.csv, respectively; 2 will do the same but append to the file. WARNING: slow!")
		("help", "Print this message and exit.")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count ("help")) {
		std::cout << desc << "\n";
		return 1;
	}

	if (!vm.count ("files")) {
		std::cerr << "No file specified.\nUse --files to specify protobuf feed files.\n";
		return -1;
	}

	if (!vm.count ("database")) {
	    std::cerr << "No database specified. Use --database to select a SQLIte database.\n";
		return -1;
	}
	// if (!vm.count ("version")) {
	// 	std::cout << "WARNING: version number not specified; entire database will be loaded!\n";
	// 	version = "";
	// }

	std::clock_t clockstart;
	std::chrono::high_resolution_clock::time_point wallstart;

	// Load the global GTFS database object:
	time_start (clockstart, wallstart);
	gtfs::GTFS gtfs (dbname);
	std::cout << " * Database loaded into memory\n";
	time_end (clockstart, wallstart);

	// An ordered map of vehicles that can be accessed using ["vehicle_id"]
	std::unordered_map<std::string, std::unique_ptr<gtfs::Vehicle> > vehicles;
	sampling::RNG rng;
	bool forever = true;

	// std::ofstream f; // file for particles
	// f.open ("particles.csv");
	// f << "vehicle_id,trip_id,timestamp,particle_id,t,d\n";
	// f.close ();

	time_t curtime;
	int repi = 20;
	while (forever && repi > 0) {
		// repi--;
		curtime = time (NULL);

		// Load GTFS feed -> vehicles
		{
			time_start (clockstart, wallstart);
			for (auto file: files) {
				try {
					if ( ! load_feed (vehicles, file, N, rng, gtfs, &curtime) ) {
						std::cerr << "Unable to read file.\n";
						continue;
					}

					if (forever) std::remove (file.c_str ());
				} catch (...) {
					std::cerr << "Error occured loading file.\n";
				}
			}
			std::cout << "\n";
			time_end (clockstart, wallstart);
		}

		std::cout.flush ();

		// Update each vehicle's particles
		{
			// -> triggers particle transition -> resample
			time_start (clockstart, wallstart);
			std::cout << "\n * Running particle filter ";
			struct tm * timeinfo;
			timeinfo = std::localtime (&curtime);
			char buff[20];
			strftime (buff, sizeof (buff), "%H:%M:%S", timeinfo);
			printf("at %s ...", buff);
			std::cout << "\n";
			std::cout.flush ();
			#pragma omp parallel for schedule(dynamic, 20) num_threads(numcore)
			for (unsigned i=0; i<vehicles.bucket_count (); i++) {
				for (auto v = vehicles.begin (i); v != vehicles.end (i); v++) {
					if (v->second->is_finished ()) continue;
					// std::cout << "\n - vehicle " << v->second->get_id ();
					if (v->second->get_trip () &&
						v->second->get_trip ()->get_route ()) {
						std::cout << "\n\n +------------------------------- Route ---+ "
							<< v->second->get_trip ()->get_route ()->get_short_name ()
							<< " [" << v->second->get_id () << "]";
						std::cout.flush ();
						try {
							v->second->update (rng);
						} catch (const std::bad_alloc& e) {
							std::clog << "\n *** ERROR: " << e.what () << " - out of memory?\n";
						}
						std::cout.flush ();
					}
					std::clog.flush ();
				}
			}
			std::cout << "\n";
			time_end (clockstart, wallstart);
		}

		// if (csvout) {
		// 	time_start (clockstart, wallstart);
		// 	std::cout << "\n * Writing particles to CSV\n";
		// 	std::cout.flush ();
		// 	f.open ("particles.csv", std::ofstream::app);
		// 	int i=1;
		// 	for (auto& v: vehicles) {
		// 		printf("\rVehicle %*d of %lu", 3, i, vehicles.bucket_count ());
		// 		std::cout.flush ();
		// 		i++;
		// 		for (auto& p: v.second->get_particles ()) {
		// 			if (rng.runif () < 0.8) continue;
		// 			int k0 = p.get_trajectory ().size () - v.second->get_delta ();
		// 			for (unsigned k=k0; k<p.get_trajectory ().size (); k++) {
		// 				f << v.second->get_id () << ","
		// 					<< v.second->get_trip ()->get_id () << ","
		// 					<< v.second->get_timestamp () << ","
		// 					<< p.get_id () << ","
		// 					<< p.get_start () + k << ","
		// 					<< p.get_trajectory ()[k] << "\n";
		// 			}
		// 		}
		// 	}
		// 	f.close ();
		// 	std::cout << "\n";
		// 	time_end (clockstart, wallstart);
		// }

		// Update road segments -> Kalman filter
		{
			time_start (clockstart, wallstart);
			std::cout << "\n * Updating road network with latest travel times ...";
			// loop over VEHICLES that were updated this iteration (?)
			for (auto& v: vehicles) {
				auto t = v.second->get_trip ();
				if (!t) continue;
				auto r = t->get_route ();
				if (!r) continue;
				auto sh = r->get_shape ();
				if (!sh) continue;
				auto sgs = sh->get_segments ();
				if (sgs.size () == 0) continue;
				// std::cout << "\n - Vehicle " << v.second->get_id () << " travel times ("
					// << sgs.size () << ")";
				int L = v.second->get_travel_times ().size ();
				for (int l=0; l<L; l++) {
					gtfs::TravelTime* tt = v.second->get_travel_time (l);
					// double len = tt.segment->get_length ();
					// int spd = 0;
					// if (tt.time > 0)
					// 	spd = round (60.0 * 60.0 / 1000.0 * len / tt.time);
					// printf("\n > Segment %*d of %*d: %*d m in %*d seconds (approx. %*d km/h) | %s",
					// 	2, l, 2, L, 4, int (len + 0.5), 3, tt.time, 3, spd, tt.complete ? "X" : " ");
					// printf("\n > Segment %*d of %*d: %*d seconds | %s",
						// 2, l, 2, L, 3, tt->time, tt->used && tt->complete ? "X" : (tt->complete ? "o" : " "));
					if (tt->time > 0 && tt->complete && !tt->used) {
						// std::cout << " -> adding ";
						tt->use ();
						// tt.used = true;
					}
				}
			}

			// Update segments and write to protocol buffer
			transit_network::Feed feed;
			// std::cout << "\n ~~~~~~~~~~~~~~~~~~~ \n";
			for (auto& s: gtfs.get_segments ()) {
				if (s.second->has_data ()) {
					std::cout << "\n + Update segment " << s.first << ": ";
					s.second->update (curtime);
				}
				transit_network::Segment* seg = feed.add_segments ();
				seg->set_segment_id (s.second->get_id ());
				if (s.second->is_initialized ()) {
					seg->set_travel_time (s.second->get_travel_time ());
					seg->set_travel_time_var (s.second->get_travel_time_var ());
					seg->set_timestamp (s.second->get_timestamp ());
					// and then set the LENGTH of each segment!
					seg->set_length (s.second->get_length ());
				}
				gps::Coord pt1;
				transit_network::Position* pstart = seg->mutable_start ();
				if (s.second->fromInt ()) {
					if (s.second->get_from ()) pt1 = s.second->get_from ()->get_pos ();
				} else {
					if (s.second->get_start ()) pt1 = s.second->get_start ()->get_pos ();
				}
				if (pt1.initialized ()) {
					pstart->set_lat ( pt1.lat );
					pstart->set_lng ( pt1.lng );
				}
				gps::Coord pt2;
				transit_network::Position* pend = seg->mutable_end ();
				if (s.second->toInt ()) {
					if (s.second->get_to ()) pt2 = s.second->get_to ()->get_pos ();
				} else {
					if (s.second->get_end ()) pt2 = s.second->get_end ()->get_pos ();
				}
				if (pt2.initialized ()) {
					pend->set_lat ( pt2.lat );
					pend->set_lng ( pt2.lng );
				}
			}

			std::fstream output ("networkstate.pb",
								 std::ios::out | std::ios::trunc | std::ios::binary);
			if (!feed.SerializeToOstream (&output)) {
				std::cerr << "\n x Failed to write ETA feed.\n";
			}

			// google::protobuf::ShutdownProtobufLibrary ();

			std::cout << "\n";
			time_end (clockstart, wallstart);
		}

		// Update ETA predictions
		{
			// time_start (clockstart, wallstart);
			// std::cout << "\n * Calculating ETAs ...";
			// std::cout.flush ();
			// for (auto& v: vehicles) {
			// 	if (!v.second->get_trip () || !v.second->is_initialized ()) continue;
			// 	// std::cout << "\n - Vehicle: " << v.second->get_id ()
			// 	// 	<< " - Time: " << v.second->get_timestamp ();
			// 	for (auto& p: v.second->get_particles ()) p.calculate_etas ();
			// }
			// std::cout << "\n";
			// time_end (clockstart, wallstart);
		}

		// Write vehicle positions to protobuf
		// + ETAs in future
		if (false) {
			time_start (clockstart, wallstart);
			std::cout << "\n * Writing to buffer ...";
			std::cout.flush ();

			transit_network::Feed feed;
			transit_network::Status* nw = feed.mutable_status ();
			nw->set_ontime (0);

			for (auto& v: vehicles) {
				if (!v.second->get_trip () || v.second->get_status () < 0) continue;
				transit_network::Vehicle* vehicle = feed.add_vehicles ();
				vehicle->set_id (v.second->get_id ().c_str ());
				vehicle->set_trip_id (v.second->get_trip ()->get_id ().c_str ());
				vehicle->set_timestamp (v.second->get_timestamp ());

				if (v.second->get_delay ()) vehicle->set_delay (v.second->get_delay ().get ());



				transit_network::Position* pos = vehicle->mutable_pos ();
				pos->set_lat (v.second->get_position ().lat);
				pos->set_lng (v.second->get_position ().lng);

				double dist = 0.0;//, speed = 0.0;
				int Np = 0;
				// std::clog << "\n Vehicle " << v.second->get_id ()
				// 	<< ": ";
				for (auto& p: v.second->get_particles ()) {
					if (p.get_trajectory ().size () == 0) continue;
					Np++;
					dist += p.get_distance ();
					// speed += p.get_velocity (p.get_latest ());
				}
				if (Np > 0) {
					// std::clog << " d=" << dist/Np;// << ", v=" << speed/Np;
					pos->set_distance (dist / Np);
					// pos->set_speed (speed / Np);
				}
			}

			std::fstream output ("networkstate.pb",
								 std::ios::out | std::ios::trunc | std::ios::binary);
			if (!feed.SerializeToOstream (&output)) {
				std::cerr << "\n x Failed to write ETA feed.\n";
			}

			google::protobuf::ShutdownProtobufLibrary ();

			std::cout << "\n";
			time_end (clockstart, wallstart);
		}

		// Write ETAs to buffers
		{
			// time_start (clockstart, wallstart);
			// std::cout << "\n * Writing ETAs to protocol buffer ...";
			// std::cout.flush ();
			//
			// transit_etas::Feed feed;
			//
			// for (auto& v: vehicles) {
			// 	if (!v.second->get_trip () || !v.second->is_initialized ()) continue;
			// 	transit_etas::Trip* trip = feed.add_trips ();
			// 	trip->set_vehicle_id (v.second->get_id ().c_str ());
			// 	trip->set_trip_id (v.second->get_trip ()->get_id ().c_str ());
			// 	trip->set_route_id (v.second->get_trip ()->get_route ()->get_id ().c_str ());
			// 	double dist = 0, speed = 0;
			// 	for (auto& p: v.second->get_particles ()) {
			// 		dist += p.get_distance ();
			// 		speed += p.get_velocity ();
			// 	}
			// 	trip->set_distance_into_trip (dist / v.second->get_particles ().size ());
			// 	trip->set_velocity (speed / v.second->get_particles ().size ());
			//
			// 	// Initialize a vector of ETAs for each particles; stop by stop
			// 	unsigned Np (v.second->get_particles ().size ());
			// 	std::vector<uint64_t> etas;
			// 	etas.reserve (Np);
			//
			// 	auto stops = v.second->get_trip ()->get_stoptimes ();
			// 	for (unsigned j=1; j<stops.size (); j++) {
			// 		// For each stop, fetch ETAs for that stop
			// 		for (auto& p: v.second->get_particles ()) {
			// 			if (p.get_eta (j) == 0 || p.is_finished ()) continue;
			// 			etas.push_back (p.get_eta (j));
			// 		}
			// 		if (etas.size () == 0) continue;
			// 		// order them to get percentiles: 0.025, 0.5, 0.975
			// 		std::sort (etas.begin (), etas.end ());
			//
			// 		// append to tripetas
			// 		transit_etas::Trip::ETA* tripetas = trip->add_etas ();
			// 		tripetas->set_stop_sequence (j+1);
			// 		tripetas->set_stop_id (stops[j].stop->get_id ().c_str ());
			// 		tripetas->set_arrival_min (etas[(int)(etas.size () * 0.025)]);
			// 		tripetas->set_arrival_max (etas[(int)(etas.size () * 0.975)]);
			// 		tripetas->set_arrival_eta (etas[(int)(etas.size () * 0.5)]);
			//
			// 		// clear for next stop
			// 		etas.clear ();
			// 	}
			// }
			//
			// std::fstream output ("gtfs_etas.pb",
			// 					 std::ios::out | std::ios::trunc | std::ios::binary);
			// if (!feed.SerializeToOstream (&output)) {
			// 	std::cerr << "\n x Failed to write ETA feed.\n";
			// }
			//
			// google::protobuf::ShutdownProtobufLibrary ();
			//
			// std::cout << "\n";
			// time_end (clockstart, wallstart);
		}


		if (forever) std::this_thread::sleep_for (std::chrono::milliseconds (5 * 1000));
	}

	return 0;
}



/**
 * Load a feed message into vehicle object vector
 * @param vs        reference to vector of vehicle pointers
 * @param feed_file reference to feed
 * @param N         the number of particle to initialze new vehicles with
 * @param rng       reference to a random number generator
 * @param gtfs      A GTFS object containing the static data
 * @param t         pointer to the "current" time ...
 * @return          true if the feed is loaded correctly, false if it is not
 */
bool load_feed (std::unordered_map<std::string, std::unique_ptr<gtfs::Vehicle> > &vs,
				std::string &feed_file, int N, sampling::RNG &rng,
				gtfs::GTFS &gtfs, time_t *filetime) {
	transit_realtime::FeedMessage feed;
	std::cout << "Checking for vehicle updates in feed: " << feed_file << " ... ";
	std::fstream feed_in (feed_file, std::ios::in | std::ios::binary);
	if (!feed_in) {
		std::cerr << "file not found!\n";
		return false;
	} else if (!feed.ParseFromIstream (&feed_in)) {
		std::cerr << "failed to parse GTFS realtime feed!\n";
		return false;
	} else {
		std::cout << "done -> " << feed.entity_size () << " updates loaded.\n";
		if (feed.header ().has_timestamp ()) {
			*filetime = feed.header ().timestamp ();
			// std::cout << " [ time = " << feed.header ().timestamp () << "]\n";
		}
	}

	// Cycle through feed entities and update associated vehicles, or create a new one.
	std::vector<std::string> KEEPtrips;

	sqlite3* db;
	sqlite3_stmt* tripskeep;
	std::string qry = "SELECT trip_id FROM trips WHERE route_id IN "
		"(SELECT route_id FROM routes WHERE route_short_name IN "
		// "('NEX'))";
		"('274','277','224','222','258','NEX','129'))";
		//"('277', '274'))";
	if (sqlite3_open (gtfs.get_dbname ().c_str (), &db)) {
		std::cerr << "\n x oops...";
	} else if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &tripskeep, 0) != SQLITE_OK) {
		std::cerr << "\n x oops2...";

	} else {
		while (sqlite3_step (tripskeep) == SQLITE_ROW) {
			std::string t = (char*)sqlite3_column_text (tripskeep, 0);
			KEEPtrips.push_back (t);
		}
	}

	for (int i=0; i<feed.entity_size (); i++) {
		printf(" * Processing feed: %*d%%\r", 3, (int)(100 * (i+1) / feed.entity_size ()));
		std::cout.flush ();
		auto& ent = feed.entity (i);
		if (KEEPtrips.size () > 0) {
			if (ent.has_trip_update () && ent.trip_update ().has_trip () &&
				ent.trip_update ().trip ().has_trip_id ()) {
				// std::cout << "\n + " << ent.trip_update ().trip ().trip_id ();
				if (std::find (KEEPtrips.begin (), KEEPtrips.end (),
							   ent.trip_update ().trip ().trip_id ()) == KEEPtrips.end ()) continue;
		    } else if (ent.has_vehicle () && ent.vehicle ().has_trip () &&
					   ent.vehicle ().trip ().has_trip_id ()) {
				if (std::find (KEEPtrips.begin (), KEEPtrips.end (),
							   ent.vehicle ().trip ().trip_id ()) == KEEPtrips.end ()) continue;
		    } else {
				continue;
			}
		}
		std::string vid;
		if (ent.has_trip_update () && ent.trip_update ().has_vehicle ()) {
			vid = ent.trip_update ().vehicle ().id ();
		} else if (ent.has_vehicle () && ent.vehicle ().has_vehicle ()) {
			vid = ent.vehicle().vehicle ().id ();
		}
		if (vs.find (vid) == vs.end ()) {
			// vehicle doesn't already exist - create it
			vs.emplace (vid, std::unique_ptr<gtfs::Vehicle> (new gtfs::Vehicle (vid, N)));
		}
		if (ent.has_vehicle ()) vs[vid]->update (ent.vehicle (), gtfs);
		if (ent.has_trip_update ()) vs[vid]->update (ent.trip_update (), gtfs);

		std::cout.flush ();
	}
	std::cout << "\n";

	return true;
}

/**
 * Start timer.
 * @param clock a CPU clock
 * @param wall  a wall clock
 */
void time_start (std::clock_t& clock, std::chrono::high_resolution_clock::time_point& wall) {
	clock = std::clock ();
	wall  = std::chrono::high_resolution_clock::now ();
	std::cout << "\n\n >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Timer started\n";
	std::cout.flush ();
};

/**
 * End a timer and print the results.
 * @param clock a CPU clock
 * @param wall  a wall clock
 */
void time_end (std::clock_t& clock, std::chrono::high_resolution_clock::time_point& wall) {
	auto clockend = std::clock ();
	auto wallend  = std::chrono::high_resolution_clock::now ();

	auto clock_dur = (clockend - clock) / (double)(CLOCKS_PER_SEC / 1000);
	auto wall_dur  = std::chrono::duration<double, std::milli> (wallend - wall).count ();
	printf ("\n <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< CPU: %*.3f ms        Wall: %*.3f ms\n\n",
			9, clock_dur, 9, wall_dur);
	std::cout.flush ();
};
