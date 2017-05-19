#include <math.h>
#include <random>
#include <stdexcept>

#include <sampling.h>

namespace sampling {

	/**
	 * Default constructor for a standard uniform, U(0,1)
	 */
	uniform::uniform () : a (0), b (1) {};

	/**
	 * General constructor for a uniform U(a,b)
	 */
	uniform::uniform (double a, double b) : a (a), b (b) {
		if (a >= b) {
			throw std::invalid_argument ("a must be less than b");
		}
	};

	/**
	 * Computes the PDF for a given value of x
	 * @param  x the x value to evaluate
	 * @return   the probability density function at x
	 */
	double uniform::pdf (double x) {
		return (x < a || x > b) ? 0 : 1 / (b - a);
	};

	/**
	 * Returns the log PDF for a given x
	 * @param  x   x value to evaluate
	 * @return     log probability density at x
	 */
	double uniform::pdf_log (double x) {
		return std::log (uniform::pdf (x));
	};

	/**
	 * Sample a random observation from the given uniform distribution
	 * @param  rng reference to a RNG
	 * @return     a random value
	 */
	double uniform::rand (sampling::RNG &rng) {
		return rng.runif () * b + a;
	};

}; // end namespace sampling
