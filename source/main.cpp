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

struct InferenceData {
    double timestamp;
    std::string class_name;
    double latitude;
    double longitude;
    double altitude;
    Vec3D dim;
    std::size_t obj_id;
};

void to_json(json& j, const InferenceData& d) {
    j = json {
        {"timestamp", 0.0},
        {"class", d.class_name},
        {"latitude", d.latitude},
        {"longitude", d.longitude},
        {"altitude", d.altitude},
        {"dimensions", std::vector<double>{d.dim.x, d.dim.y, d.dim.z}},
        {"obj_id", 0.0}
    };
}


int main(int argc, char* argv[]) {
    //Get the file paths that we will be writing
    const std::string gt_fp = "./test_data/gt_sim_results.json";
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
    Fuser fuser(4, 8, Vec3D{250, 250, 250}, origin);

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
            std::unordered_map<std::string, double> classification_map = {{classification, 1}};

            fuser.add_inference(new_pos, new_dim, mod_name, std::move(classification_map));
        }
    }

    std::cout << "Step 1 Complete: JSON data Loaded succsessfully!" << std::endl;

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
    std::cout << "Step 2 Complete: Confidence Map Assigned, starting performance clock" << std::endl; 
    auto start = std::chrono::high_resolution_clock::now();

    fuser.fuse();


    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "Step 3: Infrences sorted and merged in: " << duration.count() << " ms" << std::endl;

    //Dump to JSON and log
    std::cout << "Final Infrence clusters: " << std::endl;
    json output_data;
    output_data["modality"] = "fusion";
    std::vector<InferenceData> dump_data {};

    auto& output_q = fuser.get_output();
    while (output_q.size() != 0) {
        auto elem = output_q.back();
        output_q.pop_back();
        std::cout << "Item centered at: " << elem.center.to_string() << std::endl;
        std::cout << "Item Dim: " << elem.dim.to_string() << std::endl;;
        std::cout << "Item Class List: " << std::endl;
        for (const auto& class_pair : elem.classification) {
            std::cout << "Class: " << class_pair.first << " Weight: " << class_pair.second << std::endl;
        }
        std::cout << "\n\n" << std::endl;

        dump_data.push_back(InferenceData{
            0,
            "FixMe",
            elem.center.x,
            elem.center.y,
            elem.center.z,
            elem.dim,
            0
        });
    }

    output_data["inferences"] = dump_data;
    std::ofstream file("./test_data/fusion_sim_results.json");
    file << output_data.dump(4);

    std::cout << "Json Dumped" << std::endl;

    return 0;
}