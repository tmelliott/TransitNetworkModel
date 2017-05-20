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


	/**
	 * Class used for Exponential distributions.
	 *
	 * (Log) PDF and random variables
	 */
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


	/**
	 * A class for taking a random (weighted) sample.
	 *
	 * All samples are made with replacement.
	 * If constructed with an INT, then equal weights are used;
	 * otherwise weights are passed as a const reference,
	 * otherwise a lot of copying would take place!
	 */
	class sample {
	private:
		int N; /*!< the number of objects in the sample */
		bool weighted;  /*!< whether or not the sample is weighted or not */
		std::vector<double> weights; /*!< sampling weights */

	public:
		sample (int N);
		sample (const std::vector<double> &wts);

		std::vector<int> get (sampling::RNG &rng);
		std::vector<int> get (int n, sampling::RNG &rng);
	};
};

#endif
