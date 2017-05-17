#ifndef GPS_HXX
#define GPS_HXX

#include <cmath>
#include <vector>
namespace gps {
  const double R = 6371000.0;
  
  class Coord {
    public:
      double lat;
      double lng;
      Coord() {};
      Coord(double lat, double lng);

      // Member functions:
      double distanceTo(gps::Coord destination);
      double bearingTo(gps::Coord destination);
      gps::Coord destinationPoint(double distance, double bearing);
      double crossTrackDistanceTo(gps::Coord p1, gps::Coord p2);
      std::vector<double> projectFlat(gps::Coord origin);

      /*inline std::ostream& operator<< (std::ostream& os, const Coord& p) {
	return os << "hi";
	};*/
  };

  inline std::ostream& operator<< (std::ostream& os, const Coord& p) {
    return os << "(" << p.lat << ", " << p.lng << ")";
  };

}; // end namespace gps

#endif
