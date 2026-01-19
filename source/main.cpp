#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <syncstream>

#include "threadpool.hpp"

//#include <json.hpp>

//#include "octree.hpp"
//#include "coord_conv.hpp"

//using json = nlohmann::json;
using namespace FusionSystem;

// struct dummy_obj {
//     float foo;
// };

// int main(int argc, char* argv[]) {
//     const std::string gt_fp = "./test_data/all_objects_ground_truth.json";
//     const std::string camera_fp = "./test_data/camera_sim_results.json";

//     std::ifstream gt_file(gt_fp);
//     if (!gt_file.is_open()) {
//         std::cout << "Failed to open ground truth file at: " << gt_fp << std::endl;
//         return 1;
//     }

//     json gt_data = json::parse(gt_file);
//     Vec3D origin = Vec3D {
//         .x = gt_data["start_pos"][0],
//         .y = gt_data["start_pos"][1],
//         .z = gt_data["start_pos"][2]
//     };

//     std::cout << "Ground truth data puts origin at: " << origin.to_string() << std::endl;

//     std::ifstream camera_file(camera_fp);
//     if (!camera_file.is_open()) {
//         std::cout << "Failed to open camera file!" << std::endl;
//         return 1;
//     }

//     json data = json::parse(camera_file); 
//     std::cout << "Camera data read!" << std::endl;
//     for (const auto& det : data["inferences"]) {
//         Vec3D new_pos = Vec3D {
//             .x = det["latitude"],
//             .y = det["longitude"],
//             .z = det["altitude"]
//         };
        
//         std::cout << "Inference centered around: " << new_pos.to_string() << std::endl;
        
//         Vec3D local_pos = geo_to_local(origin, new_pos);
//         std::cout << "Local position is at: " << local_pos.to_string() << "\n" << std::endl;

//     }

//     return 0;
// }

int main() {
    std::cout << "Threadpool test" << std::endl;
    
    Threadpool tp(4);
    for (int i = 0; i < 100; i++) {
        tp.queue_task([=]() {
            //Waste cycles
            std::this_thread::sleep_for(std::chrono::hours(400));
            std::osyncstream(std::cout) << "This is task #" << i << std::endl;
        });
    }

    tp.attach_to_process_context();

    return 0;
}