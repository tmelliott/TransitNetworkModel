#include <cxxtest/TestSuite.h>

#include <math.h>
#include "gps.h"

#define isnan std::isnan

class CoordTests : public CxxTest::TestSuite {
public:
  	gps::Coord p1 = gps::Coord (-36.866580, 174.757195);
  	gps::Coord p2 = gps::Coord (-36.866183, 174.757773);
	gps::Coord p3 = gps::Coord (-36.866327, 174.757650);

  	void testDistance(void) {
    	TS_ASSERT_EQUALS(round(p1.distanceTo(p2) * 1000), 67769);
    	TS_ASSERT_EQUALS(p1.distanceTo(p1), 0);
		TS_ASSERT_EQUALS(p1.distanceTo(p2), p2.distanceTo(p1));
		TS_ASSERT_EQUALS(round(p1.distanceTo(p3) * 1000), 49293);
  	};

  	void testBearings(void) {
    	TS_ASSERT_EQUALS(round(p1.bearingTo(p2) * 1000) / 1000, 49.353);
		TS_ASSERT_IS_NAN(p1.bearingTo(p1));
  	};

  	void testComparisons(void) {
    	TS_ASSERT_EQUALS(p1, p1);
		TS_ASSERT_DIFFERS(p1, p2); // need to define != operator
  	};

  	void testDestination(void) {
    	auto d = p1.distanceTo(p2);
    	auto b = p1.bearingTo(p2);
    	TS_ASSERT_EQUALS(p1.destinationPoint(d, b), p2);
  	};

	void testCrossTrack(void) {
		TS_ASSERT_EQUALS(round(p3.crossTrackDistanceTo(p1, p2) * 1000), 5021);
		TS_ASSERT_EQUALS(round(p3.alongTrackDistance(p1, p2) * 1000), 49037);
	};
};
