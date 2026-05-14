
#include "fusion_system.hpp"
#include <iostream>

using namespace FusionSystem;

int main() {
    //Declare the project level consts
    const Vec3D bounding_volume = Vec3D{100.0, 100.0, 100.0};
    const Vec3D starting_origin = Vec3D{0.0, 0.0, 0.0};
    const double starting_heading = 0;
    const std::size_t aux_threads = 3;
    const uint8_t spatial_depth = 8;

    //The class and modality maps can be used to reference the indicies of certain classes & modalities
    std::vector<std::string> class_map = {"pedestrian", "vehicle", "traffic_cone"};
    std::vector<std::string> mod_map = {"camera", "radar", "lidar"};

    //Usage starts with creating a fuser
    auto fuser = Fuser(aux_threads, spatial_depth, bounding_volume, starting_origin, starting_heading);

    //Next we can add inferences for two sets of objects that collide
    fuser.add_inference(Vec3D{10.0, 0.0, 0.0}, Vec3D{2.0, 1.0, 1.0}, 0.0, 0, 1, 0.5, "obj1", false);
    fuser.add_inference(Vec3D{10.5, 0.0, 0.0}, Vec3D{1.0, 1.0, 1.0}, 0.0, 1, 1, 0.5, "obj2", false);

    fuser.add_inference(Vec3D{0.0, 10.0, 0.0}, Vec3D{1.0, 1.0, 1.0}, 0.0, 0, 2, 0.5, "obj3", false);
    fuser.add_inference(Vec3D{0.0, 10.0, 0.0}, Vec3D{1.0, 1.0, 1.0}, 0.0, 1, 2, 0.5, "obj4", false);

    std::cout << "Fusing..." << std::endl;
    fuser.fuse(3);

    //Check status, if not ok log it
    if (!fuser.is_ok()) {
        std::cout << "Problem: " << fuser.get_error() << std::endl;
    } else {
        auto& output = fuser.get_output();
        for (const auto& item : output) {
            std::cout << "Class: " << item.class_name << " UUID: " << item.uuid << std::endl; 
        }
    }
}