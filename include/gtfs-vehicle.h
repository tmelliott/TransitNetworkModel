#ifndef GTFS_VEHICLE_H
#define GTFS_VEHICLE_H value

/**
* Create a Vehicle object with given ID
* @param vehicle_id the ID of the vehicle as given in the GTFS feed
*/
gtfs::Vehicle::Vehicle (std::string vehicle_id) : vehicle_id_ (vehicle_id) {
	std::cout << " ++ Created vehicle " << vehicle_id_ << std::endl;

	

	int N = 5;
	for (int i=0; i<N; i++) {
		std::shared_ptr<gtfs::Particle> p (new gtfs::Particle(i));
		// particles_.push_back(std::move(p));
		particles_.emplace_back(std::move(p));
	}
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
const std::string gtfs::Vehicle::vehicle_id () const {
	return vehicle_id_;
};

std::vector<std::shared_ptr<gtfs::Particle> >& gtfs::Vehicle::particles () {
	return particles_;
};

void gtfs::Vehicle::resample () {
	// How many of each particle to keep?
	std::vector<int> pkeep ({1, 3, 2, 1, 1});

	// Move old particles
	std::vector<std::shared_ptr<gtfs::Particle> > old_particles = std::move(particles_);

	// Fill out particles again, using move/copy
	for (int& i: pkeep) {
		particles_.push_back(old_particles[i]);
	}
}

#endif
