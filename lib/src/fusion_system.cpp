/**                                                      
 *  ,---.    ,---.  ,--.  ,--.,------.,--------.,------. 
 * '   .-'  /  O  \ |  '--'  ||  .---''--.  .--'|  .---' 
 * `.  `-. |  .-.  ||  .--.  ||  `--,    |  |   |  `--,  
 * .-'    ||  | |  ||  |  |  ||  |`      |  |   |  `---. 
 * `-----' `--' `--'`--'  `--'`--'       `--'   `------'                                                      
 * 
 * SAHFTE (Spatial Algorithmic Hashing Fusion Time-sliced Engine)
 * @file common.cpp
 * @author Moose Abou-Harb
 * @brief this file  contains the function definitions for the headers deinfed in common.hpp
 * @copyright `26, Lisenced under whatever Paccar Inc.'s requirements are
 */

#include <utility>            //std::pair
#include <cmath>              //Useful functions like abs, round, etc
#include <execution>          //Execution policies so that the compiler will try to vectorize code if possible
#include <algorithm>          //The best header in the STL, mainly used for `std::sort`
#include <numeric>            //Gives us access to the iota initilzer used in union-find

#include "fusion_system.hpp"  //Headers for this object

namespace FusionSystem {

/*=====================================================================================================
                                        Interface Functions
=====================================================================================================*/

void Fuser::reserve_inferences(std::size_t count) {
    //Just implement reserve on the inference vector
    inferences.reserve(count);
}

void Fuser::add_inference(Vec3D&& pos, Vec3D&& dim, std::string&& mod_name, std::unordered_map<std::string, double>&& classification) {
    //Simply push back a new InputInference with these details, defaulting to 0 (basically null) for fields that we fill in later
    inferences.push_back(InputInference{
        std::move(pos),
        Vec3D{0, 0, 0},
        0,
        std::move(dim),
        std::move(mod_name),
        std::move(classification)
    });
}

void Fuser::add_inference(const Vec3D& pos, const Vec3D& dim, const std::string& mod_name, const std::unordered_map<std::string, double>& classification) {
    //Simply push back a new InputInference with these details, defaulting to 0 (basically null) for fields that we fill in later
    inferences.push_back(InputInference{
        pos,
        Vec3D{0, 0, 0},
        0,
        dim,
        mod_name,
        classification
    });
}

void Fuser::fuse() {
    //Set up the input size value
    input_size = inferences.size();

    //Sort infrences along their Z-order curve
    order_inferences();

    //Identify merge pairs
    identify_collisions();

    //Blend merge pairs into clusters
    form_clusters();

    //Take clusters and produce final fused outputs
    merge_boxes();
}

void Fuser::assign_confidence_map(std::unordered_map<std::pair<std::string, std::string>, double, PairHash>&& map) {
    confidence_map = std::move(map);
}

std::vector<Fuser::OutputInference>& Fuser::get_output() {
    return output.storage_ref();
}

/*=====================================================================================================
                                       Testing and Logging Functions
=====================================================================================================*/

void Fuser::debug_buff() {
    for (const auto& ptr : inferences) {
        std::cout << "Z Order: " << ptr.z_order;
        const auto& class_labels = ptr.classification;
        for (const auto& pair : class_labels) {
            std::cout << " Class: " << pair.first << " Confidence: " << pair.second << std::endl; 
        }
    }
}

/*=====================================================================================================
                                 Main Internal Fusion Functions
=====================================================================================================*/

void Fuser::order_inferences() {
    //Have the threadpool convert the coords to Z-order, and to local
    TSVector<std::size_t> cull_indices;
    pool.queue_and_map_task(
        inferences,
        pool.get_max_threads() + 1,
        [&](InputInference& inf, std::size_t indx) {
            inf.local_center = geo_to_local(ref_origin, inf.center);
            auto z_coord_opt = local_to_z_order(inf.local_center, bounding_volume, bit_depth);
            if (z_coord_opt.has_value()) {
                inf.z_order = z_coord_opt.value();
            } else {
                cull_indices.push_back(indx);
            }
        }
    );

    //Now we know the number of elements to cull, perform a swap and pop
    //This is a O(n log(k)) operation
    if (cull_indices.size() > 0) {
        auto& indices = cull_indices.storage_ref();
        std::sort(std::execution::par_unseq, indices.begin(), indices.end(), std::greater<size_t>());
        for (size_t indx : indices) {
            if (indx < indices.size()) {
                inferences[indx] = std::move(inferences.back());
                inferences.pop_back();
            }
        }
    }

    //Update total count
    input_size = inferences.size();

    //Now that infrences have z codes attached, lets sort them by their Z_order
    std::sort(std::execution::par_unseq, inferences.begin(), inferences.end());
}

void Fuser::identify_collisions() {
    //If there are no infrences in existence, dont do anything
    if (input_size == 0) {
        return;
    }

    //Step 1
    //Loop over the neighborhood, getting intersections, and creating merges if boxes intersect
    pool.queue_and_map_task(
        inferences,
        pool.get_max_threads() + 1,
        [&](InputInference& inf, std::size_t inf_indx) {

            //Pick a conceivably close distance where we can stop looking for neighbors
            //TODO techniqucally we could instead of picking distances create a Z-order code for this but thats for later
            const double max_range = 2 * std::max({inf.dim.x, inf.dim.y, inf.dim.z});

            //Pick a number of misses before we abandon the search
            //TODO this can be something we can set elsewhere
            const std::size_t max_misses = 25;
            std::size_t missed_count = 0;

            //Loop over the other infrences and perform intersection checks
            for (std::size_t inf_iter = inf_indx + 1; inf_iter < input_size; inf_iter++) {
                //Get the item we are checking
                const auto& check = inferences[inf_iter];

                //Check to see if we have reached an object that is outside spatial bounds
                if (distance_between(inf.local_center, check.local_center) > max_range) {
                    missed_count++;
                    if (missed_count > max_misses) {
                        break;
                    }
                    continue;
                } else {
                    missed_count = 0;
                }
                
                //Otherwise we can check to see if they are intersecting, if they are we mark it as mergable
                if (is_intersecting(inf, check)) {
                    merges.push_back({inf_indx, inf_iter});
                }
            }
        }
    );
}

void Fuser::form_clusters() {
    //Step 2
    //Process all the merge requests, starting with everyone is their own parent
    parents.resize(inferences.size());
    std::iota(parents.begin(), parents.end(), 0);
    auto& merge_storage_ref = merges.storage_ref();
    for (const auto& pair : merge_storage_ref) {
        set_unite(pair.first, pair.second);
    }

    //Step 3
    //Unite all the merges
    for (std::size_t i = 0; i < input_size; i++) {
        std::size_t root = union_find(i);
        clusters[root].push_back(i);
    }
}

void Fuser::merge_boxes() {
    //Step 4
    /**
     * "Weighted Average: it's found by multiplying each value by its weight, summing those products, and then dividing by the total sum of the weights"
     */
    //Pre-allocate our output
    //TODO add weighted averaging to pos / dim
    output.reserve(clusters.size());

    //For each cluster, create the fusion, remember that the indicies received are into the Z_buffer
    pool.queue_and_map_task(
        clusters,
        pool.get_max_threads() + 1,
        [&](std::pair<const std::size_t, std::vector<std::size_t>>& cluster) {
            //Now we can create each cluster as an average of these points
            Vec3D average_center {0.0, 0.0, 0.0};
            Vec3D average_dim {0.0, 0.0, 0.0};

            //To average the classification, we need to keep track of the total confidences and how many items were assigned to it
            //This map keys classification -> {sum of confidences, sum of weights}
            std::unordered_map<std::string, std::pair<double, double>> classification_tracker {};
            for (std::size_t indx : cluster.second) {
                //Add to the average
                average_center = average_center + inferences[indx].center;
                average_dim = average_dim + inferences[indx].dim;

                //Get the classifications and add them to the list
                auto& new_classifs = inferences[indx].classification;
                auto& mod_name = inferences[indx].modality;
                for (const auto& pair : new_classifs) {
                    //For each classification it could be, we must get its name and score
                    auto& class_name = pair.first;
                    double class_score = pair.second;

                    //To get its associated weight in our weight map, we create the key
                    //TODO if the weight is not defined for a class it defaults to one, this could be customizable
                    double weight = 1;
                    auto weight_iter = confidence_map.find({mod_name, class_name});
                    if (weight_iter != confidence_map.end()) {
                        weight = weight_iter->second;
                    }

                    //Store the weighted average building blocks in the classification tracker
                    classification_tracker[class_name].first += (class_score * weight);
                    classification_tracker[class_name].second += weight;
                }

            }

            //Normalize the center and dimensions
            const double cluster_size = cluster.second.size();
            average_center = average_center / cluster_size;
            average_dim = average_dim / cluster_size;

            //Transform the classifications into a weighted average
            std::unordered_map<std::string, double> classifications {};
            for (const auto& [class_name, weight_set] : classification_tracker) {

                //Check to prevent a divide by zero error
                if (weight_set.second > 0) {
                    classifications[class_name] = weight_set.first / weight_set.second;
                } else {
                    //If the total weights equal zero its zero
                    classifications[class_name] = 0;
                }
            }
            
            //Finally, push the inference to the output queue
            output.push_back(OutputInference{
                average_center,
                average_dim,
                std::move(classifications)
            });

        }
    );
}

/*=====================================================================================================
                                 Utility Functions and Objects
=====================================================================================================*/

bool Fuser::is_intersecting(const InputInference& a, const InputInference& b) {
    if (std::abs(a.local_center.x - b.local_center.x) * 2 > (a.dim.x + b.dim.x)) {
        return false;
    }

    if (std::abs(a.local_center.y - b.local_center.y) * 2 > (a.dim.y + b.dim.y)) {
        return false;
    }

    if (std::abs(a.local_center.z - b.local_center.z) * 2 > (a.dim.z + b.dim.z)) {
        return false;
    }

    return true;
}

std::size_t Fuser::union_find(std::size_t indx) {
    if (parents[indx] == indx) {
        return indx;
    } else {
        return parents[indx] = union_find(parents[indx]);
    }
}

void Fuser::set_unite(std::size_t i, std::size_t j) {
    std::size_t i_root = union_find(i);
    std::size_t j_root = union_find(j);
    if (i_root != j_root) {
        parents[i_root] = j_root;
    }
}

} //End namespace fusion system