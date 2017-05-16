#ifndef GTFS_VEHICLE_H
#define GTFS_VEHICLE_H value

/**
* Create a Vehicle object with given ID
* @param vehicle_id the ID of the vehicle as given in the GTFS feed
*/
gtfs::Vehicle::Vehicle (std::string vehicle_id) :
vehicle_id_ (vehicle_id), n_particles_ (5), next_id_ (0) {
	std::cout << " ++ Created vehicle " << vehicle_id_ << std::endl;

	particles_.reserve(n_particles_);
	for (int i=0; i<n_particles_; i++) {
		particles_.emplace_back(*this);
	}
	std::cout << next_id_ << std::endl;
	// std::cout << " >> now has " << particles_.size () << " particles." << std::endl;
};

/**
* Desctructor for a vehicle object, ensuring all particles are deleted too.
*/
gtfs::Vehicle::~Vehicle() {
	std::cout << " -- Vehicle " << vehicle_id_ << " deleted." << std::endl;
};

/**
* @return ID of vehicle
*/
const std::string& gtfs::Vehicle::vehicle_id () const {
	return vehicle_id_;
};

/**
* @return number of particles that will be used in future resamples
*/
const unsigned int& gtfs::Vehicle::n_particles () const {
	return n_particles_;
}

/**
* @return vector of particles
*/
std::vector<gtfs::Particle>& gtfs::Vehicle::particles () {
	return particles_;
};

unsigned long& gtfs::Vehicle::next_id () {
	return next_id_;
};


// --- SETTERS
/**
* @param n the number of particles that will be generated
*          at the next iteration.
*/
void gtfs::Vehicle::n_particles (unsigned int n) {
	n_particles_ = n;
};


/**
 * Perform weighted resampling with replacement.
 *
 * Use the computed particle weights to resample, with replacement,
 * the particles associated with the vehicle.
 */
void gtfs::Vehicle::resample () {
	// How many of each particle to keep?
	// std::vector<int> pkeep ({1, 3, 2, 1, 1});
	std::vector<int> pn ({0, 3, 1, 0, 1});

	// Move old particles into temporary holding vector
	// std::vector<gtfs::Particle> old_particles = std::move(particles_);

	// for (unsigned int i=0; i<pn.size(); i++) {
	// 	if (pn[i] > 1) {
	// 		for (int j=1; j<pn[i]; j++) {
	// 			std::unique_ptr<gtfs::Particle> pj; // = std::copy(old_particles[i]);
	// 			*pj = std::copy(*old_particles[i]);
	// 		}
	// 	}
	// 	if (pn[i] > 0) {
	//
	// 	}
	// }


	// for (int& i: pkeep) {
	// 	// unique_ptr<gtfs::Particle> p = std::
	// 	std::cout << "Particle " << old_particles[i]->particle_id () << std::endl;
	// }

	// // Fill out particles again, using move/copy
	// for (int& i: pkeep) {
	// 	particles_.push_back(old_particles[i]);
	// }
}

#endif
