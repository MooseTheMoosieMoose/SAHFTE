#pragma once

#include <cmath>
#include <numbers>
#include <cstdint>

#include "common.hpp"

namespace FusionSystem {

/**
 * @brief takes in a position, `origin`, and a position `target` and produces a `Vec3D` where the components
 * represent signed distances in meters from `target` to `origin`
 */
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

/**
 * @brief takes in a global position and converts them to a position along a Z-order curve in 3d space
 * @note this works by converting from global positions, assuming that distances are short enough that we
 * can treat the coordinate system as a flat plane, and creating a cube space where the center is at
 * `2 ^ bit_depth / 2` on all axis
 */
constexpr uint64_t geo_to_z_order(
            const Vec3D& target, const Vec3D& origin, const Vec3D& z_volume, uint8_t bit_depth
) {
    //Get the relative distance in meters
    auto relative_pos = geo_to_local(origin, target);

    //Conver to corner aligned grid
    //...


}

};