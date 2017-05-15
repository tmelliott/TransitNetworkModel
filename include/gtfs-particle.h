#ifndef GTFS_PARTICLE_H
#define GTFS_PARTICLE_H value

/**
* Particle constructor.
*
* The ID is automatically selected from the parent vehicle.
*/
gtfs::Particle::Particle (int i) {
	particle_id_ = i;
	std::cout << " + Created particle with id = " << particle_id_ << std::endl;
};

/**
* Copy constructor for a particle.
*
* This will copy all of the properties, EXCEPT particle id.
*/
gtfs::Particle::Particle (const gtfs::Particle &p) {
	std::cout << " >+ Copying particle " << p.particle_id () << std::endl;

	particle_id_ = p.particle_id ();
};

gtfs::Particle::Particle (gtfs::Particle&& p) {
	std::cout << " >> Moving particle " << p.particle_id () << std::endl;
	particle_id_ = p.particle_id ();
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
