#include <cxxtest/TestSuite.h>

#include <math.h>
#include "gps.h"

class GPStest : public CxxTest::TestSuite {
 public:
  gps::Coord p1 = gps::Coord (-36.866580, 174.757195);
  gps::Coord p2 = gps::Coord (-36.866183, 174.757773);
	
  void testDistance(void) {
    TS_ASSERT_EQUALS(round(p1.distanceTo(p2) * 1000), 67769);
    TS_ASSERT_EQUALS(p1.distanceTo(p1), 0);
  }
  
  void testBearings(void) {  
    TS_ASSERT_EQUALS(round(p1.bearingTo(p2) * 1000) / 1000, 49.353);
  }

  void testDestination(void) {
    
  }
};
