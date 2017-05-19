#include <random>
#include <vector>

#include "sampling.h"

namespace sampling {

	/**
	 * A non-weighted sampler.
	 *
	 * @param  N   the size of the sample to be sampled from.
	 */
	sample::sample (unsigned int N) : N (N), weighted (false) {};

	/**
	 * A weighted sampler.
	 *
	 * @param  wts weights of the sample
	 */
	sample::sample (const std::vector<double> &wts)
	: N (wts.size ()), weighted (true) {
		weights.reserve (N);
		double ws = 0.0;
		for (unsigned int i=0; i<N; i++) {
			ws += wts[i];
			weights[i] = ws;
		}
	};


	/**
	 * Perform resampling, using the same number of observations
	 * as in the original sample.
	 *
	 * @param  rng a random number generator
	 * @return     a vector of sample indexes
	 */
	std::vector<int> sample::get (RNG &rng) {
		return get (N, rng);
	};

	/**
	 * Perform resampling, taking a sample of size `n`.
	 *
	 * @param n   the size of the new sample
	 * @param rng a random number generator
	 * @return    a vector of sample indexes
	 */
	std::vector<int> sample::get (unsigned int n, RNG &rng) {
		std::vector<int> s;
		s.reserve (n);
		if (weighted) {
			// perform weighted resampling
			for (unsigned int i=0; i<n; i++) {
				int j=0;
				auto u = rng.runif () * weights[N-1];
				while (weights[j] < u) j++;
				s.push_back (j);
			}
		} else {
			// perform non-weighted
			for (unsigned int i=0; i<n; i++) {
				s.push_back (floor(rng.runif () * N));
			}
		}
		return s;
	};
}; // end namespace sampling
