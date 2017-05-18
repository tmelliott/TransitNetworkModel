#include <cxxtest/TestSuite.h>
#include <time.h>

#include <sampling.h>

#define isnan std::isnan

class RNGTests : public CxxTest::TestSuite {
public:
	sampling::RNG rng;

	void testUniform(void) {
		rng.set_seed (time(NULL));
		double u = rng.runif ();
		// double u2 = rng.runif(50.0, 100.0);
		TS_ASSERT(u > 0 && u < 1);
		// TS_ASSERT(u2 > 50.0 && u2 < 100);
	};

	void testNormal(void) {
		rng.set_seed (time(NULL) + 1);
		TS_ASSERT_THROWS_NOTHING (rng.rnorm ());
		// TS_ASSERT_IS_NAN (rng.rnorm(0, -1));
	};

	void testSeed(void) {
		rng.set_seed(10);
		double u1 = rng.runif ();
		rng.set_seed(10);
		double u2 = rng.runif ();
		double u3 = rng.runif ();

		TS_ASSERT_EQUALS(u1, u2);
		TS_ASSERT_DIFFERS(u1, u3);
	};
};
