#include <iostream>
#include <cmath>
#include <vector>

#include <gps.h>

namespace gps {
  Coord::Coord(double Lat, double Lng) {
    lat = Lat;
    lng = Lng;
  }

  double rad(double d) {
    return d * M_PI / 180;
  }

  double deg(double r) {
    return r * 180 / M_PI;
  }

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
  }

  double Coord::bearingTo(gps::Coord p) {
    double phi1 = rad(lat);
    double phi2 = rad(p.lat);
    double Dlam = rad(p.lng - lng);

    double y = sin(Dlam) * cos(phi2);
    double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(Dlam);
    double theta = atan2(y, x);

    return fmod((deg(theta) + 360), 360);
  }

  gps::Coord Coord::destinationPoint(double d, double bearing) {
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
  }

  double Coord::crossTrackDistanceTo(gps::Coord p1, gps::Coord p2) {
    double d13 = p1.distanceTo(*this) / R;
    double t13 = rad(p1.bearingTo(*this));
    double t12 = rad(p1.bearingTo(p2));

    return asin(sin(d13) * sin(t13 - t12)) * R;
  }

  std::vector<double> Coord::projectFlat(gps::Coord origin) {
    double x = (rad(lng) - rad(origin.lng)) * cos(rad(origin.lat));
    double y = rad(lat) - rad(origin.lat);

    return std::vector<double> {R * x, R * y};
  }
}; // namespace gps
