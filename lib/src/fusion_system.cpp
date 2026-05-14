/**                                                      
 *  ,---.    ,---.  ,--.  ,--.,------.,--------.,------. 
 * '   .-'  /  O  \ |  '--'  ||  .---''--.  .--'|  .---' 
 * `.  `-. |  .-.  ||  .--.  ||  `--,    |  |   |  `--,  
 * .-'    ||  | |  ||  |  |  ||  |`      |  |   |  `---. 
 * `-----' `--' `--'`--'  `--'`--'       `--'   `------'                                                      
 * 
 * SAHFTE (Spatial Algorithmic Hashing Fusion Time-sliced Engine)
 * @file fusion_system.cpp
 * @author Moose Abou-Harb
 * @brief this file has the function definitions for the classes described in fusion_system.hpp
 * @copyright `26, Moose Abou-Harb under the 3-Clause BSD Lisence
 */

#include <utility>            //std::pair
#include <cmath>              //Useful functions like abs, round, etc
#include <execution>          //Execution policies so that the compiler will try to vectorize code if possible
#include <algorithm>          //The best header in the STL, mainly used for `std::sort`
#include <numeric>            //Gives us access to the iota initilzer used in union-find
#include <exception>          //An exception to wrap the gnarlier bits and bobs
#include <cmath>              //std::fmod
#include <numbers>            //the blessed dessert number

#ifdef USE_STD_FORMAT
        #include <format>     //Conditionall use std::format if available
#endif

#include "fusion_system.hpp"  //Headers for this object

namespace FusionSystem { //begin namespace FusionSystem

/*=====================================================================================================
                                        Interface Functions
=====================================================================================================*/

void Fuser::reserve_inferences(std::size_t count) {
    //Just implement reserve on the inference vector
    inferences.reserve(count);
}

void Fuser::add_inference(
    Vec3D pos, 
    Vec3D dim, 
    double rotation, 
    std::size_t mod_name, 
    std::size_t class_name, 
    double confidence,
    std::string uuid,
    bool global_position
) {
    //Switch based on if the position is given in global or local coordinates,
    //Populating the missing coordinate system along the way.
    if (global_position) {
        Vec3D local_pos = geo_to_local(ref_origin, pos, ref_heading_cos, ref_heading_sin);
        inferences.push_back(ObjectDetection{
            .uuid = uuid,
            .center = pos,
            .local_center = local_pos, //I hope NRVO can make this snappy
            .dim = dim,
            .axes_a = Vec2D{0.0, 0.0},
            .axes_b = Vec2D{0.0, 0.0},
            .modality = mod_name,
            .class_name = class_name,
            .z_order = 0,
            .rotation = rotation,
            .det_confidence = confidence,
        });
    } else {
        Vec3D global_pos = local_to_geo(ref_origin, pos, ref_heading_cos, ref_heading_sin);
        inferences.push_back(ObjectDetection{
            .uuid = uuid,
            .center = global_pos,
            .local_center = pos, //I hope NRVO can make this snappy
            .dim = dim,
            .axes_a = Vec2D{0.0, 0.0},
            .axes_b = Vec2D{0.0, 0.0},
            .modality = mod_name,
            .class_name = class_name,
            .z_order = 0,
            .rotation = rotation,
            .det_confidence = confidence,
        });
    }
}

void Fuser::fuse(std::size_t mod_count) {

    //Set our good flag to all good
    good_flag = true;

    if (mod_count != 1) {
        //Wrap our subcalls in a try-catch
        try {
            //Set up the input size value
            input_size = inferences.size();

            //Sort infrences along their Z-order curve
            order_inferences();

            //error_buffer = std::format("{} - After Ordering inferences, input size is: {}", error_buffer, input_size);

            //Identify merge pairs
            identify_collisions();

            //error_buffer = std::format("{} - After searching for merges, merge count is: {}", error_buffer, merges.size());

            //Blend merge pairs into clusters
            form_clusters();

            //error_buffer = std::format("{} - Cluster count after forming clusters is: {}", error_buffer, clusters.size());

            //Take clusters and produce final fused outputs
            merge_boxes();

            //error_buffer = std::format("{} - After fusing output size is: {}", error_buffer, output.size());

        } catch (const std::exception& e) {
            good_flag = false;
            error_buffer = e.what();
        }
    } else {
        //Since we only have one modality that is already annotated with its relative position, dump it to output
        for (const auto& inference : inferences) {
            output.storage_ref().push_back(FusionResult {
                .uuid = std::string(inference.uuid),
                .local_position = inference.local_center,
                .global_position = inference.center,
                .dimensions = inference.dim,
                .class_name = inference.class_name,
                .rotation = inference.rotation,
                .confidence = inference.det_confidence
            });
        }
    }
}

void Fuser::empty_buffers() {
    //Calling clear on these invokes the destructors for all of them without
    //changing the underlying allocation
    inferences.clear();
    output.clear();
    merges.clear();
    clusters.clear();
    parents.clear();
}

void Fuser::assign_class_confidence_map(
    std::vector<std::vector<double>> map,
    double default_val
) {
    class_confidence_map = map;
    class_conf_default = default_val;
}

void Fuser::assign_modality_pos_confidence_map(
    std::vector<double> map,
    double default_val
) {
    position_confidence_map = map;
    pos_conf_default = default_val;
}

void Fuser::assign_modality_dim_confidence_map(
    std::vector<double> map,
    double default_val
) {
    dimension_confidence_map = map;
    dim_conf_default = default_val;
}

void Fuser::set_reference_origin(Vec3D new_origin, double new_heading) {
    ref_origin = new_origin;
    ref_heading = new_heading;

    //Precache the sin and cos values so that when we do coord conversions its done
    ref_heading_cos = std::cos(new_heading);
    ref_heading_sin = std::sin(new_heading);
}

std::vector<Fuser::FusionResult>& Fuser::get_output() {
    return output.storage_ref();
}

bool Fuser::is_ok() {
    return good_flag;
}

std::string Fuser::get_error() {
    auto lc = std::string(error_buffer);
    error_buffer.clear();
    error_buffer.shrink_to_fit();
    return lc;
}

/*=====================================================================================================
                                       Testing and Logging Functions
=====================================================================================================*/

#ifdef DEBUGGING

void Fuser::debug_buff() {
    for (const auto& ptr : inferences) {
        std::cout << "Z Order: " << ptr.z_order;
        const auto& class_labels = ptr.classification;
        for (const auto& pair : class_labels) {
            std::cout << " Class: " << pair.first << " Confidence: " << pair.second << std::endl; 
        }
    }
}

#endif

/*=====================================================================================================
                                 Main Internal Fusion Functions
=====================================================================================================*/

void Fuser::order_inferences() {
    //Have the threadpool convert the coords to Z-order, and to local
    TSVector<std::size_t> cull_indices;
    pool.queue_and_map_task(
        inferences,
        pool.get_max_threads() + 1,
        [&](ObjectDetection& inf, std::size_t indx) {

            //Note inference coordinate conversion has been moved to the `add_inference` methods
            //To add flexability between geo and local coords

            auto z_coord_opt = local_to_z_order(inf.local_center, bounding_volume, bit_depth);
            if (z_coord_opt.has_value()) {
                inf.z_order = z_coord_opt.value();
                
                //Since we are here, and we know that the object is in our FOV we can calculate
                //Its SAT axies
                double cos_ax = std::cos(inf.rotation);
                double sin_ax = std::sin(inf.rotation);
                inf.axes_a = Vec2D{cos_ax, sin_ax};
                inf.axes_b = Vec2D{-1.0 * sin_ax, cos_ax};

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
            //Changed indicies -> inferences
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
        [&](ObjectDetection& inf, std::size_t inf_indx) {

            //Pick a conceivably close distance where we can stop looking for neighbors
            //TODO techniqucally we could instead of picking distances create a Z-order code for this but thats for later
            //In V2 with the change to calculating squared distances this is now a squared distance
            const double max_range = std::pow(std::max({inf.dim.x, inf.dim.y, inf.dim.z}), 2);

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
    //Pre-allocate our output
    output.reserve(clusters.size());

    //For each cluster, create the fusion
    pool.queue_and_map_task(
        clusters,
        pool.get_max_threads() + 1,
        [&](std::pair<const std::size_t, std::vector<std::size_t>>& cluster) {
            
            //See if we can fast track cluster calculations becuase its a 1 item group
            std::size_t cluster_size = cluster.second.size();
            if (cluster_size == 1) {
                //Grab the single item cluster as an auto ref for cleaner access
                auto& single_cluster = inferences[cluster.second[0]];
                output.push_back(FusionResult{
                    .uuid = std::string(single_cluster.uuid),
                    .local_position = single_cluster.local_center,
                    .global_position = single_cluster.center,
                    .dimensions = single_cluster.dim,
                    .class_name = single_cluster.class_name,
                    .rotation = single_cluster.rotation,
                    .confidence = single_cluster.det_confidence
                });
                return;
            }

            //If fast track fails we have to perform the cluster fusion
            //Create variables to store the weighted averages and the weight sums
            //TODO it would be great to refactor this so that we have no extra allocations in
            //The tracking map for seen classes
            Vec3D average_pos = Vec3D{0.0, 0.0, 0.0};
            double pos_weight_sum = 0;

            Vec3D average_dim = Vec3D{0.0, 0.0, 0.0};
            double dim_weight_sum = 0;

            double average_rot = 0;

            //This map keeps track of the number of times we have seen a given class
            std::unordered_map<std::size_t, double> class_map;
            double class_weight_sum = 0;

            //UUIDs are very well behaved, and every unique UUID
            //is 36 charachters long, so we can prealloc our string
            //and take into account the : seperator
            std::string uuid_string = "";
            uuid_string.reserve((36 * cluster_size) + (cluster_size - 1));

            //Sum everything up
            std::size_t cluster_indx = 0;
            for (const auto indx : cluster.second) {
                auto& new_node = inferences[indx];

                //Grab the position and add to its weight sum
                average_pos = average_pos + new_node.local_center;
                pos_weight_sum += get_pos_confidence(new_node.modality);

                //Grab the dimensions and add to its weights sum
                average_dim = average_dim + new_node.dim;
                dim_weight_sum += get_dim_confidence(new_node.modality);

                //Add up the rotation for our running total
                average_rot += new_node.rotation;

                //Grab the class map and add confidences
                auto map_iter = class_map.find(new_node.class_name);
                if (map_iter != class_map.end()) {
                    //Map position already exists, update that position
                    map_iter->second += new_node.det_confidence;
                } else {
                    class_map.insert({new_node.class_name, new_node.det_confidence});
                }

                //Add to our class weigt sum
                class_weight_sum += get_class_confidence({new_node.modality, new_node.class_name});

                //Append the new node UUID to the running UUID string,
                //If we are at the last one dont add a trailing : sign
                if (cluster_indx == (cluster_size - 1)) {
#ifdef USE_STD_FORMAT
                    uuid_string = std::format("{}{}", uuid_string, new_node.uuid);
#else
                    uuid_string += new_node.uuid;
#endif
                } else {
#ifdef USE_STD_FORMAT
                    uuid_string = std::format("{}{}:", uuid_string, new_node.uuid);
#else
                    uuid_string += std::string(new_node.uuid).append(1, ':');
#endif
                }

                //Advance counter to keep track of things
                cluster_indx++;
            }

            //Normalize the simple averages
            average_pos = average_pos / pos_weight_sum;
            average_dim = average_dim / dim_weight_sum;

            //Make sure rotation is clamped along [0, 2pi]
            average_rot /= pos_weight_sum;
            average_rot = std::fmod(average_rot, 2 * std::numbers::pi);

            //Normalize the class sums and extract the maximum
            //As of V3 We now use iterators and max_element to optimize the algorithms
            auto max_conf_iter = std::max_element(
                class_map.begin(), 
                class_map.end(),
                [](const auto& a, const auto& b) {
                    return a.second < b.second;
                }
            );
            double max_conf = max_conf_iter->second / class_weight_sum;
            std::size_t selected_class = max_conf_iter->first;

            //Create a new value and push
            output.push_back(FusionResult{
                .uuid = uuid_string,
                .local_position = average_pos,
                .global_position = local_to_geo(ref_origin, average_pos, ref_heading_cos, ref_heading_sin),
                .dimensions = average_dim,
                .class_name = selected_class,
                .rotation = average_rot,
                .confidence = max_conf,
            });
        }
    );
}

/*=====================================================================================================
                                 Utility Functions and Objects
=====================================================================================================*/

bool Fuser::is_intersecting(const ObjectDetection& a, const ObjectDetection& b) {

    //since our Z axis is still aligned, we can check that first
    if (std::abs(a.local_center.z - b.local_center.z) * 2 > (a.dim.z + b.dim.z)) {
        return false;
    }

    //Claculate the vector between centers
    Vec2D connecting_vec = { b.local_center.x - a.local_center.x, b.local_center.y - a.local_center.y };

    //create a lambda that is performed across each set of axies
    auto has_separating_axis = [&](const Vec2D& axis) {
        //Project the center onto the given axis
        double dist = std::abs(connecting_vec.x * axis.x + connecting_vec.y * axis.y);

        //Project half of box a
        double ra = (a.dim.x / 2.0) * std::abs(a.axes_a.x * axis.x + a.axes_a.y * axis.y) + (a.dim.y / 2.0) * std::abs(a.axes_b.x * axis.x + a.axes_b.y * axis.y);

        //Project half of box b
        double rb = (b.dim.x / 2.0) * std::abs(b.axes_a.x * axis.x + b.axes_a.y * axis.y) + (b.dim.y / 2.0) * std::abs(b.axes_b.x * axis.x + b.axes_b.y * axis.y);

        //If the distance is creater than the sum of the projections, they arent intersecting
        return dist > (ra + rb);
    };

    //Test all the axis from both boxes
    if (has_separating_axis(a.axes_a)) { return false; }
    else if (has_separating_axis(a.axes_b)) { return false; }
    else if (has_separating_axis(b.axes_a)) { return false; }
    else if (has_separating_axis(b.axes_b)) { return false; }

    //If all tests have passed the boxes do indeed intersect
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

double Fuser::get_pos_confidence(std::size_t key) {
    if (key < position_confidence_map.size()) {
        return position_confidence_map[key];
    } else {
        return pos_conf_default;
    }
}

double Fuser::get_dim_confidence(std::size_t key) {
    if (key < dimension_confidence_map.size()) {
        return dimension_confidence_map[key];
    } else {
        return dim_conf_default;
    }
}

double Fuser::get_class_confidence(std::pair<std::size_t, std::size_t> key) {
    //Short circuit semantics should make this safe
    if (key.first < class_confidence_map.size() && key.second < class_confidence_map[key.first].size()) {
        return class_confidence_map[key.first][key.second];
    } else {
        return class_conf_default;
    }
}

} // End namespace fusion system