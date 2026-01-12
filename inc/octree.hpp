
#include <variant>
#include <array>
#include <cstdint>
#include <cmath>
#include <vector>

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
        Internal* parent_octant;
        Vec3D actual_point;
        std::vector<T*> element;
    };
private:
    struct Internal {
        Internal* parent_octant;
        size_t depth;
        Vec3D center_point;
        std::array<std::variant<Internal*, Leaf*>, 8> children;
    };

//Declare the actual class
public:
    Octree (const Vec3D& bounding_dimensions, size_t depth_max) : max_depth(depth_max) {

        //Create the root node
        tree_root.depth = 0;
        tree_root.parent_octant = nullptr;
        tree_root.center_point = Vec3D{0, 0, 0};

        //Pre-calculate the bounding dimensions for this space
        sub_dimensions.push_back(bounding_dimensions);
        for (int i = 1; i < max_depth; i++) {
            sub_dimensions.push_back(Vec3D{
                .x = sub_dimensions[i - 1].x / 2,
                .y = sub_dimensions[i - 1].y / 2,
                .z = sub_dimensions[i - 1].z / 2
            });
        }
        

    }

    constexpr bool add_element(T&& item, const Vec3D& pos) noexcept {
        //See if element exists in master space
        if (!point_in_space(pos, tree_root)) {
            return false;
        }

        //Walk down the tree and insert the element
        std::variant<Internal*, Leaf*> head = &tree_root;
        size_t depth_counter = 0;
        while (depth_counter > max_depth) {

        }

        return true;
    }

private:
    //The stopping condition for node storage
    const size_t max_depth;

    //The root of the internal tree
    Internal tree_root;

    //The reserved memory block where internal nodes and leaf nodes are built, kept this way 
    //over built-in unique_ptrs to allow for memory reuse across iterations
    ChunkAllocator<Internal> internal_nodes;
    ChunkAllocator<Leaf> leaf_nodes;

    //The pre-calculated sizes of all the sub divisions of the bounding space
    //up to max depth to save on memory and calculations
    std::vector<Vec3D> sub_dimensions;

    constexpr bool point_in_space(const Vec3D& point, const Internal& space) noexcept {
        const double x_offset = sub_dimensions[space.depth].x 2;
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
};

} //End namespace FusionSystem