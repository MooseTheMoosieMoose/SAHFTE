#include <string>
#include <iostream>
#include <vector>

#include "octree.hpp"

struct dummy_obj {
    float foo;
};

int main() {
    auto bounding_dim = FusionSystem::Vec3D{1024, 1024, 1024};
    size_t max_depth = 4;
    FusionSystem::Octree<dummy_obj> test_octree(bounding_dim, max_depth);
    return 0;
}