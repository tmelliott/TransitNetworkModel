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

	struct nearPt;

	/**
	 * Coordinate class represents lat/lng points on the Earth.
	 */
  	class Coord {
	public:
      	double lat = 0.0; /*!< the latitude value (-90,90) (degrees North/South of the equator) */
      	double lng = 0.0; /*!< the longitude value (-180,180) (degrees East/West of Greenwich) */
      	Coord() {};
      	Coord(double lat, double lng);

		// Member functions:
		double distanceTo(gps::Coord destination) const;
		double bearingTo(gps::Coord destination) const;
		gps::Coord destinationPoint(double distance, double bearing) const;
		double crossTrackDistanceTo(gps::Coord p1, gps::Coord p2) const;
		double alongTrackDistance(gps::Coord p1, gps::Coord p2) const;
		std::vector<double> projectFlat(gps::Coord origin) const;
		nearPt nearestPoint (std::vector<gps::Coord>& path);

		bool set (void) const {
			return lat != 0.0 && lng != 0.0;
		}

		bool operator==(const Coord &p) const;
		bool operator!=(const Coord &p) const;
  	};

	double rad(double d);
	double deg(double r);

	/**
	 * Print a coord object.
	 * @param os the ostream to write to
	 * @param p  the point to print
	 * @return   an ostream instance
	 */
  	inline std::ostream& operator<< (std::ostream& os, const Coord& p) {
    	return os << "(" << p.lat << ", " << p.lng << ")";
  	};

	/**
	 * A nearest-point structure.
	 */
	struct nearPt {
		gps::Coord pt; /*!< the nearest point */
		double d; /*!< distance to that point */
		/** default constructor */
		nearPt (gps::Coord p, double d) : pt (p), d (d) {};
		nearPt () {};
	};

}; // end namespace gps

#endif
