#pragma once

namespace FusionSystem {

/**
 * @brief a 3d vector in space used for either a measure of side lengths or of location, translation, scale, etc
 * @note the program is written under the following coordinate system:
 * 

 * 
 * -"Right" is in the positive x direction
 * 
 * -"Left" is in the negative x direction
 * 
 * -"Forward" is in the positive y direction
 * 
 * -"Backwards" is in the negative y direction
 * 
 * -"Up" is in the positive z direction
 * 
 * -"Down" is in the negative z direction
 */
struct Vec3D {
    double x;
    double y;
    double z;
};

}