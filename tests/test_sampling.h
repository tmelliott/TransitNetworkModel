#include <cxxtest/TestSuite.h>
#include <time.h>

#include <sampling.h>

#define isnan std::isnan

class RNGTests : public CxxTest::TestSuite {
public:
	sampling::RNG rng;

	void testSeed(void) {
		rng.set_seed(10);
		double u1 = rng.runif ();
		rng.set_seed(10);
		double u2 = rng.runif ();
		double u3 = rng.runif ();

		TS_ASSERT_EQUALS(u1, u2);
		TS_ASSERT_DIFFERS(u1, u3);
	};

	void testUniform(void) {
		rng.set_seed (time(NULL));

		TS_ASSERT_THROWS_NOTHING (sampling::uniform unif);
		TS_ASSERT_THROWS_NOTHING (sampling::uniform unif (-100, 100));
		TS_ASSERT_THROWS (sampling::uniform unif (1, 1), std::invalid_argument);
		TS_ASSERT_THROWS (sampling::uniform unif (1, 0), std::invalid_argument);

		sampling::uniform unif1;
		sampling::uniform unif2 (-100, 100);
		TS_ASSERT_THROWS_NOTHING (unif1.rand (rng));
		TS_ASSERT_THROWS_NOTHING (unif2.rand (rng));

		double u1 = unif1.rand (rng);
		double u2 = unif2.rand (rng);
		TS_ASSERT (u1 > 0 && u1 < 1);
		TS_ASSERT (u2 > -100 && u2 < 100);

		TS_ASSERT_EQUALS (unif1.pdf (1), 1);
		TS_ASSERT_EQUALS (unif2.pdf (200), 0);
		TS_ASSERT_EQUALS (unif1.pdf_log (0.2), 0);
		TS_ASSERT_EQUALS (unif2.pdf_log (0), std::log (1.0/200));
		TS_ASSERT_EQUALS (unif1.pdf_log (2), - INFINITY);
	};

	void testNormal(void) {
		rng.set_seed (time(NULL) + 1);

		TS_ASSERT_THROWS_NOTHING (sampling::normal norm);
		TS_ASSERT_THROWS_NOTHING (sampling::normal norm (5, 10));
		TS_ASSERT_THROWS(sampling::normal norm (1, -1), std::invalid_argument);

		sampling::normal norm1;
		sampling::normal norm2 (5, 10);
		TS_ASSERT_THROWS_NOTHING (norm1.rand (rng));
		TS_ASSERT_THROWS_NOTHING (norm2.rand (rng));

		TS_ASSERT_EQUALS (round(norm1.pdf (1) * 1e6), 241971)
	};

	void testExponential(void) {
		rng.set_seed (time(NULL) + 2);

		// TS_ASSERT_THROWS_NOTHING (sampling::exponential ex);
		// TS_ASSERT_THROWS_NOTHING (sampling::exponential exp (0.1));
		// TS_ASSERT_THROWS (sampling::exponential exp (-1.0), std::invalid_argument);

		// sampling::exponential exp1;
		// sampling::exponential exp2 (0.1); // mean = 1/0.1 = 10
		// TS_ASSERT_THROWS_NOTHING (exp1.rand (rng));
		// TS_ASSERT_THROWS_NOTHING (exp2.rand (rng));
		//
		// TS_ASSERT_EQUALS (round(exp1.pdf (1.0) * 1e6), 367879);
		// TS_ASSERT_EQUALS (round(exp2.pdf_log (1.0) * 1e6), -2402585);

	};

};
