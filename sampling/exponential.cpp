#include <math.h>
#include <random>
#include <stdexcept>

#include <sampling.h>

namespace sampling {

	/**
	 * Default constructor for a standard exponential, N(0,1)
	 */
	exponential::exponential () : lambda (1) {};

	/**
	 * General constructure for a exponential N(mu, sigma^2)
	 */
	exponential::exponential (double lambda) : lambda (lambda) {
		if (lambda <= 0) {
			throw std::invalid_argument ("lambda must be positive");
		}
	};

	/**
	 * Computes the PDF for a given value of x
	 * @param  x the x value to evaluate
	 * @return   the probability density function at x
	 */
	double exponential::pdf (double x) {
		return std::exp (exponential::pdf_log (x));
	};

	/**
	 * Returns the log PDF for a given x
	 * @param  x   x value to evaluate
	 * @return     log probability density at x
	 */
	double exponential::pdf_log (double x) {
		return (x < 0) ? (-1.0/0.0) : std::log (lambda) - lambda * x;
	};

	/**
	 * Sample a random observation from the given exponential distribution
	 * @param  rng reference to a RNG
	 * @return     a random value
	 */
	double exponential::rand (sampling::RNG &rng) {
		return - std::log (rng.runif ()) / lambda;
	};

}; // end namespace sampling
