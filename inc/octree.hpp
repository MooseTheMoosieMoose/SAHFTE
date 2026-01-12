
#include <variant>
#include <array>
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

#include "common.hpp"
#include "chunk_allocator.hpp"

//REMOVE AFTER TESTING
//TODO
#include <iostream>

namespace FusionSystem {

template <typename T>
class Octree {
private:
    struct Internal;
public:
    struct Leaf {
        Internal* parent_octant = nullptr;
        Vec3D bounding_dimensions {};
        Vec3D actual_point {};
        T element {};
    };
private:
    struct Internal {
        Internal* parent_octant = nullptr;
        size_t depth = 0;
        Vec3D center_point {};
        std::array<std::variant<Internal*, Leaf*>, 8> children {};
    };

//Declare the actual class
public:
    Octree(const Vec3D& bounding_dimensions, size_t depth_max) : max_depth(depth_max) {

        //Create the root node
        tree_root.depth = 0;
        tree_root.parent_octant = nullptr;
        tree_root.center_point = Vec3D{0, 0, 0};
        tree_root.children = {nullptr};

        //Pre-calculate the bounding dimensions for this space
        sub_dimensions.push_back(bounding_dimensions);
        for (int i = 1; i < max_depth + 1; i++) {
            sub_dimensions.push_back(Vec3D{
                .x = sub_dimensions[i - 1].x / 2,
                .y = sub_dimensions[i - 1].y / 2,
                .z = sub_dimensions[i - 1].z / 2
            });
        }
        

    }

    /**
     * @brief adds an element to the space
     */
    constexpr bool add_element(T&& item, const Vec3D& pos, const Vec3D& dim) noexcept {
        //See if element exists in master space
        if (!point_in_space(pos, tree_root)) {
            return false;
        }

        //Walk down the tree and insert the element
        Internal* head = &tree_root;
        size_t depth_counter = 0;
        while (depth_counter > max_depth) {
            //Get the points relative octant
            uint8_t octant = get_space_octant(pos, head);
            
            //Check to see if the next node is initilized yet
            if (head->children[octant] == nullptr) {
                Internal* new_internal = internal_nodes.allocate();
                //Get new center point of octant
                
            }
        }

        return true;
    }

    constexpr std::vector<T> extract_and_reset() {

    }

private:
    //The stopping condition for node storage
    const size_t max_depth;

    //The root of the internal tree
    Internal tree_root = {};

    //The reserved memory block where internal nodes and leaf nodes are built, kept this way 
    //over built-in unique_ptrs to allow for memory reuse across iterations
    ChunkAllocator<Internal> internal_nodes;
    ChunkAllocator<Leaf> leaf_nodes;

    //The pre-calculated sizes of all the sub divisions of the bounding space
    //up to max depth to save on memory and calculations
    std::vector<Vec3D> sub_dimensions;

    /**
     * @brief somewhat efficiently determines if a point lies within a space, used for intersection and detection culling
     */
    constexpr bool point_in_space(const Vec3D& point, const Internal& space) noexcept {
        const double x_offset = sub_dimensions[space.depth].x / 2;
        if (std::abs(point.x - space.center_point.x) >= x_offset) {
            return false;
        }

        const double y_offset = sub_dimensions[space.depth].y / 2;
        if (std::abs(point.y - space.center_point.y) >= y_offset) {
            return false;
        }

        const double z_offset = sub_dimensions[space.depth].z / 2;
        if (std::abs(point.z - space.center_point.z) >= z_offset) {
            return false;
        }

        return true;
    }

    /**
     * @brief given some space and a given point, determins which octant within that space it should lie in
     * by dividing space evenly along all 3 axis, i.e is it left or right? (first bit), is it forwards or back? (second bit)
     * is it up or down? 3rd bit, to create a code mapping. This corresponds to a 3d Z-Order curve or a Morton Code
     * @note octant selection is determined purely based on the center point of the space, no bounds checking occurs
     * @note refer in the docs for the reference unit cube to work out the bit twiddling that makes this work
     * @note https://en.wikipedia.org/wiki/Z-order_curve for coding convention orders
     */
    constexpr uint8_t get_space_octant(const Vec3D& point, const Internal& space) noexcept {
        return static_cast<uint8_t>(
            (point.x >= space.center_point.x) | 
            ((point.y >= space.center_point.y) << 1) | 
            ((point.z >= space.center_point.z) << 2)
        );
    }

    constexpr Vec3D get_octant_center (const Vec3D& point, const Internal& space, uint8_t octant) noexcept {
        //Based on the octant bitflags create a multiplier by either 1 or -1 to go left or right of current center
        double x_toggle = (octant & 1) ? 1 : -1;
        double y_toggle = ((octant >> 1) & 1) ? 1 : -1;
        double z_toggle = ((octant >> 2) & 1) ? 1 : -1;

        //The offset is the dimensions of the next depth cube, toggled along the dividing axis
        Vec3D offset = Vec3D{
            .x = (sub_dimensions[head->depth + 1].x) * x_toggle,
            .y = (sub_dimensions[head->depth + 1].y) * y_toggle,
            .z = (sub_dimensions[head->depth + 1].z) * z_toggle
        };

        return point + offset;
    }
    
};

} //End namespace FusionSystem