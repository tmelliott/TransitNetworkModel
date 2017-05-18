#include <cmath>
#include <vector>

#include <gps.h>

namespace gps {
	/**
	 * Constructor for a co-ordinate
	 * @param  Lat the latitude (-90, 90), degrees North/South from the equator
	 * @param  Lng the longitude (-180, 180), degrees East/West from Greenwich, England
	 */
	Coord::Coord(double Lat, double Lng) {
		lat = Lat;
		lng = Lng;
	};

	/**
	 * Compute the Great Circle Distance between to a point.
	 * @param  p the desination point
	 * @return   the distance, in meters, to the point
	 */
  	double Coord::distanceTo(gps::Coord p) {
	    double phi1 = rad(lat);
	    double phi2 = rad(p.lat);
	    double lam1 = rad(lng);
	    double lam2 = rad(p.lng);
	    double Dphi = phi2 - phi1;
	    double Dlam = lam1 - lam2;

	    double a = sin(Dphi / 2) * sin(Dphi / 2) +
	        cos(phi1) * cos(phi2) * sin(Dlam / 2) * sin(Dlam / 2);
	    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
	    double d = R * c;

	    return d;
  	};

	/**
	 * Compute the (initial) bearing to a point.
	 *
	 * This function will compute the initial bearing, in degrees,
	 * when traveling from the current point to the destination point.
	 * Note that, due to curvature of the Earth, the bearing
	 * usually changes along the path (unless its along an axis).
	 *
	 * @param  p The destination point
	 * @return   the bearing, in degrees, to the point
	 */
	double Coord::bearingTo(gps::Coord p) {
		// if (this == p) return 0.0 / 0.0;
	    double phi1 = rad(lat);
	    double phi2 = rad(p.lat);
	    double Dlam = rad(p.lng - lng);

	    double y = sin(Dlam) * cos(phi2);
	    double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(Dlam);
	    double theta = atan2(y, x);

	    return fmod((deg(theta) + 360), 360);
	};

	/**
	 * Compute the destination given initial bearing and distance from a starting point.
	 * @param  d       the distance to be traveled
	 * @param  bearing the initial bearing
	 * @return         the destination point
	 */
  	Coord Coord::destinationPoint(double d, double bearing) {
	    double delta = d / R;
	    double theta = rad(bearing);
	    double phi1 = rad(lat);
	    double lam1 = rad(lng);

	    double sinphi1 = sin(phi1);
	    double cosphi1 = cos(phi1);
	    double sindelta = sin(delta);
	    double cosdelta = cos(delta);
	    double sintheta = sin(theta);
	    double costheta = cos(theta);

	    double sinphi2 = sinphi1 * cosdelta + cosphi1 * sindelta * costheta;
	    double phi2 = asin(sinphi2);
	    double y = sintheta * sindelta * cosphi1;
	    double x = cosdelta - sinphi1 * sinphi2;
	    double lam2 = lam1 + atan2(y, x);

	    return Coord(deg(phi2), fmod(deg(lam2) + 540, 360) - 180);
  	};

	/**
	 * Compute the cross-track distance between the given point,
	 * and a path between two points.
	 *
	 * This is the shortest distance between a given point,
	 * and the path between two points.
	 *
	 * @param  p1 The origin of the path
	 * @param  p2 The desination of the path
	 * @return    The shortest distance between the point and the path.
	 */
  	double Coord::crossTrackDistanceTo(gps::Coord p1, gps::Coord p2) {
	    double d13 = p1.distanceTo(*this) / R;
	    double t13 = rad(p1.bearingTo(*this));
	    double t12 = rad(p1.bearingTo(p2));

	    return asin(sin(d13) * sin(t13 - t12)) * R;
  	};

	/**
	 * Apply the Equirectangular projection to a point using
	 * a supplied point as the origin.
	 *
	 * The point is centered relative to the origin point,
	 * and then the Flat-Earth coordinates (meters north, east)
	 * are computed and returned in a vector.
	 *
	 * @param  origin the point to be used as the center of the projection
	 * @return        a cartesian coordinate of the projected point
	 */
  	std::vector<double> Coord::projectFlat(gps::Coord origin) {
	    double x = (rad(lng) - rad(origin.lng)) * cos(rad(origin.lat));
	    double y = rad(lat) - rad(origin.lat);

	    return std::vector<double> {R * x, R * y};
  	};

	/**
	 * Comparison operator for two coordinates.
	 *
	 * Coordinates are considered equal if the distance between them
	 * is less than 1mm (1/1000 of a meter).
	 * In terms of this project, this is adequate.
	 *
	 * @param p the point being compared to
	 * @return bool whether or not the coordinates are the same or not
	 */
	bool Coord::operator==(const Coord &p) const {
		return gps::Coord(lat, lng).distanceTo(gps::Coord(p.lat, p.lng)) < 0.001;
	};

	/**
	 * Comparison (not equal) operator for two coordinates.
	 *
	 * Coordinates are considered equal if the distance between them
	 * is less than 1mm (1/1000 of a meter).
	 * In terms of this project, this is adequate.
	 *
	 * @param p the point being compared to
	 * @return bool whether or not the coordinates are the same or not
	 */
	bool Coord::operator!=(const Coord &p) const {
		return gps::Coord(lat, lng).distanceTo(gps::Coord(p.lat, p.lng)) >= 0.001;
	};

	/**
	 * Convert degrees to radians
	 *
	 * \f$ x = \frac{\pi}{180} \theta^\circ \f$
	 *
	 * @param  d an angle in degrees
	 * @return   an angle in radians
	 */
	double rad(double d) {
		return d * M_PI / 180;
	};

	/**
	 * Convert radians to degrees
  	 *
 	 * \f$ \theta^\circ = \frac{180}{\pi} x \f$
 	 *
	 * @param  r an angle in radians
	 * @return   an angle in degrees
	 */
  	double deg(double r) {
    	return r * 180 / M_PI;
  	};
}; // namespace gps
