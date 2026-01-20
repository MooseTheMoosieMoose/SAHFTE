#pragma once

#include <cmath>
#include <numbers>
#include <cstdint>
#include <optional>

#include "common.hpp"

namespace FusionSystem {

/**
 * @brief takes in a position, `origin`, and a position `target` and produces a `Vec3D` where the components
 * represent signed distances in meters from `target` to `origin`
 */
constexpr Vec3D geo_to_local(const Vec3D& origin, const Vec3D& target) {
    //Define useful consts for the calculations
    const double earth_radius = 6378137.0;
    constexpr double deg_to_rad = std::numbers::pi / 180.0;

    //Get the diff in lat, long and alt
    const double d_lat = target.x - origin.x;
    const double d_lon = target.y - origin.y;
    const double d_alt = target.z - origin.z;
    
    //Using the consts and the diff we can get the y
    const double y_meters = d_lat * deg_to_rad * earth_radius;

    //Becuase latitude lines shrink around the pole we need a cos term to fix them
    const double radius_at_lat = earth_radius * std::cos(origin.x * deg_to_rad);
    const double x_meters = d_lon * deg_to_rad * radius_at_lat;

    //Return the finished struct, RVO reduces passing burden I hope
    return Vec3D{x_meters, y_meters, d_alt};
}

/**
 * @brief takes in a global position and converts them to a position along a Z-order curve in 3d space
 * @note this works by converting from global positions, assuming that distances are short enough that we
 * can treat the coordinate system as a flat plane, and creating a cube space where the center is at
 * @note 1 <= bit_depth <= 21, otherwise the passed value will be clipped to this range
 */
constexpr std::optional<uint64_t> geo_to_z_order(
            const Vec3D& target, const Vec3D& origin, const Vec3D& z_volume, uint8_t bit_depth
) {
    //Clip the bit depth
    if (bit_depth == 0) {
        bit_depth = 1;
    } else if (bit_depth > 21) {
        bit_depth = 21;
    }

    //Get the resolution in the volume
    const double cell_scale = (1  << bit_depth);
    double x_res = z_volume.x / cell_scale;
    double y_res = z_volume.y / cell_scale;
    double z_res = z_volume.z / cell_scale;

    //Get the relative position in meters
    auto relative_pos = geo_to_local(origin, target);
    
    //Normalize to the grid and offset so that the origin as the center
    const double half_cell_scale = (1 << (bit_depth - 1));
    double x_grid = (relative_pos.x / x_res) + half_cell_scale;
    double y_grid = (relative_pos.y / y_res) + half_cell_scale;
    double z_grid = (relative_pos.z / z_res) + half_cell_scale;

    //Check for bounds, flooring allows the coords to act as buckets on lines,
    //instead of midpoints which will probably be easier to rationalize, keeping it
    //as a signed int also allows us to think about if its negative for bounds checking
    int64_t x_idx = std::floor(x_grid); 
    int64_t y_idx = std::floor(y_grid);
    int64_t z_idx = std::floor(z_grid);

    //Now that we are clipped to a coordinate, we can check to see if we are within
    //The bounds of the z-order, which goes 0 - (2^bit_depth-1)
    if (x_idx < 0 || x_idx >= cell_scale || y_idx < 0 || y_idx >= cell_scale || 
        z_idx < 0 || z_idx >= cell_scale) {
        return std::nullopt;
    }

    //Aight its inthe bounds, we can then cast it back to an unsigned int for the next
    //operations
    uint64_t x_int = static_cast<uint64_t>(x_idx);
    uint64_t y_int = static_cast<uint64_t>(x_idx);
    uint64_t z_int = static_cast<uint64_t>(x_idx);

    //Interleave into a Z-order coordinate
    uint64_t z_val = 0;
    for (int i = 0; i < bit_depth; i++) {
        uint8_t x_bit = (x_int >> i) & 1;
        uint8_t y_bit = (y_int >> i) & 1;
        uint8_t z_bit = (z_int >> i) & 1;
        uint8_t new_bits = (x_bit) | (y_bit << 1) | (z_bit << 2);
        z_val |= (static_cast<uint64_t>(new_bits) << (i * 3));
    }

    return std::optional(z_val);
}

constexpr double distance_between(const Vec3D& a, const Vec3D& b) {
    return std::sqrt(std::pow((b.x - a.x), 2) + std::pow((b.y - a.y), 2) + std::pow((b.y - a.y), 2));
}

}; //End namespace fusion system