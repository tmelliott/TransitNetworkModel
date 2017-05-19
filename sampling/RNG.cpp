#include <random>
#include <stdexcept>

#include <sampling.h>

namespace sampling {

	/**
	 * Default constructor.
	 *
	 * Initialises the generator with a standard uniform and
	 * standard normal random number generator.
	 */
	RNG::RNG () : uniform (0.0, 1.0), normal (0.0, 1.0) {};

	/**
	 * Default constructor with seed.
	 *
	 * Same as default constructor, but also sets the seed.
	 */
	RNG::RNG (unsigned int seed) : RNG::RNG () {
		set_seed (seed);
	};


	// METHODS

	/**
	 * Set the RNG's seed.
	 * @param seed the seed to use
	 */
	void RNG::set_seed (unsigned int seed) {
		gen.seed (seed);
	};


	// DISTRIBUTIONS

	double RNG::runif (void) {
		return uniform (gen);
	};

	double RNG::rnorm (void) {
		return normal (gen);
	};




}; // end namespace sampling
