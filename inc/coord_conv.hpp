#pragma once

#include <cmath>
#include <numbers>

#include "common.hpp"

namespace FusionSystem {

constexpr Vec3D geo_to_local(const Vec3D& origin, const Vec3D& target) {
    const double earth_radius = 6378137.0;
    constexpr double deg_to_rad = std::numbers::pi / 180.0;

    const double d_lat = target.x - origin.x;
    const double d_lon = target.y - origin.y;
    const double d_alt = target.z - origin.z;
    const double y_meters = d_lat * deg_to_rad * earth_radius;

    const double radius_at_lat = earth_radius * std::cos(origin.x * deg_to_rad);
    const double x_meters = d_lon * deg_to_rad * radius_at_lat;

    return Vec3D{x_meters, y_meters, d_alt};
}

};