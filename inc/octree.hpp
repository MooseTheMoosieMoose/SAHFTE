
#include <variant>
#include <array>
#include <cstdint>
#include <vector>

#include "common.hpp"
#include "chunk_allocator.hpp"

namespace FusionSystem {

template <typename T>
class Octree {
private:
    struct Internal;
public:
    struct Leaf {
        Internal* parent_octant;
        Vec3D actual_point;
        T element;
    };
private:
    struct Internal {
        Internal* parent_octant;
        Vec3D bounding_dimensions;
        Vec3D center_point;
        std::array<std::variant<Internal*, std::vector<Leaf*>>, 8> children;
    };

//Declare the actual class
public:
    Octree (const Vec3D& bounding_dimensions, size_t depth_max) : max_depth(depth_max) {
        root_dimensions = bounding_dimensions;

        tree_root.bounding_dimensions = root_dimensions;
        tree_root.parent_octant = nullptr;
        tree_root.center_point = Vec3D{0, 0, 0};

    }

    constexpr void add_element(T&& item, const Vec3D& pos);

private:
    //The dimensions of the root bounding box to be partitioned, anything outside this box is discarded
    Vec3D root_dimensions;

    //The stopping condition for node storage
    const size_t max_depth;

    //The root of the internal tree
    Internal tree_root;

    //The reserved memory block where internal nodes and leaf nodes are built, kept this way 
    //over built-in unique_ptrs to allow for memory reuse across iterations
    ChunkAllocator<Internal> internal_nodes;
    ChunkAllocator<Leaf> leaf_nodes;

    
};

} //End namespace FusionSystem