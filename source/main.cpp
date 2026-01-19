//Std Library Includes
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <syncstream>
#include <cmath>

//External Library Includes
#include <json.hpp>


//Internal Library Includes
//#include "threadpool.hpp"
//#include "coord_conv.hpp"
#include "chunk_allocator.hpp"

using json = nlohmann::json;
using namespace FusionSystem;

struct example_obj {
    double foo;
    double fooooo;
    std::vector<double> my_double_vec;
};

int main(int argc, char* argv[]) {
    // const std::string gt_fp = "./test_data/all_objects_ground_truth.json";
    // const std::string camera_fp = "./test_data/camera_sim_results.json";

    // std::ifstream gt_file(gt_fp);
    // if (!gt_file.is_open()) {
    //     std::cout << "Failed to open ground truth file at: " << gt_fp << std::endl;
    //     return 1;
    // }

    // json gt_data = json::parse(gt_file);
    // Vec3D origin = Vec3D {
    //     .x = gt_data["start_pos"][0],
    //     .y = gt_data["start_pos"][1],
    //     .z = gt_data["start_pos"][2]
    // };

    // std::cout << "Ground truth data puts origin at: " << origin.to_string() << std::endl;

    // std::ifstream camera_file(camera_fp);
    // if (!camera_file.is_open()) {
    //     std::cout << "Failed to open camera file!" << std::endl;
    //     return 1;
    // }

    // json data = json::parse(camera_file); 
    // std::cout << "Camera data read!" << std::endl;
    // for (const auto& det : data["inferences"]) {
    //     Vec3D new_pos = Vec3D {
    //         .x = det["latitude"],
    //         .y = det["longitude"],
    //         .z = det["altitude"]
    //     };
        
    //     std::cout << "Inference centered around: " << new_pos.to_string() << std::endl;
        
    //     Vec3D local_pos = geo_to_local(origin, new_pos);
    //     std::cout << "Local position is at: " << local_pos.to_string() << std::endl;

    //     auto z_order_pos = geo_to_z_order(new_pos, origin, Vec3D{100.0, 100.0, 100.0}, 16);
    //     if (z_order_pos.has_value()) {
    //         std::cout << "Z-Order position is at: " << z_order_pos.value() << "\n" << std::endl;
    //     } else {
    //         std::cout << "Z-Order position is clipped by master volume!" << "\n" << std::endl;
    //     }

    // }

    ChunkAllocator<example_obj> obj_alloc;

    example_obj* myObj = obj_alloc.allocate();
    myObj->foo = 4;
    std::cout << "Foo is: " << myObj->foo << std::endl;
    obj_alloc.reset();

    return 0;
}