#ifndef GTFS_PARTICLE_H
#define GTFS_PARTICLE_H value

/**
* Particle constructor.
*
* The ID is automatically selected from the parent vehicle.
*/
gtfs::Particle::Particle (gtfs::Vehicle& v) { // : vehicle_ (v) {
	unsigned long next = v.next_id_++;
	particle_id_ = next;
	std::cout << " + Created particle for vehicle " << v.vehicle_id ()
		<< " with id = " << particle_id_ << std::endl;
};

gtfs::Particle::Particle (const gtfs::Particle &p) {
	std::cout << "Copying particle " << p.particle_id () << " -> ";
	// gtfs::Particle pc (new Particle(p.vehicle ()));
	std::cout << p.particle_id () << std::endl;

	// then copy all the values ...

};

/**
* Destructor for a particle.
*/
gtfs::Particle::~Particle() {
	std::cout << " - Particle " << particle_id_ << " deleted." << std::endl;
};


// --- GETTERS

/**
* @return the particle's ID
*/
const unsigned long gtfs::Particle::particle_id () const {
	return particle_id_;
};

#endif
