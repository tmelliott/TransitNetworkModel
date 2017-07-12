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
#include "sampling.h"
#include "gtfs.h"
#include "gps.h"

namespace po = boost::program_options;

bool load_feed (std::unordered_map<std::string, std::unique_ptr<gtfs::Vehicle> > &vs,
				std::string &feed_file, int N, sampling::RNG &rng,
				gtfs::GTFS &gtfs);
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
	std::string version;
	/** number of particles per vehicle */
	int N;

	desc.add_options ()
		("files", po::value<std::vector<std::string> >(&files)->multitoken (),
			"GTFS Realtime protobuf feed files.")
		("database", po::value<std::string>(&dbname), "Database Connection to use.")
		("version", po::value<std::string>(&version), "Version number to pull subset from database.")
		("N", po::value<int>(&N)->default_value(1000), "Number of particles to initialize each vehicle.")
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
	if (!vm.count ("version")) {
		std::cout << "WARNING: version number not specified; entire database will be loaded!\n";
		version = "";
	}

	std::clock_t clockstart;
	std::chrono::high_resolution_clock::time_point wallstart;

	// Load the global GTFS database object:
	time_start (clockstart, wallstart);
	gtfs::GTFS gtfs (dbname, version);
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

	while (forever) {
		// forever = false;
		{
			// Load GTFS feed -> vehicles
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
			time_end (clockstart, wallstart);

			std::cout << "\n";
			// -> triggers particle transition -> resample
			time_start (clockstart, wallstart);
			// #pragma omp parallel// for schedule(dynamic) num_threads(3)
			// {
			// 	#pragma omp single
			// 	{
					for (auto& v: vehicles) {
						// #pragma omp task firstprivate(v)
						// {
							if (v.second->get_trip () &&
								v.second->get_trip ()->get_route ()) {
								std::cout << "\n ++++++++++++++++++++++++++++++++++++++++ Route "
								<< v.second->get_trip ()->get_route ()->get_short_name () << "\n";
								v.second->update (rng);
							}
						// }
					}
			// 	}
			// }
			std::cout << "\n";
			time_end (clockstart, wallstart);
		}

		{
			// Update road segments -> Kalman filter

		}

		{
			// Update ETA predictions
			time_start (clockstart, wallstart);
			std::cout << "\n * Calculating ETAs ...";
			for (auto& v: vehicles) {
				if (!v.second->get_trip ()) continue;
				for (auto& p: v.second->get_particles ()) p.calculate_etas ();
			}
			std::cout << "\n";
			time_end (clockstart, wallstart);
		}

		{
			// Write results to CSV files
			time_start (clockstart, wallstart);
			std::cout << "\n * Writing particles to CSV ...";
			system("rm -f PARTICLES.csv ETAS.csv");
			std::ofstream particlefile; // file for particles
			particlefile.open ("PARTICLES.csv");
			particlefile << "vehicle_id,particle_id,timestamp,trip_id,distance,velocity,parent_id,lat,lng\n";
			std::ofstream etafile;      // file for particles/stop ETAs
			etafile.open ("ETAS.csv");
			etafile << "vehicle_id,particle_id,stop_sequence,eta\n";
			for (auto& v: vehicles) {
				if (!v.second->get_trip ()) continue;
				auto shape = v.second->get_trip ()->get_route ()->get_shape ();
				for (auto& p: v.second->get_particles ()) {
					auto pos = gtfs::get_coords (p.get_distance (), shape);
					particlefile
						<< v.second->get_id () << "," << p.get_id () << ","
					 	<< v.second->get_timestamp () << "," << v.second->get_trip ()->get_id () << ","
						<< p.get_distance () << "," << p.get_velocity () << ","
						<< p.get_parent_id () << "," << pos.lat << "," << pos.lng << "\n";
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

		if (false) {
			// Write results to file
			time_start (clockstart, wallstart);
			std::cout << "\n * Writing particles to db ...";
			std::cout.flush ();
			sqlite3* db;
			sqlite3_stmt* stmt_del;
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

	// sqlite3* db;
	// sqlite3_stmt* tripskeep;
	// std::string qry = "SELECT trip_id FROM trips WHERE route_id IN "
	// 	"(SELECT route_id FROM routes WHERE route_short_name IN "
	// 	"('274'))";
	// 	// "('274','277','224','222','258','NEX','129'))";
	// if (sqlite3_open (gtfs.get_dbname ().c_str (), &db)) {
	// 	std::cerr << "\n x oops...";
	// } else if (sqlite3_prepare_v2 (db, qry.c_str (), -1, &tripskeep, 0) != SQLITE_OK) {
	// 	std::cerr << "\n x oops2...";
	//
	// } else {
	// 	while (sqlite3_step (tripskeep) == SQLITE_ROW) {
	// 		std::string t = (char*)sqlite3_column_text (tripskeep, 0);
	// 		KEEPtrips.push_back (t);
	// 	}
	// }

	for (int i=0; i<feed.entity_size (); i++) {
		printf(" * Processing feed: %*d%%\r", 3, (int)(100 * (i+1) / feed.entity_size ()));
		std::cout.flush ();
		auto& ent = feed.entity (i);
		if (KEEPtrips.size () > 0) {
			if (ent.has_trip_update () && ent.trip_update ().has_trip () &&
				ent.trip_update ().trip ().has_trip_id ()) {
				std::cout << "\n + " << ent.trip_update ().trip ().trip_id ();
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
