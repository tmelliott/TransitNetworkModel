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

		TS_ASSERT_THROWS_NOTHING (sampling::normal norm);
		TS_ASSERT_THROWS_NOTHING (sampling::normal norm (5, 10));

		sampling::normal norm1;
		sampling::normal norm2 (5, 10);
		TS_ASSERT_THROWS_NOTHING (norm1.rand (rng));
		TS_ASSERT_THROWS_NOTHING (norm2.rand (rng));
		TS_ASSERT_THROWS(sampling::normal norm3 (1, -1), std::invalid_argument);

		TS_ASSERT_EQUALS (round(norm1.pdf (1) * 1e6), 241971)
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
