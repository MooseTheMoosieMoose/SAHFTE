/**                                                      
 *  ,---.    ,---.  ,--.  ,--.,------.,--------.,------. 
 * '   .-'  /  O  \ |  '--'  ||  .---''--.  .--'|  .---' 
 * `.  `-. |  .-.  ||  .--.  ||  `--,    |  |   |  `--,  
 * .-'    ||  | |  ||  |  |  ||  |`      |  |   |  `---. 
 * `-----' `--' `--'`--'  `--'`--'       `--'   `------'                                                      
 * 
 * SAHFTE (Spatial Algorithmic Hashing Fusion Time-sliced Engine)
 * @file common.hpp
 * @author Moose Abou-Harb
 * @brief this file  contains the headers for common details in the program,
 * including a reusable `Vec3D` component and basic coordinate conversions
 * @copyright `26, Lisenced under whatever Paccar Inc.'s requirements are
 */

#pragma once

#include <string> //Used for the `to_string()` method in Vec3D

#ifdef USE_STD_FORMAT
    #include <format>  //Used for the `to_string()` method in Vec3D
#else
    #include <sstream> // Used as a replacement for std::format for non compliant compilers
#endif

namespace FusionSystem {

/*=====================================================================================================
                                                 Vec3D
=====================================================================================================*/

/**
 * @brief a 3d vector in space used for either a measure of side lengths or of location, translation, scale, etc
 * @note the program is written under the following coordinate system:
 * 
 * -"Right" is in the negative y direction
 * 
 * -"Left" is in the poisitive y direction
 * 
 * -"Forward" is in the positive x direction
 * 
 * -"Backwards" is in the negative x direction
 * 
 * -"Up" is in the positive z direction
 * 
 * -"Down" is in the negative z direction
 */
struct Vec3D {
    double x;
    double y;
    double z;

    /**
     * @brief an overload of the `+` operator for `Vec3D` which allows you to perform element-wise
     * addition with another `Vec3D`
     */
    constexpr Vec3D operator+(const Vec3D& other) const noexcept {
        return Vec3D{
            .x = x + other.x,
            .y = y + other.y,
            .z = z + other.z
        };
    }

    /**
     * @brief an overload of the `-` operator for `Vec3D` which allows you to perform element-wise
     * subtraction with another `Vec3D`
     */
    constexpr Vec3D operator-(const Vec3D& other) const noexcept {
        return Vec3D{
            .x = x - other.x,
            .y = y - other.y,
            .z = z - other.z
        };
    }

    /**
     * @brief an overload of the `/` operator for `Vec3D` which allows you to perform scalar
     * division with the left hand side being a `Vec3D` and the right hand side being a `double`
     */
    constexpr Vec3D operator/(double scalar) const noexcept {
        return Vec3D{
            .x = x / scalar,
            .y = y / scalar,
            .z = z / scalar,
        };
    }

    /**
     * @brief an overload of the `*` operator for `Vec3D` which allows you to perform scalar
     * multiplication with the left hand side being a `Vec3D` and the right hand side being a `double`
     */
    constexpr Vec3D operator*(double scalar) const noexcept {
        return Vec3D{
            .x = x * scalar,
            .y = y * scalar,
            .z = z * scalar,
        };
    }

    inline std::string to_string() {
#ifdef USE_STD_FORMAT
        return std::format("({}, {}, {})", x, y, z);
#else
        std::stringstream s;
        s << "(" << x << ", " << y << ", " << z << ")";
        return s.str();
#endif
    }
};

/*=====================================================================================================
                                           Coord Converter Functions
=====================================================================================================*/

/**
 * @brief takes in a position, `origin`, and a position `target` and produces a `Vec3D` where the components
 * represent signed distances in meters from `target` to `origin`
 * @param origin is the point you want mapped to `(0, 0, 0)`
 * @param target is the point at which you want to find its distance from `origin`
 * @return a `Vec3D` with the distance in meters in each component from the `target` to `origin`
 * @note this does NOT implement the full haversine formula, but instead works assuming that points
 * are close enough that we can treat them as being on a flat plane, see here:
 * https://en.wikipedia.org/wiki/Haversine_formula
 */
Vec3D geo_to_local(const Vec3D& origin, const Vec3D& target);

/**
 * @brief takes in a position `origin` that marks the geographic center of your frame of 
 * reference, (lat, long, alt) and a distance from that origin, `target` in meters
 * and produces the geographic position following thoes distances from the target position
 * @param origin a `Vec3D` with the geographic position to reference from
 * @param target a `Vec3D` with the distance in meters from that reference position
 * @return a `Vec3D` with the geographic (lat, long, alt) equivalent of target
 * @note this does NOT implement the full haversine formula, but instead works assuming that points
 * are close enough that we can treat them as being on a flat plane, see here:
 * https://en.wikipedia.org/wiki/Haversine_formula
 */
Vec3D local_to_geo(const Vec3D& origin, const Vec3D& target);

/**
 * @brief takes in a local position and converts it to a position along a Z-order curve in 3d space
 * @param local_target is the position to convert to its Z-coord
 * @param z_volume the dimensions in meters of the bounding volume of the Z-order curve
 * @param bit_depth is the number of bits for each axis to divide along, a higher number means a finer grain
 * @return an `std::optional` wrapped `uint64_t` which has the Z-order value if the given point is within the
 * `z_volume`, `std::nullopt` otherwise
 * @note this works by converting from local positions, assuming that distances are short enough that we
 * can treat the coordinate system as a flat plane, and creating a cube space where the center is at
 * @note 1 <= bit_depth <= 21, otherwise the passed value will be clipped to this range
 */
std::optional<uint64_t> local_to_z_order(const Vec3D& local_target, const Vec3D& z_volume, uint8_t bit_depth);

/**
 * @brief calculates the euclidian distance between two points as `Vec3D`s with double percision
 * @param a a `Vec3D` which represents the first position to measure between
 * @param b a `Vec3D` which represents the second position to measure between
 * @return double - representing the distance between a and b
 * @note uses the basic formula: `sqrt((b.x - a.x)^2 + (b.y - a.y)^2 + (b.z - a.z)^2)`
 */
double distance_between(const Vec3D& a, const Vec3D& b);

} //End namespace Fusion System