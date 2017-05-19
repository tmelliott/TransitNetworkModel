#include <math.h>
#include <random>
#include <stdexcept>

#include <sampling.h>

namespace sampling {

	/**
	 * Default constructor for a standard normal, N(0,1)
	 */
	normal::normal () : mu (0), sigma (1) {};

	/**
	 * General constructor for a normal N(mu, sigma^2)
	 */
	normal::normal (double mu, double sigma) : mu (mu), sigma (sigma) {
		if (sigma <= 0) {
			throw std::invalid_argument ("sigma must be positive");
		}
	};

	/**
	 * Computes the PDF for a given value of x
	 * @param  x the x value to evaluate
	 * @return   the probability density function at x
	 */
	double normal::pdf (double x) {
		return std::exp (normal::pdf_log (x));
	};

	/**
	 * Returns the log PDF for a given x
	 * @param  x   x value to evaluate
	 * @return     log probability density at x
	 */
	double normal::pdf_log (double x) {
		return - 0.5 * std::log (2 * M_PI) - std::log (sigma) -
			pow(x - mu, 2) / (2 * pow(sigma, 2));
	};

	/**
	 * Sample a random observation from the given normal distribution
	 * @param  rng reference to a RNG
	 * @return     a random value
	 */
	double normal::rand (sampling::RNG &rng) {
		return rng.rnorm () * sigma + mu;
	};

}; // end namespace sampling
