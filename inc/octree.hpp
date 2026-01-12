
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
    //Forward declare the internal node type
    struct Internal;
public:
    /**
     * @brief item is a single node in a linked list system used to store the
     * actual elements in the octree, allowing easy memory reuse, and flexability
     * in the number of items in each fine spatial grid
     */
    struct Item {
        T item {};
        Vec3D item_pos {};
        Vec3D bounding_dimensions {};
        Item* next = nullptr;
    };

    /**
     * @brief leafs hold a linked list of items that they store, and are terminal on the tree
     */
    struct Leaf {
        Internal* parent_octant = nullptr;
        Item* item_list = nullptr;
    };

private:
    /**
     * @brief internal nodes are nodes baked into the tree structure that are generated based
     * on the depth of the octree
     */
    struct Internal {
        Internal* parent_octant = nullptr;
        size_t depth = 0;
        Vec3D center_point {};
        std::array<std::variant<Internal*, Leaf*>, 8> children {};
    };

//Declare the actual class
public:
    Octree(const Vec3D& bounding_dimensions, size_t depth_max) : max_depth(depth_max) {

        //Pre-calculate the bounding dimensions for this space
        sub_dimensions.push_back(bounding_dimensions);
        for (int i = 1; i < max_depth + 1; i++) {
            sub_dimensions.push_back(sub_dimensions[i - 1] / 2);
        }
        

    }

    /**
     * @brief adds an element to the space
     */
    constexpr bool add_element(const T& item, const Vec3D& pos, const Vec3D& dim) noexcept {
        //See if element exists in master space
        if (!point_in_space(pos, tree_root)) {
            return false;
        }

        //Walk down the tree and insert the element
        Internal* head = &tree_root;

        //Seek out the insertion point
        while (head->depth > max_depth - 1) {
            //Get the points relative octant
            uint8_t octant = get_space_octant(pos, head);
            
            //Check to see if the next node is initilized yet, if not, create a new object
            if (head->children[octant] == nullptr) {
                Internal* new_internal = internal_nodes.allocate();
                new_internal->center_point = get_octant_center(head, octant);
                new_internal->parent_octant = head;
                new_internal->depth = head->depth + 1;
                head->children[octant] = new_internal;
            }

            //Walk to a finer level of detail
            head = head->children[octant];
        }

        //Head is now set to the parent of the next leaf node, check to see if the next
        //Node exists or not
        uint8_t storage_octant = get_space_octant(pos, head);
        if (head->children[storage_octant] == nullptr) {
            Leaf* new_leaf = leaf_nodes.allocate();
            new_leaf->parent_octant = head;
        }
        

        //Assign leaf
        head->children[storage_octant] = new_leaf;

        return true;
    }

    constexpr std::vector<T> extract_and_reset() {
        //TODO
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

    //To also preserve memory used throughout the program, items are kept in a linear allocator too
    ChunkAllocator<Item> item_nodes;

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

    constexpr Vec3D get_octant_center (const Internal& space, uint8_t octant) noexcept {
        //Based on the octant bitflags create a multiplier by either 1 or -1 to go left or right of current center
        double x_toggle = (octant & 1) ? 1 : -1;
        double y_toggle = ((octant >> 1) & 1) ? 1 : -1;
        double z_toggle = ((octant >> 2) & 1) ? 1 : -1;

        //The offset is the dimensions of the next depth cube, toggled along the dividing axis
        Vec3D offset = Vec3D{
            .x = (sub_dimensions[head->depth].x / 4) * x_toggle,
            .y = (sub_dimensions[head->depth].y / 4) * y_toggle,
            .z = (sub_dimensions[head->depth].z / 4) * z_toggle
        };

        return space.center_point + offset;
    }
    
};

} //End namespace FusionSystem