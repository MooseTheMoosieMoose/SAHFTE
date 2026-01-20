//Std Library Includes
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <syncstream>
#include <cmath>
#include <chrono> //Time Profiling

//External Library Includes
#include <json.hpp>


//Internal Library Includes
#include "fusion_system.hpp"

using json = nlohmann::json;
using namespace FusionSystem;


int main(int argc, char* argv[]) {
    //Get the file paths that we will be writing
    const std::string gt_fp = "./test_data/all_objects_ground_truth.json";
    const std::string res_fp_base = "./test_data/";
    std::vector<std::string> mod_names = {"camera", "lidar", "radar"};

    //Open the ground truth file for the origin
    std::ifstream gt_file(gt_fp);
    if (!gt_file.is_open()) {
        std::cout << "Failed to open ground truth file at: " << gt_fp << std::endl;
        return 1;
    }

    //Extract and log the origin
    json gt_data = json::parse(gt_file);
    Vec3D origin = Vec3D {
        .x = gt_data["start_pos"][0],
        .y = gt_data["start_pos"][1],
        .z = gt_data["start_pos"][2]
    };
    std::cout << "Ground truth data puts origin at: " << origin.to_string() << std::endl;

    //Create the instance of our Fuser
    Fuser fuser(4, 8, Vec3D{100, 100, 100}, origin);

    //For each modality, extract their data
    for (const auto& mod_name : mod_names) {
        std::string mod_fp = res_fp_base + mod_name + std::string("_sim_results.json");
        std::ifstream mod_file(mod_fp);
        if (!mod_file.is_open()) {
            std::cout << "Failed to open file: " << mod_name << std::endl;
            return 1;
        }

        json data = json::parse(mod_file);

        for (const auto& det : data["inferences"]) {
            Vec3D new_pos = Vec3D {
                .x = det["latitude"],
                .y = det["longitude"],
                .z = det["altitude"]
            };

            Vec3D new_dim = Vec3D {
                .x = det["dimensions"][0],
                .y = det["dimensions"][1],
                .z = det["dimensions"][2]
            };

            std::string classification = det["class"];
            std::map<std::string, double> classification_map = {{classification, 1}};

            fuser.add_inference(Fuser::Inference{
                new_pos,
                new_dim,
                std::move(mod_name),
                std::move(classification_map)
            });
        }
    }

    std::cout << "Loaded succsessfully!" << std::endl;

    //Assign Confidence Map
    fuser.assign_confidence_map({
        {{"camera", "pedestrian"},    1},
        {{"camera", "vehicle"},       1},
        {{"camera", "traffic_cone"},  1},
        {{"lidar",  "pedestrian"},    1},
        {{"lidar",  "vehicle"},       1},
        {{"lidar",  "traffic_cone"},  1},
        {{"radar",  "pedestrian"},    1},
        {{"radar",  "vehicle"},       1},
        {{"radar",  "traffic_cone"},  1},
    });

    //Time
    auto start = std::chrono::high_resolution_clock::now();

    fuser.order_inferences();
    fuser.merge_intersections();


    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "Infrences sorted and merged in: " << duration.count() << " ms" << std::endl;

    return 0;
}