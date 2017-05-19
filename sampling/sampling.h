#ifndef SAMPLING_H
#define SAMPLING_H value

#include <random>
#include <vector>

/**
 * All sampling functionality contained in `samping::`.
 */
namespace sampling {
	/**
	 * Random Number Generator
	 *
	 * Ideally one per program, passed by reference.
	 *
	 */
	class RNG {
	private:
		std::mt19937_64 gen;

		std::uniform_real_distribution<double> uniform;
		std::normal_distribution<double> normal;

	public:
		// Constructors
		RNG ();
		RNG (unsigned int seed);

		// Methods
		void set_seed (unsigned int seed);

		// Distributions
		double runif (void);

		double rnorm (void);
	};

	/**
	 * Class used for Uniform distributions.
	 *
	 * (Log) PDF and random variables
	 */
	class uniform {
	private:
		double a; /*!< lower bound for uniform distibution */
		double b; /*!< upper bound for uniform distribution */

	public:
		uniform ();
		uniform (double a, double b);

		double pdf (double x);
		double pdf_log (double x);

		double rand (sampling::RNG &rng);
	};

	/**
	 * Class used for Normal distributions.
	 *
	 * (Log) PDF and random variables
	 */
	class normal {
	private:
		double mu,     /*!< mean */
		       sigma;  /*!< standard deviation */

	public:
		normal ();
		normal (double mu, double sigma);

		double pdf (double x);
		double pdf_log (double x);

		double rand (sampling::RNG &rng);
	};

	class exponential {
	private:
		double lambda;

	public:
		exponential ();
		exponential (double lambda);

		double pdf (double x);
		double pdf_log (double x);

		double rand (sampling::RNG &rng);
	};

	// vector<int> sample (int N, sampling::RNG &rng);
	// vector<int> sample (int N, bool replace, sampling::RNG &rng);
	// vector<int> sample (int N, vector<double> weights, sampling::RNG &rng);
	// vector<int> sample (int N, vector<double> weights, bool replace);
};

#endif
