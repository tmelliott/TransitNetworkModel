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
				gtfs::GTFS &gtfs);
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

	// PROGRAM COMMAND LINE ARGUMNETS
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

	bool csvout;

	desc.add_options ()
		("files", po::value<std::vector<std::string> >(&files)->multitoken (),
			"GTFS Realtime protobuf feed files.")
		("database", po::value<std::string>(&dbname), "Database Connection to use.")
		// ("version", po::value<std::string>(&version), "Version number to pull subset from database.")
		("N", po::value<int>(&N)->default_value(1000), "Number of particles to initialize each vehicle.")
		("numcore", po::value<int>(&numcore)->default_value(1), "Number of cores to use.")
		("csv", po::value<bool>(&csvout)->default_value(false), "Setting to true will cause all particles and their ETAs to be written to PARTICLES.csv and ETAs.csv, respectively. WARNING: slow!")
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

	// std::cout << " * Loaded " << gtfs.get_stops ().size () << " stops\n";
	// std::cout << " * Loaded " << gtfs.get_intersections ().size () << " intersections\n";
	// std::cout << " * Loaded " << gtfs.get_segments ().size () << " segments\n\n";
	//
	// std::cout << " * Loaded " << gtfs.get_trips ().size () << " trips\n";
	// std::cout << " * Loaded " << gtfs.get_routes ().size () << " routes\n";
	// std::cout << " * Loaded " << gtfs.get_shapes ().size () << " shapes\n";


	// std::clock_t clockend;

	system("rm -f PARTICLES.csv ETAS.csv");
	std::ofstream particlefile; // file for particles
	particlefile.open ("PARTICLES.csv");
	particlefile << "vehicle_id,particle_id,timestamp,trip_id,distance,velocity,parent_id,lat,lng,lh,init\n";
	particlefile.close ();
	std::ofstream etafile;      // file for particles/stop ETAs
	etafile.open ("ETAS.csv");
	etafile << "vehicle_id,particle_id,stop_sequence,eta\n";
	etafile.close ();

	time_t curtime;
	while (forever) {
		curtime = time (NULL);

		// Load GTFS feed -> vehicles
		{
            // auto wallstart = std::chrono::high_resolution_clock::now();
            // auto wallend = std::chrono::high_resolution_clock::now();

			time_start (clockstart, wallstart);
			for (auto file: files) {
				try {
					if ( ! load_feed (vehicles, file, N, rng, gtfs) ) {
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

		// Update each vehicle's particles
		{
			// -> triggers particle transition -> resample
			time_start (clockstart, wallstart);
			std::cout << "\n * Running particle filter ...\n";
			std::cout.flush ();
			#pragma omp parallel for schedule(static) num_threads(numcore)
			for (unsigned i=0; i<vehicles.bucket_count (); i++) {
				// std::cout << "\n - vehicle " << i;
				for (auto v = vehicles.begin (i); v != vehicles.end (i); v++) {
					if (v->second->get_trip () &&
						v->second->get_trip ()->get_route ()) {
						// std::cout << "\n ++++++++++++++++++++++++++++++++++++++++ Route "
						// << v->second->get_trip ()->get_route ()->get_short_name () << "\n";
						v->second->update (rng);
					}
				}
			}
			std::cout << "\n";
			time_end (clockstart, wallstart);
		}

		// Update road segments -> Kalman filter
		{
			time_start (clockstart, wallstart);
			std::cout << "\n * Updating road network with latest travel times ...";
			// loop over VEHICLES that were updated this iteration (?)
			for (auto& v: vehicles) {
				// std::cout << "\n - Vehicle " << v.second->get_id () << " travel times ("
				// 	<< v.second->get_particles()[0].get_travel_times ().size ()
				// 	<< v.second->get_trip ()->get_route ()->get_shape ()->get_segments ().size ()
				// 	<< ")";
				if (!v.second->is_initialized ()) {
					// std::cout << " -- not init";
					continue;
				}
				// only use segments that are LESS than MIN segment index
				int minSeg = v.second->get_trip ()->get_route ()->get_shape ()->get_segments ().size ();
				for (auto& p: v.second->get_particles ()) {
					if (p.get_segment_index () < minSeg) minSeg = p.get_segment_index ();
				}
				// std::cout << " - " << minSeg;
				std::vector<int> tts;
				tts.reserve (v.second->get_particles ().size ());
				for (int i=0; i<minSeg; i++) {
					tts.clear ();
					std::shared_ptr<gtfs::Segment> segi;
					for (auto& p: v.second->get_particles ()) {
						// if COMPLETE && INITIALIZED then append travel time to segment's data
						// AND uninitialize the particle's travel time for that segment
						// so it doesn't get reused next time
						auto tt = p.get_travel_time (i);
						if (!tt) break;
						if (!tt->initialized || !tt->complete) break; // if any aren't, skip anyway!
						if (!segi) segi = tt->segment;
						tts.push_back (tt->time);
					}
					// std::cout << " (" << tts.size () << " vs "
					// 	<< v.second->get_particles ().size () << ")?";
					if (tts.size () != v.second->get_particles ().size ()) continue;
					double ttmean = std::accumulate(tts.begin (), tts.end (), 0.0) / tts.size ();
					double sqdiff = 0;
					for (auto& t: tts) sqdiff += pow(t - ttmean, 2);
					double var = sqdiff / tts.size ();
					// std::cout << "\n + Segment " << i << ": " << ttmean << " (" << sqrt(var) << ")";
					segi->add_data (ttmean, var);
					// give data to segment
					for (auto& p: v.second->get_particles ()) p.reset_travel_time (i);
				}
			}

			// Update segments and write to protocol buffer
			transit_network::Feed feed;
			// std::cout << "\n ~~~~~~~~~~~~~~~~~~~ \n";
			for (auto& s: gtfs.get_segments ()) {
				if (s.second->has_data ()) {
					// std::cout << "\n + Update segment " << s.first << ": ";
					s.second->update (curtime);
				}
				transit_network::Segment* seg = feed.add_segments ();
				seg->set_segment_id (s.second->get_id ());
				if (s.second->is_initialized ()) {
					seg->set_travel_time (s.second->get_travel_time ());
					seg->set_travel_time_var (s.second->get_travel_time_var ());
					seg->set_timestamp (s.second->get_timestamp ());
				}
			}

			std::fstream output ("gtfs_network.pb",
								 std::ios::out | std::ios::trunc | std::ios::binary);
			if (!feed.SerializeToOstream (&output)) {
				std::cerr << "\n x Failed to write ETA feed.\n";
			}

			google::protobuf::ShutdownProtobufLibrary ();

			std::cout << "\n";
			time_end (clockstart, wallstart);
		}

		// Update ETA predictions
		{
			time_start (clockstart, wallstart);
			std::cout << "\n * Calculating ETAs ...";
			std::cout.flush ();
			for (auto& v: vehicles) {
				if (!v.second->get_trip () || !v.second->is_initialized ()) continue;
				// std::cout << "\n - Vehicle: " << v.second->get_id ()
				// 	<< " - Time: " << v.second->get_timestamp ();
				for (auto& p: v.second->get_particles ()) p.calculate_etas ();
			}
			std::cout << "\n";
			time_end (clockstart, wallstart);
		}

		// Write ETAs to buffers
		{
			time_start (clockstart, wallstart);
			std::cout << "\n * Writing ETAs to protocol buffer ...";
			std::cout.flush ();

			transit_etas::Feed feed;

			for (auto& v: vehicles) {
				if (!v.second->get_trip () || !v.second->is_initialized ()) continue;
				transit_etas::Trip* trip = feed.add_trips ();
				trip->set_vehicle_id (v.second->get_id ().c_str ());
				trip->set_trip_id (v.second->get_trip ()->get_id ().c_str ());
				trip->set_route_id (v.second->get_trip ()->get_route ()->get_id ().c_str ());
				double dist = 0, speed = 0;
				for (auto& p: v.second->get_particles ()) {
					dist += p.get_distance ();
					speed += p.get_velocity ();
				}
				trip->set_distance_into_trip (dist / v.second->get_particles ().size ());
				trip->set_velocity (speed / v.second->get_particles ().size ());

				// Initialize a vector of ETAs for each particles; stop by stop
				unsigned Np (v.second->get_particles ().size ());
				std::vector<uint64_t> etas;
				etas.reserve (Np);

				auto stops = v.second->get_trip ()->get_stoptimes ();
				for (unsigned j=1; j<stops.size (); j++) {
					// For each stop, fetch ETAs for that stop
					for (auto& p: v.second->get_particles ()) {
						if (p.get_eta (j) == 0 || p.is_finished ()) continue;
						etas.push_back (p.get_eta (j));
					}
					if (etas.size () == 0) continue;
					// order them to get percentiles: 0.025, 0.5, 0.975
					std::sort (etas.begin (), etas.end ());

					// append to tripetas
					transit_etas::Trip::ETA* tripetas = trip->add_etas ();
					tripetas->set_stop_sequence (j+1);
					tripetas->set_stop_id (stops[j].stop->get_id ().c_str ());
					tripetas->set_arrival_min (etas[(int)(etas.size () * 0.025)]);
					tripetas->set_arrival_max (etas[(int)(etas.size () * 0.975)]);
					tripetas->set_arrival_eta (etas[(int)(etas.size () * 0.5)]);

					// clear for next stop
					etas.clear ();
				}
			}

			std::fstream output ("gtfs_etas.pb",
								 std::ios::out | std::ios::trunc | std::ios::binary);
			if (!feed.SerializeToOstream (&output)) {
				std::cerr << "\n x Failed to write ETA feed.\n";
			}

			google::protobuf::ShutdownProtobufLibrary ();

			std::cout << "\n";
			time_end (clockstart, wallstart);
		}

		// Write results to CSV files
		if (csvout) {
			time_start (clockstart, wallstart);
			std::cout << "\n * Writing particles to CSV ...";
			std::cout.flush ();
			particlefile.open ("PARTICLES.csv", std::ofstream::app);
			etafile.open ("ETAS.csv", std::ofstream::app);
			for (auto& v: vehicles) {
				if (!v.second->get_trip ()) continue;
				auto shape = v.second->get_trip ()->get_route ()->get_shape ();
				for (auto& p: v.second->get_particles ()) {
					auto pos = gtfs::get_coords (p.get_distance (), shape);
					particlefile
						<< v.second->get_id () << "," << p.get_id () << ","
					 	<< v.second->get_timestamp () << "," << v.second->get_trip ()->get_id () << ","
						<< p.get_distance () << "," << p.get_velocity () << ","
						<< p.get_parent_id () << "," << pos.lat << "," << pos.lng << ","
						<< p.get_likelihood ()  << ","
						<< v.second->is_initialized () << "\n";
					if (p.get_etas ().size () > 0) {
						for (unsigned int i=0; i<p.get_etas ().size (); i++) {
							if (p.get_eta (i) && p.get_eta (i) > 0) {
								etafile
									<< v.second->get_id () << "," << p.get_id () << ","
									<< (i+1) << "," << p.get_eta (i) << "\n";
							}
						}
					}
				}
			}
			particlefile.close ();
			etafile.close ();
			std::cout << "\n";
			time_end (clockstart, wallstart);
		}

		// Write results to file - depreciated
		if (false) {
			time_start (clockstart, wallstart);
			std::cout << "\n * Writing particles to db ...";
			std::cout.flush ();
			sqlite3* db;
			// sqlite3_stmt* stmt_del;
			sqlite3_stmt* stmt_ins;
			if (sqlite3_open (dbname.c_str (), &db)) {
				fprintf(stderr, " * Can't open db connection: %s\n", sqlite3_errmsg (db));
			} else {
				sqlite3_exec (db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
				sqlite3_exec (db, "DELETE FROM particles", NULL, NULL, NULL);

				// if (sqlite3_prepare_v2 (db, "DELETE FROM particles WHERE vehicle_id = $1",
				// 						-1, &stmt_del, 0) != SQLITE_OK) {
				// 	std::cerr << "\n  x Unable to prepare DELETE query ";
				// }
				if (sqlite3_prepare_v2 (db, "INSERT INTO particles VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12)",
					 	   					   -1, &stmt_ins, 0) != SQLITE_OK) {
					std::cerr << "\n  x Unable to prepare INSERT query ";
				} else {
					// prepared OK
					for (auto& v: vehicles) {
						// if (! v.second->is_initialized ()) continue;
						if (!v.second->get_trip ()) continue;
						std::string tid = v.second->get_trip ()->get_id ();//.c_str ();
						// if (sqlite3_bind_text (stmt_del, 1, v.second->get_id ().c_str (),
						// 					   -1, SQLITE_STATIC) != SQLITE_OK ||
						//     sqlite3_step (stmt_del) != SQLITE_DONE) {
						// 	std::cerr << "\n  x Unable to delete particles ";
						// }
						// sqlite3_reset (stmt_del);
						// std::cout << tid.c_str () << "\n";
						auto shape = v.second->get_trip ()->get_route ()->get_shape ();
						for (auto& p: v.second->get_particles ()) {
							sqlite3_bind_int (stmt_ins, 1, p.get_id ());
							sqlite3_bind_text (stmt_ins, 2, v.second->get_id ().c_str (), -1, SQLITE_TRANSIENT);
							sqlite3_bind_text (stmt_ins, 3, tid.c_str (), -1, SQLITE_TRANSIENT);
							sqlite3_bind_double (stmt_ins, 4, p.get_distance ());
							sqlite3_bind_double (stmt_ins, 5, p.get_velocity ());
							sqlite3_bind_int64 (stmt_ins, 6, v.second->get_timestamp ());
							sqlite3_bind_double (stmt_ins, 7, p.get_likelihood ());
							sqlite3_bind_int (stmt_ins, 8, p.get_parent_id ());
							sqlite3_bind_int (stmt_ins, 9, v.second->is_initialized ());
							// Write the ETAs
							auto etas = p.get_etas ();
							std::string etastr = "";
							if (etas.size () > 0) {
								etastr = boost::algorithm::join (etas |
									boost::adaptors::transformed (static_cast<std::string(*)(uint64_t)>(std::to_string) ), ",");
								// std::cout << etastr << "|  ";
							}
							sqlite3_bind_text (stmt_ins, 10, etastr.c_str (), -1, SQLITE_TRANSIENT);

							auto pos = gtfs::get_coords (p.get_distance (), shape);
							sqlite3_bind_double (stmt_ins, 11, pos.lat);
							sqlite3_bind_double (stmt_ins, 12, pos.lng);

							if (sqlite3_step (stmt_ins) != SQLITE_DONE) {
								std::cerr << " x Error inserting value: " << sqlite3_errmsg (db);
							}
							sqlite3_reset (stmt_ins);
						}
					}
				}
				sqlite3_finalize (stmt_ins);

				char* zErrMsg = 0;
				if (sqlite3_exec (db, "COMMIT", NULL, NULL, &zErrMsg) == 0) {
					std::cout << " done\n";
				} else {
					std::cout << "\nTransaction couldn't complete: " << zErrMsg << "\n";
				}
				sqlite3_close (db);
			}
			time_end (clockstart, wallstart);
		}

		if (forever) std::this_thread::sleep_for (std::chrono::milliseconds (10 * 1000));
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
 * @return          true if the feed is loaded correctly, false if it is not
 */
bool load_feed (std::unordered_map<std::string, std::unique_ptr<gtfs::Vehicle> > &vs,
				std::string &feed_file, int N, sampling::RNG &rng,
				gtfs::GTFS &gtfs) {
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
	}

	// Cycle through feed entities and update associated vehicles, or create a new one.
	std::vector<std::string> KEEPtrips;

	sqlite3* db;
	sqlite3_stmt* tripskeep;
	std::string qry = "SELECT trip_id FROM trips WHERE route_id IN "
		"(SELECT route_id FROM routes WHERE route_short_name IN "
		// "('274','277','224','222','258','NEX','129'))";
		"('274'))";
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
