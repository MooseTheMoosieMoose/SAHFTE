#pragma once

#include <map>
#include <string>
#include <tuple>
#include <cstdint>
    #include <iostream>
    #include <syncstream>
#include <cmath>
#include <execution>
#include <algorithm>
#include <numeric>

#include "common.hpp"
#include "mtx_ds.hpp"
#include "threadpool.hpp"
#include "coord_conv.hpp"

namespace FusionSystem {

/**
 * @brief a Fuser takes in a set of infrences, and produces a fused output based on position, size,
 * classifications, and an internal confidence map
 */
class Fuser {
public:
/*=====================================================================================================
                             Constructors, Destructors and the Big 5
=====================================================================================================*/

    Fuser(std::size_t thread_count, uint8_t spatial_bit_depth, Vec3D volume, Vec3D starting_origin) :
        bit_depth(spatial_bit_depth),
        bounding_volume(volume),
        ref_origin(starting_origin)
    {
        //Set up the pool with threads
        pool.initilize_threads(thread_count);
        
    }

    ~Fuser() = default;
    Fuser (const Fuser& other) = delete;
    Fuser& operator=(const Fuser& other) = delete;
    Fuser (Fuser&& other) = delete;
    Fuser& operator=(Fuser&& other) = delete;

/*=====================================================================================================
                            Public Structs for I/O for this Class
=====================================================================================================*/

    //A Struct which holds the details of an object detection for input, also stores helpful information
    //that can be cached and used throughout the fusion process
    struct InputInference {
        //Global space center
        Vec3D center;

        //Local space center
        Vec3D local_center;

        //Z ordering value
        std::size_t z_order;

        //Dimensions in Meters
        Vec3D dim; 

        //The modality name
        std::string modality;

        //A map which keys mod_name -> classification certainty
        std::map<std::string, double> classification; 

        bool operator<(const InputInference& other) const {
            return z_order < other.z_order;
        }
    };

    //A struct which holds the details of an output message, contains more trimmed down info
    struct OutputInference {
        Vec3D center;
        Vec3D dim;
        std::map<std::string, double> classification;
    };

/*=====================================================================================================
                                        Interface Functions
=====================================================================================================*/

    void add_inference(Vec3D&& pos, Vec3D&& dim, std::string&& mod_name, std::map<std::string, double>&& classification) {
        inferences.push_back(InputInference{
            std::move(pos),
            Vec3D{0, 0, 0},
            0,
            std::move(dim),
            std::move(mod_name),
            std::move(classification)
        });
    }

    void add_inference(const Vec3D& pos, const Vec3D& dim, const std::string& mod_name, const std::map<std::string, double>& classification) {
        inferences.push_back(InputInference{
            pos,
            Vec3D{0, 0, 0},
            0,
            dim,
            mod_name,
            classification
        });
    }

    void fuse() {
        //Set up the input size value
        input_size = inferences.size();

        //Sort infrences along their Z-order curve
        order_inferences();
        std::cout << "Inferences Ordered" << std::endl;

        identify_collisions();
        std::cout << "Collisions Identified" << std::endl;

        form_clusters();
        std::cout << "Clusters Formed" << std::endl;

        merge_and_fuse();
        std::cout << "Merge Complete" << std::endl;
    }

    void assign_confidence_map(std::map<std::pair<std::string, std::string>, double>&& map) {
        confidence_map = std::move(map);
    }

    std::queue<OutputInference>& get_output() {
        return output.storage_ref();
    }

/*=====================================================================================================
                                       Testing and Logging Functions
=====================================================================================================*/

    /**
     * @brief logs out the contents of the Z-buffer
     */
    void debug_buff() {
        for (const auto& ptr : inferences) {
            std::cout << "Z Order: " << ptr.z_order;
            const auto& class_labels = ptr.classification;
            for (const auto& pair : class_labels) {
                std::cout << " Class: " << pair.first << " Confidence: " << pair.second << std::endl; 
            }
        }
    }

private:
/*=====================================================================================================
                                 Main Internal Fusion Functions
=====================================================================================================*/

    void order_inferences() {
        //Have the threadpool convert the coords to Z-order, and to local
        TSVector<std::size_t> cull_indices;
        pool.queue_and_map_task(
            inferences,
            pool.get_max_threads() + 1,
            [&](InputInference& inf, std::size_t indx) {
                inf.local_center = geo_to_local(ref_origin, inf.center);
                auto z_coord_opt = local_to_z_order(inf.local_center, ref_origin, bounding_volume, bit_depth);
                if (z_coord_opt.has_value()) {
                    inf.z_order = z_coord_opt.value();
                } else {
                    std::osyncstream(std::cout) << "Local position: " << inf.local_center.to_string() << " was culled! "
                    "Distance to origin: " << distance_between(inf.local_center, ref_origin) << std::endl;
                    cull_indices.push_back(indx);
                }
            }
        );

        std::cout << "Infrences Count: " << input_size << std::endl;

        //Now we know the number of elements to cull, perform a swap and pop
        //This is a O(n log(k)) operation
        if (cull_indices.size() > 0) {
            auto& indices = cull_indices.storage_ref();
            std::sort(indices.begin(), indices.end(), std::greater<size_t>());
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

        std::cout << "Infrence Count: " << input_size << std::endl;
    }

    void identify_collisions() {
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

    void form_clusters() {
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

        //TEMP
        std::cout << "Clusters count: " << clusters.size() << std::endl;
    }

    void merge_and_fuse() {
        //Step 4
        //For each cluster, create the fusion, remember that the indicies received are into the Z_buffer
        /**
         * "Weighted Average: it's found by multiplying each value by its weight, summing those products, and then dividing by the total sum of the weights"
         */
        pool.queue_and_map_task(
            clusters,
            pool.get_max_threads() + 1,
            [&](std::pair<const std::size_t, std::vector<std::size_t>>& cluster) {
                //Now we can create each cluster as an average of these points
                Vec3D average_center {0.0, 0.0, 0.0};
                Vec3D average_dim {0.0, 0.0, 0.0};

                //To average the classification, we need to keep track of the total confidences and how many items were assigned to it
                //This map keys classification -> {sum of confidences, sum of weights}
                std::map<std::string, std::pair<double, double>> classification_tracker {};
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
                std::map<std::string, double> classifications {};
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
                output.push(OutputInference{
                    average_center,
                    average_dim,
                    std::move(classifications)
                });

            }
        );
    }

/*=====================================================================================================
                                         Utility Functions
=====================================================================================================*/

    /**
     * @brief an implementation of bounding box collision under the seperating axis theorem
     * @note https://en.wikipedia.org/wiki/Hyperplane_separation_theorem
     * @todo need to add a grace boundary for small objects that may not get joined as expected
     */
    bool is_intersecting(const InputInference& a, const InputInference& b) {
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

    std::size_t union_find(std::size_t indx) {
        if (parents[indx] == indx) {
            return indx;
        } else {
            return parents[indx] = union_find(parents[indx]);
        }
    }

    /**
     * @brief merges two sets
     */
    void set_unite(std::size_t i, std::size_t j) {
        std::size_t i_root = union_find(i);
        std::size_t j_root = union_find(j);
        if (i_root != j_root) {
            parents[i_root] = j_root;
        }
    }

/*=====================================================================================================
                                    General Internal Resources
=====================================================================================================*/

    //The Threadpool that we use to distribute work
    Threadpool pool {};

    //The resolution of the Z-Order partitioning used in spatial hashing
    uint8_t bit_depth;

    //The total fusion volume
    Vec3D bounding_volume {};

    //The current reference origin
    Vec3D ref_origin {};

    //A map which keys {modality, class} -> confidence for weighted averaging
    std::map<std::pair<std::string, std::string>, double> confidence_map {};

    //The total size of the input, a useful constant to have
    std::size_t input_size;

/*=====================================================================================================
                                    Input and Output Structures
=====================================================================================================*/

    //The bulk storage where objects are kept
    std::vector<InputInference> inferences {};

    //A Threadsafe queue that holds the results of fusion
    TSQueue<OutputInference> output {};

/*=====================================================================================================
                                Disjoint Set Data and Structures
=====================================================================================================*/

    //The flat map of parents used in Union Find
    std::vector<std::size_t> parents;

    //Create a queue to manage the union merges found
    TSVector<std::pair<std::size_t, std::size_t>> merges;

    //Create a map which keys roots to the children that are within it, these roots form the clusters
    std::map<std::size_t, std::vector<std::size_t>> clusters {};

}; //End definition for Fuser Class

} //End namespace fusion system