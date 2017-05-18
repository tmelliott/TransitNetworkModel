#ifndef GPS_HXX
#define GPS_HXX

#include <iostream>
#include <cmath>
#include <vector>

/**
 * GPS Namespace for co-ordinate manipulation.
 *
 * Most of the formulae were obtained from http://www.movable-type.co.uk/scripts/latlong.html
 */
namespace gps {
	/**
	 * Radius of the earth, in meters
	 */
  	const double R = 6371000.0;

	/**
	 * Coordinate class represents lat/lng points on the Earth.
	 */
  	class Coord {
	public:
      	double lat; /*!< the latitude value (-90,90) (degrees North/South of the equator) */
      	double lng; /*!< the longitude value (-180,180) (degrees East/West of Greenwich) */
      	Coord() {};
      	Coord(double lat, double lng);

		// Member functions:
		double distanceTo(gps::Coord destination);
		double bearingTo(gps::Coord destination);
		gps::Coord destinationPoint(double distance, double bearing);
		double crossTrackDistanceTo(gps::Coord p1, gps::Coord p2);
		std::vector<double> projectFlat(gps::Coord origin);

		bool operator==(const Coord &p) const;
		bool operator!=(const Coord &p) const;
  	};

	double rad(double d);
	double deg(double r);

  	inline std::ostream& operator<< (std::ostream& os, const Coord& p) {
    	return os << "(" << p.lat << ", " << p.lng << ")";
  	};

}; // end namespace gps

#endif
