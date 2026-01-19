#pragma once

#include <map>
#include <string>
#include <tuple>
#include <cstdint>

#include "common.hpp"
#include "mtx_ds.hpp"
#include "chunk_allocator.hpp"
#include "threadpool.hpp"

namespace FusionSystem {

class FusionSystem {
public:
    FusionSystem(std::size_t thread_count) {
        //Set up the pool with threads
        pool.initilize_threads(thread_count);
    }

    ~FusionSystem() = default;
    FusionSystem (const FusionSystem& other) = delete;
    FusionSystem& operator=(const FusionSystem& other) = delete;
    FusionSystem (FusionSystem&& other) = delete;
    FusionSystem& operator=(FusionSystem&& other) = delete;
private:
    //A Struct which holds the details of an object detection
    struct Inference {
        Vec3D center;
        Vec3D dim;
        std::map<std::string, double> classification;
    };

    //A struct which holds a Z-order value, and an index into obj storage for sorting
    struct ZOrderedPtr {
        uint64_t z_order;
        Inference* indx;

        //The less than operator is required for multiset
        bool operator<(const ZOrderedPtr& other) const {
            return z_order < other.z_order;
        }
    };

    //A reussable arena allocator to store the data for our inferences
    ChunkAllocator<Inference> inferences;

    //A map which keys {modality, class} -> confidence for weighted averaging
    std::map<std::tuple<std::string, std::string>, double> confidence_map;

    //A threadsafe multiset which stores z_order indices into obj storage
    TSMultiset<ZOrderedPtr> z_order_buff {};

    //The Threadpool that we use to distribute work
    Threadpool pool {};
};

} //End namespace fusion system