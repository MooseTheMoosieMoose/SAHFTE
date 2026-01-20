#pragma once

#include <map>
#include <string>
#include <tuple>
#include <cstdint>
#include <iostream>
#include <cmath>
#include <shared_mutex>
#include <atomic>
#include <execution>
#include <algorithm>
#include <numeric>

#include "common.hpp"
#include "mtx_ds.hpp"
#include "chunk_allocator.hpp"
#include "threadpool.hpp"
#include "coord_conv.hpp"

namespace FusionSystem {

class Fuser {
public:
    //A Struct which holds the details of an object detection
    struct Inference {
        Vec3D center;
        Vec3D dim;
        std::map<std::string, double> classification;
    };
private:
    //An internal structure that sorts inferences by Z-order without moving them
    struct ZOrderedPtr {
        uint64_t z_order;
        Inference* indx;

        //The less than operator is required for multiset
        bool operator<(const ZOrderedPtr& other) const {
            return z_order < other.z_order;
        }
    };

    //An internal structure utilized by the union find clustering algorithm for bb collisions
    struct UnionFindSet {
        std::vector<std::size_t> parents;

        void init(std::size_t count) {
            parents.resize(count);
            std::iota(parents.begin(), parents.end(), 0);
        }

        std::size_t find(std::size_t indx) {
            if (parents[indx] == indx) {
                return indx;
            } else {
                return parents[indx] = find(parents[indx]);
            }
        }

        /**
         * @brief merges two sets
         */
        void unite(std::size_t i, std::size_t j) {
            std::size_t i_root = find(i);
            std::size_t j_root = find(j);
            if (i_root != j_root) {
                parents[i_root] = j_root;
            }
        }
    };
public:

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

    void add_inference(Inference&& elem) {
        inferences.push_back(std::move(elem));
    }

    void add_inference(const Inference& elem) {
        inferences.push_back(std::move(elem));
    }

    /**
     * @brief logs out the contents of the Z-buffer
     */
    void debug_z_buff() {
        for (const auto& ptr : z_buff) {
            std::cout << "Z Order: " << ptr.z_order;
            const auto& class_labels = ptr.indx->classification;
            for (const auto& pair : class_labels) {
                std::cout << " Class: " << pair.first << " Confidence: " << pair.second << std::endl; 
            }
        }
    }

    void order_inferences() {
        //Ensure that the z_buffer has enough elements in memory so that we can write directly to indicies
        z_buff.resize(inferences.size());

        //Have the threadpool convert the coords to Z-order and add them to the multiset, this is blocking
        pool.queue_and_map_task(
            inferences,
            pool.get_max_threads() + 1,
            [this](Inference& inf) {
                auto conv_coord = geo_to_z_order(inf.center, ref_origin, bounding_volume, bit_depth);
                if (conv_coord.has_value()) {
                    std::size_t indx = &inf - &inferences[0];
                    z_buff[indx] = ZOrderedPtr{
                        conv_coord.value(),
                        &inf
                    };
                }
            }
        );

        //Now that infrences have z codes attached, lets sort the z_buff
        std::sort(std::execution::par_unseq, z_buff.begin(), z_buff.end());
    }

    void merge_intersections() {
        //Get the total number of infrences in the system
        std::size_t total_count = inferences.size();
        
        //If there are no infrences in existence, dont do anything
        if (total_count == 0) {
            return;
        }

        //Step 1
        //Create a queue to manage the union merges found
        TSVector<std::pair<std::size_t, std::size_t>> merges;

        //Loop over the neighborhood, getting intersections, and creating merges if boxes intersect
        pool.queue_and_map_task(
            z_buff,
            pool.get_max_threads() + 1,
            [&](ZOrderedPtr& z_ptr) {
                
                //Get the inference we are comparing against
                const auto& subject = z_ptr.indx;

                //Pick a conceivably close distance where we can stop looking for neighbors
                const double max_range = 2 * std::max({subject->dim.x, subject->dim.x, subject->dim.y});

                //Pick a number of misses before we abandon the search
                const std::size_t max_misses = 25;
                std::size_t missed_count = 0;

                //Loop over the other infrences and perform intersection checks
                std::size_t z_indx = &z_ptr - &z_buff[0];
                for (int z_iter = z_indx + 1; z_iter < total_count; z_iter++) {
                    //Get the item we are checking
                    const auto& check = z_buff[z_iter].indx;

                    //Check to see if we have reached an object that is outside spatial bounds
                    if (distance_between(subject->center, check->center) > max_range) {
                        missed_count++;
                        if (missed_count > max_misses) {
                            break;
                        }
                    }
                    
                    //Otherwise we can check to see if they are intersecting, if they are we mark it as mergable
                    if (is_intersecting(subject, check)) {
                        merges.push_back({z_indx, z_iter});
                    }
                }
            }
        );

        //Step 2
        //Process all the merge requests
        UnionFindSet union_set;
        union_set.init(total_count);
        auto& merge_storage_ref = merges.storage_ref();
        for (const auto& pair : merge_storage_ref) {
            union_set.unite(pair.first, pair.second);
        }

        //Step 3
        //Create a map which keys roots to the children that are within it, these roots form the clusters
        std::map<std::size_t, std::vector<std::size_t>> clusters {};
        for (std::size_t i = 0; i < total_count; i++) {
            std::size_t root = union_set.find(i);
            clusters[root].push_back(i);
        }

        //Step 4
        //log print all the clusters
        for (const auto& pair : clusters) {
            std::cout << "Cluster Size: " << pair.second.size() << std::endl;
            
            for (const auto& z_index : pair.second) {
                auto* inf_ptr = z_buff[z_index].indx;
                std::cout << "Pos: " << geo_to_local(ref_origin, inf_ptr->center).to_string();
                std::cout << " Dim: " << inf_ptr->dim.to_string();
                auto& class_map = inf_ptr->classification;
                for (const auto& class_pair : class_map) {
                    std::cout << " Class: " << class_pair.first << std::endl;
                }
            }
            std::cout << "\n" << std::endl;
        }
    }

    /**
     * @brief an implementation of bounding box collision under the seperating axis theorem
     * @note https://en.wikipedia.org/wiki/Hyperplane_separation_theorem
     */
    bool is_intersecting(const Inference* a, const Inference* b) {
        if (std::abs(a->center.x - b->center.x) * 2 > (a->dim.x + b->dim.x)) {
            return false;
        }

        if (std::abs(a->center.y - b->center.y) * 2 > (a->dim.y + b->dim.y)) {
            return false;
        }

        if (std::abs(a->center.z - b->center.z) * 2 > (a->dim.z + b->dim.z)) {
            return false;
        }

        return true;
    }

private:
    //The resolution of the Z-Order partitioning used in spatial hashing
    uint8_t bit_depth;

    //The total fusion volume
    Vec3D bounding_volume {};

    //The current reference origin
    Vec3D ref_origin {};

    //The bulk storage where objects are kept
    std::vector<Inference> inferences;

    //A map which keys {modality, class} -> confidence for weighted averaging
    std::map<std::tuple<std::string, std::string>, double> confidence_map;

    //A Working copy of the z_order_buff that is random access, and easier to work with
    std::vector<ZOrderedPtr> z_buff {};

    //A Threadsafe vector that holds the results of fusion
    TSVector<Inference> output {};

    //The Threadpool that we use to distribute work
    Threadpool pool {};

};

} //End namespace fusion system