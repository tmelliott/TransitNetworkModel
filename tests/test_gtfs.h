#include <cxxtest/TestSuite.h>

#include <math.h>
#include <vector>
#include <memory>
#include "gtfs.h"

class VehicleTests : public CxxTest::TestSuite {
public:
	gtfs::Vehicle v = gtfs::Vehicle("testbus", 1);

	void testVehicle (void) {
		gtfs::Vehicle& vr = v;
		TS_ASSERT_EQUALS(vr.get_id (), "testbus");
	};
	void testParticles (void) {
		gtfs::Vehicle& vr = v;
		TS_ASSERT_EQUALS(vr.get_particles ().size(), 1);
		TS_ASSERT_EQUALS(vr.get_particles ()[0].get_id (), 1);
	};
	void testResample (void) {
		gtfs::Vehicle& vr = v;
		// do resample
	};
};
