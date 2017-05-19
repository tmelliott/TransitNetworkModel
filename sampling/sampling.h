#ifndef SAMPLING_H
#define SAMPLING_H value

#include <random>


namespace sampling {
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

	class normal {
	private:
		double mu, sigma;

	public:
		normal ();
		normal (double mu, double sigma);

		double pdf (double x);
		double pdf_log (double x);

		double rand (sampling::RNG &rng);
	};
};

#endif
