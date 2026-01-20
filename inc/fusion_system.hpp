#pragma once

#include <map>
#include <string>
#include <tuple>
#include <cstdint>
#include <iostream>

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
        auto& storage = z_order_buff.storage_ref();
        for (const auto& ptr : storage) {
            std::cout << "Z Order: " << ptr.z_order;
            const auto& class_labels = ptr.indx->classification;
            for (const auto& pair : class_labels) {
                std::cout << " Class: " << pair.first << " Confidence: " << pair.second << std::endl; 
            }
        }
    }

    void order_inferences() {
        //Have the threadpool convert the coords to Z-order and add them to the multiset, this is blocking
        pool.queue_and_map_task(
            inferences,
            pool.get_max_threads() + 1,
            [this](Inference& inf) {
                auto conv_coord = geo_to_z_order(inf.center, ref_origin, bounding_volume, bit_depth);
                if (conv_coord.has_value()) {
                    z_order_buff.push(ZOrderedPtr{
                        conv_coord.value(),
                        &inf
                    });
                }
            }
        );
    }

private:
    //The resolution of the Z-Order partitioning used in spatial hashing
    uint8_t bit_depth;

    //The total fusion volume
    Vec3D bounding_volume {};

    //The current reference origin
    Vec3D ref_origin {};

    //A reussable arena allocator to store the data for our inferences
    std::vector<Inference> inferences;

    //A map which keys {modality, class} -> confidence for weighted averaging
    std::map<std::tuple<std::string, std::string>, double> confidence_map;

    //A threadsafe multiset which stores z_order indices into obj storage
    TSMultiset<ZOrderedPtr> z_order_buff {};

    //The Threadpool that we use to distribute work
    Threadpool pool {};
};

} //End namespace fusion system