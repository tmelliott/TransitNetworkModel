#ifndef GTFS_PARTICLE_H
#define GTFS_PARTICLE_H value

/**
* Particle constructor.
*
* The ID is automatically selected from the parent vehicle.
*/
gtfs::Particle::Particle (gtfs::Vehicle& v) {
	unsigned long next = v.next_id_++;
	particle_id_ = next;
	std::cout << " + Created particle for vehicle " << v.vehicle_id ()
		<< " with id = " << particle_id_ << std::endl;
};

/**
* Desctructor for a particle.
*/
gtfs::Particle::~Particle() {
	std::cout << " - Particle " << particle_id_ << " deleted." << std::endl;
};

const unsigned long gtfs::Particle::particle_id () const {
	return particle_id_;
};

#endif
