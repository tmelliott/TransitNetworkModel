#include <cxxtest/TestSuite.h>

class GPStest : public CxxTest::TestSuite {
public:
	void testDistance(void) {
		// auto p1 (gps::Coord (-36.866580, 174.757195));
		// auto p2 (gps::Coord (-36.866183, 174.757773));
		// TS_ASSERT(p1.distanceTo(p2) > 60);
	}
};
