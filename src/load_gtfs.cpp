/**
* Load the most recent GTFS static data from Auckland Transport.
*
* This program will populate the supplied database with the most recent
* version of the GTFS static feed from Auckland Transport.
*
* Segmentation will take place, in which shapes are split at intersections.
* Routes will be compared to existing routes, and if there are no changes
* the segments will simply be duplicated;
* otherwise new segments will be created.
*
* @file
* @author Tom Elliott <tom.elliott@auckland.ac.nz>
* @version 0.0.1
*/

#include <iostream>
#include <vector>
#include <stdlib.h>
#include <fstream>
#include <algorithm>

#include <boost/program_options.hpp>
#include <sqlite3.h>

#include "gps.h"
#include "gtfs.h"

#include "json.hpp"

namespace po = boost::program_options;

/**
 * A system() command that takes a std::string instead of a char.
 * @param  s The command to run.
 * @return   The return value from the system command.
 */
int system (std::string const& s) { return system (s.c_str ()); }
void convert_shapes (sqlite3* db);
void import_intersections (sqlite3* db, std::vector<std::string> files);
void segment_shapes (sqlite3* db);
void calculate_stop_distances (std::string& dbname);

/**
 * Loads GTFS file into database and segments as necessary.
 *
 * @param  argc number of command line arguments
 * @param  argv arguments
 * @return      0 if everything went OK; 1 if it didn't
 */
int main (int argc, char* argv[]) {

	po::options_description desc ("Allowed options");

	/** database connection to use */
	std::string dbname;
	std::string dir;

	desc.add_options ()
		("database", po::value<std::string>(&dbname)->default_value ("gtfs.db"), "Name of the database to use.")
		("dir", po::value<std::string>(&dir)->default_value (".."), "Directory of the database files. Defaults to ..")
		("help", "Print this message and exit.")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count ("help")) {
		std::cout << desc << "\n";
		return 1;
	}


	// STEP ONE:
	// connect to the SQLite database:
	std::cout << "Loading GTFS data into database `" << dbname << "`\n";
	sqlite3 *db;

	if (sqlite3_open ((dir + "/" + dbname).c_str(), &db)) {
		fprintf(stderr, " * Can't open database: %s\n", sqlite3_errmsg(db));
      	return(0);
	} else {
    	fprintf(stderr, " * Opened database successfully\n");
	}

	// // STEP TWO:
	// // convert shapes -> segments
	// std::cout << " * Converting shapes to segments ... ";
	// convert_shapes (db); // -- temporary dont let it run (though it should die since shapes_tmp not present)
	// std::cout << "\n   ... done.\n";
	//
	// // STEP THREE:
	// // importing intersections.json and segmenting segments:
	// std::cout << " * Importing intersections ... ";
	// std::vector<std::string> files {dir + "/data/intersections_trafficlights.json",
	// 								dir + "/data/intersections_roundabouts.json"};
	// import_intersections (db, files);
	// std::cout << "done.\n";

	// STEP FOUR: get all segments, and split into more segments
	segment_shapes (db);

	// That's enough of the database connection ...
	sqlite3_close (db);


	// // STEP FIVE: stop distance into shape for stop_times
	// std::cout << " * Calculating distance into trip of stops ... \n";
	// std::string dbn = dir + "/" + dbname;
	// calculate_stop_distances (dbn);
	// std::cout << "\n   ... done.\n";

	return 0;
}


/**
 * Converts shapes from their raw GTFS format to our segmented format.
 *
 * For now, each shape is simply converted into a single segment.
 * @param db the database to work with
 */
void convert_shapes (sqlite3* db) {
	sqlite3_stmt* stmt;
	sqlite3_stmt* stmt_get_shape;
	sqlite3_stmt* stmt_seg_ins;
	sqlite3_stmt* stmt_shape_ins;
	sqlite3_stmt* stmt_shapept_ins;

	if (sqlite3_prepare_v2 (db, "SELECT DISTINCT shape_id FROM shapes_tmp",
							-1, &stmt, 0) == SQLITE_OK) {
		std::cout << "\n   - SELECT query prepared ";
	} else {
		std::cerr << "\n   x Unable to prepare query ";
		return;
	}

	if (sqlite3_prepare_v2 (db, (char*)("SELECT shape_pt_lat, shape_pt_lon FROM shapes_tmp "
								 		"WHERE shape_id = ?1 ORDER BY CAST(shape_pt_sequence AS INT)"),
							-1, &stmt_get_shape, 0) == SQLITE_OK) {
		std::cout << "\n   - SELECT shape query prepared ";
	} else {
		std::cerr << "\n   x Unable to prepare shape query ";
		return;
	}

	if (sqlite3_prepare_v2  (db, "INSERT INTO segments (length) VALUES (?1)",
							 -1, &stmt_seg_ins, 0) == SQLITE_OK) {
	    std::cout << "\n   - INSERT query prepared ";
	} else {
	    std::cerr << "\n   x Unable to prepare INSERT query ";
		return;
	}

	if (sqlite3_prepare_v2 (db, "INSERT INTO shapes VALUES (?1, 1, ?2, 0)",
							-1, &stmt_shape_ins, 0) == SQLITE_OK) {
		std::cout << "\n   - INSERT shape prepared ";
	} else {
		std::cerr << "\n   x Error preparing shape insert query ";
		return;
	}

	if (sqlite3_prepare_v2 (db, "INSERT INTO segment_pt VALUES (?1, ?2, ?3, ?4, ?5)",
							-1, &stmt_shapept_ins, 0) == SQLITE_OK) {
		std::cout << "\n   - INSERT shape points prepared ";
	} else {
		std::cerr << "\n   x Error preparing shape point insert query ";
		return;
	}

	// For a progress bar (because we can't not have one!)
	int nshapes = 0;
	sqlite3_stmt* stmt_nrow;
	if (sqlite3_prepare_v2 (db, "SELECT COUNT(DISTINCT shape_id) FROM shapes_tmp",
							-1, &stmt_nrow, 0) == SQLITE_OK &&
		sqlite3_step (stmt_nrow) == SQLITE_ROW) {
		nshapes = sqlite3_column_int (stmt_nrow, 0);
	}
	std::cout << "nshapes = " << nshapes;
	sqlite3_finalize (stmt_nrow);

	std::cout << "\n";
	int n = 0;
	while (sqlite3_step (stmt) == SQLITE_ROW) {
		if (nshapes > 0) {
			n++;
			printf("     + Shape %*d of %d (%*d%%) \r", 4, n, nshapes, 3, (int)(100 * (n) / nshapes));
			std::cout.flush();
		}
	    std::string shape_id = (char*)sqlite3_column_text (stmt, 0);
	    std::clog << "\n   +++ " << shape_id;

	    // LOAD SHAPE - COMPUTE LENGTH
		const char* sid = shape_id.c_str ();
		if (sqlite3_bind_text (stmt_get_shape, 1, sid, -1, SQLITE_STATIC) != SQLITE_OK) {
			std::cerr << "\n     x Error binding shape ID to query ";
			continue;
		} else {
			std::clog << "\n     + Shape ID bound successfully ";
		}

		std::vector<gps::Coord> path;
		while (sqlite3_step (stmt_get_shape) == SQLITE_ROW) {
			path.emplace_back (sqlite3_column_double (stmt_get_shape, 0),
							   sqlite3_column_double (stmt_get_shape, 1));
		}
		sqlite3_reset (stmt_get_shape);

		double length = 0.0;
		for (unsigned int i=1; i<path.size(); i++) {
			length += path[i-1].distanceTo (path[i]);
		}

	    // INSERT INTO segments (length) VALUES (?) - [length]
	    if (sqlite3_bind_double (stmt_seg_ins, 1, length) != SQLITE_OK) {
			std::cerr << "\n     x Error binding ID to query ";
			continue;
		} else {
			std::clog << "\n     + ID bound to query ";
		}
		auto res = sqlite3_step (stmt_seg_ins);
		if (res != SQLITE_ROW && res != SQLITE_DONE) {
			std::cerr << "\n     x Error running INSERT query ";
			continue;
		} else {
			std::clog << "\n     + Segment Inserted ";
		}
		sqlite3_reset (stmt_seg_ins);

		// RETURN ID of new segment
		auto segment_id = sqlite3_last_insert_rowid (db);
		std::clog << " -> segment_id: " << segment_id;

	    // INSERT INTO shapes (shape_id, leg, segment_id, shape_dist_traveled)
	    //      VALUES (?, 0, ?, 0) - [shape_id, segment_id]
	    if (sqlite3_bind_text (stmt_shape_ins, 1, sid, -1, SQLITE_STATIC) != SQLITE_OK ||
			sqlite3_bind_int64 (stmt_shape_ins, 2, segment_id) != SQLITE_OK) {
			std::cerr << "   Error binding shape ";
			continue;
		} else {
			std::clog << "   Shape bound OK ";
		}
		if (sqlite3_step (stmt_shape_ins) != SQLITE_DONE) {
			std::cerr << "    Error inserting shape ";
			continue;
		} else {
			std::clog << "    Shape inserted OK";
		}
		sqlite3_reset (stmt_shape_ins);


		// INSERT INTO segment_pt VALUES (?,?,?,?,?) - [segment_id, seg_pt_seq, lat, lng, dist]
		double dist = 0.0;
		sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
		for (unsigned int i=0; i<path.size (); i++) {
			if (i > 0) dist += path[i-1].distanceTo (path[i]);
			if (sqlite3_bind_int64 (stmt_shapept_ins, 1, segment_id) != SQLITE_OK ||
				sqlite3_bind_int (stmt_shapept_ins, 2, i+1) != SQLITE_OK ||
				sqlite3_bind_double (stmt_shapept_ins, 3, path[i].lat) != SQLITE_OK ||
				sqlite3_bind_double (stmt_shapept_ins, 4, path[i].lng) != SQLITE_OK ||
				sqlite3_bind_double (stmt_shapept_ins, 5, dist) != SQLITE_OK) {
				std::cerr << "   Error inserting shape points ";
				sqlite3_exec (db, "ROLLBACK", NULL, NULL, NULL);
				continue;
			}
			if (sqlite3_step (stmt_shapept_ins) != SQLITE_DONE) {
				std::cerr << "   Error running query :( ";
				sqlite3_exec (db, "ROLLBACK", NULL, NULL, NULL);
				continue;
			}
			// OK
			sqlite3_reset (stmt_shapept_ins);
		}
		sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);
	}
	std::clog << "\n   ";

	// Close the statements to prevent memory leak
	sqlite3_finalize (stmt);
	sqlite3_finalize (stmt_get_shape);
	sqlite3_finalize (stmt_seg_ins);
	sqlite3_finalize (stmt_shapept_ins);


	// Delete TMP tables
	sqlite3_exec (db, "DROP TABLE shapes_tmp", NULL, NULL, NULL);
};



/**
 * Import insersections from a JSON file into the database.
 *
 * Additionally, clusters of "points" belonging to the same intersection
 * are combined into a single object.
 *
 * @param db    the database to use
 * @param files the files to load intersections from
 */
void import_intersections (sqlite3* db, std::vector<std::string> files) {
	using json = nlohmann::json;

	// read json file
	std::vector<gps::Coord> ints;
	std::vector<int> types; // 0 = traffic light, 1 = roundabout
	for (auto file: files) {
		std::ifstream f(file.c_str ());
		json j;
		f >> j;

		int type = 0;
		if (file.find ("roundabout") != std::string::npos) type = 1;

		// get coordinates
		for (auto i: j["elements"]) {
			if (!i["lat"].empty () && !i["lon"].empty ()) {
				ints.emplace_back (i["lat"], i["lon"]);
				types.push_back (type);
			}
		}
	}

	// auto Ints = ints;
	// ints.clear();
	// for (int i=0; i<200; i++) ints.push_back (Ints[i]);


	// Compute distance between each 'intersection' point
	double threshold = 40.0;
	std::cout << "\n   + Loaded " << ints.size () << " intersections.\n";
	int N = ints.size ();
	std::vector<std::vector<double> > distmat (N, std::vector<double> (N, 0));
	for (int i=0; i<N; i++) {
		printf("   + Calculating distance matrix ... %*d%%\r", 3, 100 * (i+1) / N);
		std::cout.flush ();
		for (int j=i+1; j<N; j++) {
			distmat[i][j] = ints[i].distanceTo (ints[j]);
			distmat[j][i] = distmat[i][j];
		}
	}
	std::cout << "   + Calculating distance matrix ... done.\n";

	// for (auto& r: distmat) {
	// 	for (auto& c: r) printf(" %*.2f ", 8, c);
	// 	std::cout << "\n";
	// }


	// Kick out rows that are singletons
	std::cout << "   + Finding groups of intersections that could be just one intersection ... ";
	std::cout.flush ();
	std::vector<int> wkeep;
	for (int i=0; i<N; i++) {
		int sum = 0;
		for (int j=0; j<N; j++) sum += (int)(distmat[i][j] < threshold);
		if (sum > 1) wkeep.push_back (i);
	}
	// dmat is a logical matrix of clumped intersections
	N = wkeep.size ();
	std::vector<std::vector<bool> > dmat (N, std::vector<bool> (N, 0));
	for (int i=0; i<N; i++) {
		for (int j=0; j<N; j++) {
			dmat[i][j] = (distmat[wkeep[i]][wkeep[j]] < threshold);
		}
	}
	std::cout << "done.\n";

	// Go through and find individual clusters
	std::cout << "   + Identifying clusters ... \r";
	std::cout.flush ();
	std::vector<std::vector<int> > y;
	for (int i=0; i<N; i++) {
		printf("   + Identifying clusters ... %*d%%\r", 3, (int) (100 * (i+1) / N));
		std::cout.flush ();
		std::vector<int> xi;
		for (int j=0; j<N; j++) {
			if (dmat[i][j]) xi.push_back (j);
		}
		std::vector<int> xj;
		for (unsigned int j=0; j<xi.size (); j++) {
			for (int k=0; k<N; k++) {
				if (dmat[k][xi[j]] && std::find (xj.begin(), xj.end (), k) == xj.end ())
					xj.push_back (k);
			}
		}
		if (xj.size () > 0) {
			for (unsigned int j=0; j<xj.size (); j++) for (int k=0; k<N; k++) dmat[xj[j]][k] = false;
			y.emplace_back (xj);
		}
	}
	std::cout << "   + Identifying clusters ... done.\n";

	// Compute means and unify intersection clusters
	std::cout << "   + Create new intersections in the middle of clusters ... ";
	std::cout.flush ();
	for (unsigned int i=0; i<y.size (); i++) {
		auto newint = gps::Coord(0, 0);
		for (unsigned int j=0; j<y[i].size (); j++) {
			newint.lat += ints[wkeep[y[i][j]]].lat;
			newint.lng += ints[wkeep[y[i][j]]].lng;
		}
		newint.lat = newint.lat / y[i].size ();
		newint.lng = newint.lng / y[i].size ();
		ints[wkeep[y[i][1]]] = newint;
		for (unsigned int j=1; j<y[i].size (); j++) {
			ints[wkeep[y[i][j]]] = gps::Coord(1.0/0.0, 1.0/0.0);
		}
	}
	std::cout << "done.\n";

	std::cout << "   + Inserting intersections into database ... ";
	std::cout.flush ();


	// NOTE: not yet storing the intersection type ...
	sqlite3_stmt* stmt;
	if (sqlite3_prepare_v2 (db, "INSERT INTO intersections (type, lat, lng) VALUES ('traffic_light',?1,?2)",
							-1, &stmt, 0) != SQLITE_OK) {
		std::cerr << "\n   x Error preparing insert query ";
		return;
	}
	sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
	int Nfinal = 0;
	for (auto i: ints) {
		if (i.lat == INFINITY || i.lng == INFINITY) continue;
		if (sqlite3_bind_double (stmt, 1, i.lat) != SQLITE_OK ||
			sqlite3_bind_double (stmt, 2, i.lng) != SQLITE_OK) {
			std::cerr << "   Error binding parameters.\n";
			return;
		}
		if (sqlite3_step (stmt) != SQLITE_DONE) {
			std::cerr << "   Error running insert query.\n";
		}
		sqlite3_reset (stmt);
		Nfinal++;
	}
	sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);
	sqlite3_finalize (stmt);

	std::cout << "done.\n"
		<< "   -> Inserted " << Nfinal << " intersections.\n";
}


/**
 * Segment shapes.
 *
 * One check to make: length before segmentation == length after segmentation!
 *
 * For each segment
 * - find intersections on the segment
 * - split at intersections
 *   - SPLIT = from nearest point on path to A to nearest point on path to B, INCLUSIVE
 * - for each SPLIT, look for existing segment between A and B
 *   - IF exists, use that,
 *   - ELSE create new segment
 * - compare length of original with new:
 *   - IF equal, then replace all rows of SHAPES with new segments
 *     - and delete the original segment
 *   - ELSE report an error creating the segment
 *
 * @param db a database connection
 */
void segment_shapes (sqlite3* db) {
	sqlite3_stmt* stmt_segs;
	if (sqlite3_prepare_v2 (db, "SELECT segment_id, length FROM segments LIMIT 10",
							-1, &stmt_segs, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare SELECT segments";
		return;
	}
	std::clog << "\n + Prepared SELECT segments";

	sqlite3_stmt* stmt_ints;
	if (sqlite3_prepare_v2 (db, "SELECT intersection_id, lat, lng FROM intersections",
							-1, &stmt_ints, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare SELECT intersections";
		return;
	}
	std::clog << "\n + Prepared SELECT intersections ... ";

	struct Int {
		int id;
		gps::Coord pos;
		Int (int id, gps::Coord pos) : id (id), pos (pos) {};
	};
	std::vector<Int> intersections;
	while (sqlite3_step (stmt_ints) == SQLITE_ROW) {
		intersections.emplace_back (sqlite3_column_int (stmt_ints, 0),
									gps::Coord (sqlite3_column_double (stmt_ints, 1),
												sqlite3_column_double (stmt_ints, 2)));
	}
	std::clog << "loaded " << intersections.size () << " intersections";

	sqlite3_stmt* stmt_shape;
	if (sqlite3_prepare_v2 (db, "SELECT lat, lng, seg_dist_traveled FROM segment_pt WHERE segment_id=$1 ORDER BY seg_pt_sequence", -1, &stmt_shape, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare SELECT segment shape points";
		return;
	}
	std::clog << "\n + Prepared SELECT segment shape points";


	// for progress bar, we need # of rows
	sqlite3_stmt* stmt_n;
	int Nseg = 0;
	if (sqlite3_prepare_v2 (db, "SELECT COUNT(segment_id) FROM segments", -1, &stmt_n, 0) == SQLITE_OK &&
		sqlite3_step (stmt_n) == SQLITE_ROW) {
		Nseg = sqlite3_column_int (stmt_n, 0);
	}
	sqlite3_finalize (stmt_n);

	/**
	 * A split object, used only to find intersections at which
	 * to split the route.
	 */
	struct Split {
		int id;
		gps::Coord at;
		Split (int id, gps::Coord at) : id (id), at (at) {};
	};

	// statements to SELECT segment, and insert SEGMENT and SEGMENT_POINTS
	sqlite3_stmt* stmt_segget;
	sqlite3_stmt* stmt_segins;
	sqlite3_stmt* stmt_segID;
	sqlite3_stmt* stmt_ptins;
	if (sqlite3_prepare_v2 (db, "SELECT segment_id FROM segments WHERE start_id=$1 AND end_id=$2",
							-1, &stmt_segget, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare SELECT segment_id query";
		return;
	}
	if (sqlite3_prepare_v2 (db, "INSERT INTO segments (start_id,end_id,length) VALUES ($1,$2,$3)",
							-1, &stmt_segins, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare INSERT segments";
		return;
	}
	if (sqlite3_prepare_v2 (db, "SELECT seq FROM sqlite_sequence WHERE name='segments'",
							-1, &stmt_segID, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare SELECT last ID query";
		return;
	}
	if (sqlite3_prepare_v2 (db, "INSERT INTO segment_pt (segment_id,seg_pt_sequence,lat,lng,seg_dist_traveled) VALUES ($1,$2,$3,$4,$5)", -1, &stmt_ptins, 0) != SQLITE_OK) {
		std::cerr << "\n x Unable to prepare INSERT segment_pt";
		return;
	}

	// Step through each segment and ... segment it further!
	std::cout << "\n * Splitting segments ";
	int Ni = 1;
	while (sqlite3_step (stmt_segs) == SQLITE_ROW) {
		if (Nseg > 0) {
			printf("\r * Splitting segments %*d%%", 3, (int) (100 * Ni / Nseg));
			std::cout.flush ();
			Ni++;
		}

		int segment_id = sqlite3_column_int (stmt_segs, 0);

		// Get the shape points
		if (sqlite3_bind_int (stmt_shape, 1, segment_id) != SQLITE_OK) {
			std::cerr << "\r x Unable to bind query for segment " << segment_id << "\n";
			continue;
		}
		std::vector<gps::Coord> shapepts;
		double latmin = 0, latmax = 0, lngmin = 0, lngmax = 0;
		while (sqlite3_step (stmt_shape) == SQLITE_ROW) {
			shapepts.emplace_back (sqlite3_column_double (stmt_shape, 0),
								   sqlite3_column_double (stmt_shape, 1));
			gps::Coord& s = shapepts.back ();
			if (latmin == 0 || s.lat < latmin) latmin = s.lat;
			if (latmax == 0 || s.lat > latmax) latmax = s.lat;
			if (lngmin == 0 || s.lng < lngmin) lngmin = s.lng;
			if (lngmax == 0 || s.lng > lngmax) lngmax = s.lng;
		}
		sqlite3_reset (stmt_shape);

		// Find nearby intersections
		// - if in box, find distance to nearest point on route
		std::clog << "\n   - Bounding box: ["
			<< latmin << ", " << lngmin << ", "
			<< latmax << ", " << latmax
			<< "] ID = " << segment_id;
		std::vector<Int> ikeep;
		for (auto& ipt: intersections) {
			if (ipt.pos.lat < latmin || ipt.pos.lat > latmax ||
				ipt.pos.lng < lngmin || ipt.pos.lng > lngmax) continue;

			auto np = ipt.pos.nearestPoint (shapepts);

			// Keep the intersections that are close
			if (np.d < 40) ikeep.emplace_back (ipt.id, ipt.pos);
		}
		if (ikeep.size () == 0) continue;

		// Now we simply travel along the route, splitting it whenever
		// come to an intersection.
		std::vector<int> x1, x2; // shape index, intersection index  (< 40m)
		for (unsigned int i=1; i<shapepts.size (); i++) {
			auto& p1 = shapepts[i-1], p2 = shapepts[i];
			if (p1 == p2) continue;
			std::vector<gps::Coord> pseg {p1, p2};
			double closest = 100;
			int cid = -1;
			for (auto& ipt: ikeep) {
				auto np = ipt.pos.nearestPoint (pseg);
				if (np.d < 40 && np.d < closest) {
					closest = np.d;
					cid = ipt.id;
				}
			}
			if (cid >= 0 && closest < 40) {
				x1.push_back (i-1);
				x2.push_back (cid);
			}
		}
		x1.push_back (0); // add a zero so we don't need a special end condition
		x2.push_back (0);
		if (x1.size () != x2.size ()) {
			std::cerr << "Something very, very terrible went wrong: "
				<< x1.size () << " + " << x2.size ();
			return;
		}

		// for (unsigned int i=0; i<x1.size (); i++)
		// 	std::cout << "\n + " << x2[i] << " - " << x1[i];

		std::vector<gps::Coord> path;
		std::vector<Split> splitpoints;
		for (unsigned int i=0; i<x1.size ()-1; i++) {
			// so long as shape index is less than 10 smaller than next index,
			// and segment index is the same as the next one, keep going
			if (x1[i] + 10 > x1[i+1] && x2[i] == x2[i+1]) {
				path.push_back (shapepts[x1[i]]);
			} else {
				// Otherwise, save this intersection
				path.push_back (shapepts[x1[i]]);
				path.push_back (shapepts[x1[i]+1]);
				gps::nearPt np;
				for (auto& ipt: ikeep) {
					// step through intersections until we find the one we're after
					if (ipt.id == x2[i]) {
						// then find the nearest point to it on the shape:
						np = ipt.pos.nearestPoint (path);
						if (np.d < 40) splitpoints.emplace_back (ipt.id, np.pt);
						break;
					}
				}
				path.clear ();
			}
		}
		// KEEP FOR DEBUGGING AND GRAPHS: R/test.R
		// std::cout << "\nIntersection IDs: ";
		// for (auto& sp: splitpoints)
		// 	printf("\n%d,%.6f,%.6f", sp.id, sp.at.lat, sp.at.lng);

		// Now just stream along the path looking for split points, and split!

		std::clog << "\n * locating " << splitpoints.size () << " splits";
		std::vector<int> segment_ids;
		segment_ids.reserve (splitpoints.size () + 1);
		path.clear (); // we can reuse it!
		unsigned int i = 1; // shape index counter
		unsigned int si = segment_ids.size ();
		while (segment_ids.size () < splitpoints.size () + 1 && i < shapepts.size ()) {
			auto& p1 = shapepts[i-1], p2 = shapepts[i];
			path.push_back (shapepts[i-1]);

			if (p1.distanceTo (splitpoints[si].at) +
				splitpoints[si].at.distanceTo (p2) <= p1.distanceTo (p2) + 1 ||
				si == splitpoints.size ()) {
				// finalise segment
				int startid = 0, endid = 0;
				if (si > 0) startid = splitpoints[si-1].id;
				if (si < splitpoints.size ()) endid = splitpoints[si].id;

				// Find or Create new segment
				if (sqlite3_bind_int (stmt_segget, 1, startid) == SQLITE_OK &&
					sqlite3_bind_int (stmt_segget, 2, endid) == SQLITE_OK &&
					sqlite3_step (stmt_segget) == SQLITE_ROW) {
					segment_ids.push_back (sqlite3_column_int (stmt_segget, 0));
				} else {
					// necessary to create a new segment!
					// - add split point to path (IF its not the last)
					if (si < splitpoints.size ()) {
						path.push_back (splitpoints[si].at);
					} else {
						// add remaining shape to path
						while (i < shapepts.size ()) {
							path.push_back (shapepts[i]);
							i++;
						}
					}

					// - create NEW segment in `segments` table
					double pathlen = 0;
					for (unsigned int j=1; j<path.size (); j++)
						pathlen += path[j-1].distanceTo (path[j]);
					if (startid > 0) {
						if (sqlite3_bind_int (stmt_segins, 1, startid) != SQLITE_OK) {
							std::cerr << "\n x Error binding values (startid)";
							return;
						}
					} else {
						if (sqlite3_bind_null (stmt_segins, 1) != SQLITE_OK) {
							std::cerr << "\n x Error binding values (startnull)";
							return;
						}
					}
					if (endid > 0) {
						if (sqlite3_bind_int (stmt_segins, 2, endid) != SQLITE_OK) {
							std::cerr << "\n x Error binding values (endid)";
							return;
						}
					} else {
						if (sqlite3_bind_null (stmt_segins, 2) != SQLITE_OK) {
							std::cerr << "\n x Error binding values (endnull)";
							return;
						}
					}
					if (sqlite3_bind_double (stmt_segins, 3, pathlen) != SQLITE_OK) {
						std::cerr << "\n x Error binding values (length)";
						return;
					}
					if (sqlite3_step (stmt_segins) != SQLITE_DONE) {
						std::cerr << "\n x Error running INSERT new segment query";
						return;
					}
					sqlite3_reset (stmt_segins);

					if (sqlite3_step (stmt_segID) != SQLITE_ROW) {
						std::cerr << "\n x Error fetching inserted ID";
						return;
					}
					// - get ID of new segment
					int sid = sqlite3_column_int (stmt_segID, 0);
					segment_ids.push_back (sid);
					sqlite3_reset (stmt_segID);

					// - insert path into `segment_pt` table
					sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
					double pathdist = 0;
					for (unsigned int j=0; j<path.size (); j++) {
						if (sqlite3_bind_int (stmt_ptins, 1, sid) != SQLITE_OK) {
							std::cerr << "\n x Error binding segment ID";
							return;
						}
						if (sqlite3_bind_int (stmt_ptins, 2, j+1) != SQLITE_OK) {
							std::cerr << "\n x Error binding segment pt sequence";
							return;
						}
						if (sqlite3_bind_double (stmt_ptins, 3, path[j].lat) != SQLITE_OK ||
							sqlite3_bind_double (stmt_ptins, 4, path[j].lng) != SQLITE_OK) {
							std::cerr << "\n x Error binding path coordinate";
							return;
						}
						if (sqlite3_bind_double (stmt_ptins, 5, pathdist) != SQLITE_OK) {
							std::cerr << "\n x Error binding seg dist traveled";
							return;
						}
						if (sqlite3_step (stmt_ptins) != SQLITE_DONE) {
							std::cerr << "\n x Error executing insertion of point query";
							return;
						}
						sqlite3_reset (stmt_ptins);
						if (j + 1 < path.size ()) pathdist += path[j].distanceTo (path[j+1]);
					}
					sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);
				}
				sqlite3_reset (stmt_segget);

				// - clear path and start it at splitpoint
				path.clear ();
				path.push_back (splitpoints[si].at);

				std::clog << "\n   + Sement " << si + 1 << ": from ";
				if (si > 0) std::clog << startid;
				else std::clog << "start";
				std::clog << " to ";
				if (si < splitpoints.size ()) std::clog << endid;
				else std::clog << "end";

				si = segment_ids.size ();
				if (si == splitpoints.size () + 1) break;
			}
			i++;
		}

		if (segment_ids.size () == splitpoints.size () + 1) {
			std::clog << "\n * created " << segment_ids.size () << " segments: \n  > ";
			for (auto sid: segment_ids) std::clog << sid << ", ";
		} else {
			std::cerr << "\n x created the wrong number of segments (" << segment_ids.size () << ")";
		}

	}  // END while (step)
	std::cout << " - done!\n";

	sqlite3_finalize (stmt_segget);
	sqlite3_finalize (stmt_segins);
	sqlite3_finalize (stmt_segID);
	sqlite3_finalize (stmt_ptins);
	sqlite3_finalize (stmt_shape);
	sqlite3_finalize (stmt_segs);
};

/**
 * Compute the shape distance traveled for each stop along a route.
 * @param dbname The database to connect to
 */
void calculate_stop_distances (std::string& dbname) {
	std::string v = "54.27";
	gtfs::GTFS gtfs (dbname, v);
	std::cout << " * GTFS database loaded     ";


	sqlite3* db;
	if (sqlite3_open (dbname.c_str (), &db)) {
		fprintf(stderr, " * Can't open database: %s\n", sqlite3_errmsg(db));
		return;
	}
	std::cout << " \n* Connected to database ";

	sqlite3_stmt* stmt_upd;
	if (sqlite3_prepare_v2 (db, "UPDATE stop_times SET shape_dist_traveled=$1 WHERE trip_id IN (SELECT trip_id FROM trips WHERE route_id=$2) AND stop_id=$3",
		  					-1, &stmt_upd, 0) != SQLITE_OK) {
		std::cerr << " x Unable to prepare UPDATE query :(";
		return;
	}

	auto Nr = gtfs.get_routes ().size ();
	unsigned int Ni = 0;
	std::cout << "\n* Computing stop distances into trip "
	<< "(" << Nr << " routes)\n";
	// For each route
	for (auto& r: gtfs.get_routes ()) {
		Ni++;
		printf("  %*d%%\r", 3, (int)(100 * Ni / Nr));
		std::cout.flush ();
		std::shared_ptr<gtfs::Route> route = std::get<1> (r);
		auto shape = route->get_shape ();
		if (!shape) continue; // no shape associated with this route

		auto stops = route->get_stops ();
		if (stops.size () == 0) continue; // No stops - skip!

		auto segments = shape->get_segments ();
		if (segments.size () == 0) continue;

		// Travel along the route until the next stop is between STOP and STOP+1
		unsigned int si = 0;
		for (auto& s: segments) {
			auto seg = s.segment;
			if (!seg) break;
			auto path = seg->get_path (); // sequence of ShapePt structs
			if (path.size () == 0) break;
			double dmin (s.shape_dist_traveled);

			// Check the first point in the segment (likely only for first stop)
			if (stops[si].stop->get_pos ().distanceTo (path[0].pt) < 1) {
				stops[si].shape_dist_traveled = dmin + path[0].seg_dist_traveled;
				// printf(" - Stop %*d [%*s]: %*d m\n",
				// 	   3, si+1,
				// 	   6, stops[si].stop->get_id ().c_str (),
				// 	   6, (int)stops[si].shape_dist_traveled);
				si ++;
				if (si == stops.size ()) break;
			}
			// step through path
			for (unsigned int i=1; i<path.size (); i++) {
				// ---Consider stop is AT the point if distance < 1m (rounding error!)
				// FIRST check stop I
				if (stops[si].stop->get_pos ().distanceTo (path[i].pt) < 1) {
					stops[si].shape_dist_traveled = dmin + path[i].seg_dist_traveled;
					// printf(" - Stop %*d [%*s]: %*d m\n",
					// 	   3, si+1,
					// 	   6, stops[si].stop->get_id ().c_str (),
					// 	   6, (int)stops[si].shape_dist_traveled);
					si ++;
					if (si == stops.size ()) break;

					continue; // it wont be BEHIND this point!
				}

				// THEN, see if stop lies between stop i-1 and stop i
				// actually, since AT joins route shapes to the stop,
				// we don't need to do this bit
			}
		}

		if (si != stops.size ()) break; // unable to get distance for all stops

		// Now save the distances into the database for each trip
		sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
		for (auto& s: stops) {
			if (sqlite3_bind_double (stmt_upd, 1, s.shape_dist_traveled) != SQLITE_OK ||
				sqlite3_bind_text (stmt_upd, 2, route->get_id ().c_str (), -1, SQLITE_STATIC) != SQLITE_OK ||
				sqlite3_bind_text (stmt_upd, 3, s.stop->get_id ().c_str (), -1, SQLITE_STATIC) != SQLITE_OK) {
				std::cerr << "\n x Unable to bind parameters.";
				return;
			}
			if (sqlite3_step (stmt_upd) != SQLITE_DONE) {
				std::cerr << "\n x Unable to execute query: " << sqlite3_errmsg (db);
				return;
			}
			sqlite3_reset (stmt_upd);

			// std::cout << "(" << s.shape_dist_traveled << ","
			// 	<< route->get_id () << "," << s.stop->get_id () << "), ";
			// std::cout.flush ();
		}
		sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);

	}
	sqlite3_finalize (stmt_upd);
	sqlite3_close (db);

	// for (auto& s: gtfs.get_stops ()) {
	// 	std::shared_ptr<gtfs::Stop> stop = std::get<1> (s);
	// 	std::cout << "Stop " << stop->get_id () << ": " << stop->get_pos () << "\n";
	// };
};
