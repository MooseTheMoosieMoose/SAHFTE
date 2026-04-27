/**                                                      
 *  ,---.    ,---.  ,--.  ,--.,------.,--------.,------. 
 * '   .-'  /  O  \ |  '--'  ||  .---''--.  .--'|  .---' 
 * `.  `-. |  .-.  ||  .--.  ||  `--,    |  |   |  `--,  
 * .-'    ||  | |  ||  |  |  ||  |`      |  |   |  `---. 
 * `-----' `--' `--'`--'  `--'`--'       `--'   `------'                                                      
 * 
 * SAHFTE (Spatial Algorithmic Hashing Fusion Time-sliced Engine)
 * @file fusion_system.hpp
 * @author Moose Abou-Harb
 * @brief this file  contains the headers and structs needed to interact with the Fusion System, when building as
 * a library this should serve as the primary header interface
 * @copyright `26, Lisenced under whatever Paccar Inc.'s requirements are
 */

#pragma once

#include <unordered_map> //For hash maps
#include <string>        //For dynamic strings
#include <cstdint>       //For standard ints (uint32_t, etc)

#include "common.hpp"     //Common coord conversions and Vec3D
#include "mtx_ds.hpp"     //Thread safe queues and vectors
#include "threadpool.hpp" //The threadpool

namespace FusionSystem {

/**
 * @brief a Fuser takes in a set of inferences, and produces a fused output based on position, size,
 * classifications, and an internal confidence map
 * @note Interacting with this system should have the following steps:
 * 
 * 1. Construct an instance of the `Fuser` object with a given number of auxillary threads, a bit_depth, a 
 * fusion volume and the current origin
 * 
 * 2. Add any extra details needed like `assign_confidence_map()` to tweak the system as needed
 * 
 * 3. In a loop perform the following:
 * 
 * 3.a. Ensure your origin is up to date with `set_reference_origin()`
 * 
 * 3.b. Add infrences using the `add_inference()` methods
 * 
 * 3.c. Instruct the system to perform a fusion with the `fuse()` method
 * 
 * 3.d. Pop fused outputs using the `get_output()` method
 * 
 * 3.e. Flush the input/output buffers with `empty_buffers()`
 */
class Fuser {
public:
/*=====================================================================================================
                             Constructors, Destructors and the Big 5
=====================================================================================================*/

    /**
     * @brief the constructor for the Fuser Object
     * @note this object is not copy constructable or movable, that is becuase it manages several threading things
     * that do not support thoes operations
     * @param thread_count is the number of auxillary threads you want the threadpool to utilize. All threaded operations
     * work under a fork-join model, where work is distributed across the auxillary and calling threads
     * @param spatial_bit_depth as an optimization, all infrences are sorted along a Z-order curve, this number is bounded
     * on 1 <= bit_depth <= 21, and defines the number of divisions in the volume that the Z-order curve will use. The higher this
     * number the more granular the spatial division, but this has diminishing returns
     * @param volume is the total space around the reference origin that we are interested in. This is in meters.
     * @param starting_origin the point in global space (latitude, longitude, altitude) where the fusion will center around
     */
    Fuser(std::size_t thread_count, uint8_t spatial_bit_depth, Vec3D volume, Vec3D starting_origin) :
        bit_depth(spatial_bit_depth),
        bounding_volume(volume),
        ref_origin(starting_origin)
    {
        //Set up the pool with threads
        pool.initilize_threads(thread_count);
    }

    /**
     * @brief the destructor of the system, which is default defined to allow the compiler to call all our 
     * child destructors when this object goes out of scope
     * @note this is not meant to be called manually, unless you really know what you are doing!
     */
    ~Fuser() = default;

    /**
     * @brief the copy constructor for the object, deleted due to the complex threading that this object manages
     */
    Fuser (const Fuser& other) = delete;

    /**
     * @brief the copy assignment constructor for the object, deleted due to the complex threading that this object manages
     */
    Fuser& operator=(const Fuser& other) = delete;

    /**
     * @brief the move constructor for the object, deleted due to the complex threading that this object manages
     */
    Fuser (Fuser&& other) = delete;

    /**
     * @brief the move assignment constructor for the object, deleted due to the complex threading that this object manages
     */
    Fuser& operator=(Fuser&& other) = delete;

/*=====================================================================================================
                            Public Structs for I/O for this Class
=====================================================================================================*/

    /**
     * @brief A Struct which holds the details of an object detection for input, also stores helpful information that 
     * can be cached and used throughout the fusion process
     * @todo change layout to optimize alignment
     */
    struct ObjectDetection {
        //Global space center
        Vec3D center;

        //Local space center
        Vec3D local_center;

        //Z ordering value
        std::size_t z_order;

        //Dimensions in Meters
        Vec3D dim;

        //Rotation of the detection along its Z-axis
        double rotation;

        //The modality name
        std::string modality;

        //The Name of the class detection
        std::string class_name;

        //The detection confidence
        double det_confidence;

        //The UUID of the detection
        std::string uuid;

        /**
         * @brief an overload of the less than operator to provide `std::sort` with an operation to sort based
         * on the Z-order value
         */
        bool operator<(const ObjectDetection& other) const {
            return z_order < other.z_order;
        }
    };

    /**
     * @brief a struct to hold the PairHash operator, which is used in the class wide confidence
     * map so that they can employ std::unorderd map, and their O(1) access & insert time
     */
    struct PairHash {
        /**
         * @brief the hashing operator used by the class wide confidence map, employs `std::hash`
         * across an `std::pair` of `std::strings`
         */
        std::size_t operator() (const std::pair<std::string, std::string>& p) const {
            std::size_t first_hash = std::hash<std::string>{}(p.first);
            std::size_t second_hash = std::hash<std::string>{}(p.second);
            return first_hash ^ (second_hash << 1);
        }
    };

/*=====================================================================================================
                                        Interface Functions
=====================================================================================================*/

    /**
     * @brief reserves the internal inference buffer, can probably increase the speed of performing fusions by 
     * pre-allocating outright the number of items in use
     * @param count is the number of elements to reserve
     * @note this function is not strictly necessary for functionality, but will increase overall performance
     */
    void reserve_inferences(std::size_t count);

    /**
     * @brief inserts an inference into the system
     * @param pos a position in global space (latitude, longitude, altitude) as a `Vec3D`
     * @param dim the dimensions of the object in meters as a `Vec3D`
     * @param rotation the rotation of the object around the up/down (Z) axis
     * @param mod_name the name of the modality that the inference is coming from
     * @param class_name the detected class
     * @param uuid the UUID of the detection for logging and tracking
     * @param confidence the confidence in the detected class bounded on [0, 1]
     * @note this is the R-Value version of this function, and will `std::move` values
     * @note once an inference is staged with this function the only way to remove it is to clear
     * the entire internal inference buffer via a call to `empty_buffers()`
     */
    void add_inference(
        Vec3D&& pos, 
        Vec3D&& dim, 
        double rotation, 
        std::string&& mod_name, 
        std::string&& class_name, 
        double confidence,
        std::string&& uuid,
        bool global_position = true
    );

    /**
     * @brief inserts an inference into the system
     * @param pos a position in global space (latitude, longitude, altitude) as a `Vec3D`
     * @param dim the dimensions of the object in meters as a `Vec3D`
     * @param rotation the rotation of the object around the up/down (Z) axis
     * @param mod_name the name of the modality that the inference is coming from
     * @param class_name the detected class
     * @param uuid the UUID of the detection for logging and tracking
     * @param confidence the confidence in the detected class bounded on [0, 1]
     * @note this is the L-Value version of this function, and will NOT `std::move` values
     * @note once an inference is staged with this function the only way to remove it is to clear
     * the entire internal inference buffer via a call to `empty_buffers()`
     */
    void add_inference(
        Vec3D& pos, 
        Vec3D& dim, 
        double rotation, 
        std::string& mod_name, 
        std::string& class_name, 
        double confidence,
        std::string& uuid,
        bool global_position = true
    );

    /**
     * @brief invokes the internal functions used for fusing, will take all current settings, all pushed inferneces, the origin they were
     * pushed with, etc and will populate the output vector, which can be fetched with `get_output()`
     */
    void fuse();
    
    /**
     * @brief gets a reference to the output vector, this view should be treated as read only, but is not const for debugging
     * @note this will only ever be populated if infrences are added with `add_inference()` and `fuse()` is called on them
     */
    std::vector<ObjectDetection>& get_output();

    /**
     * @brief emptys the internal inference input and output buffers. This will NOT free the actual allocations of either of these
     * buffers, but instead destruct the entire contents of the buffers.
     * @warning this is destructive, non-reversible and should only be called once the fusion results have been extracted
     */
    void empty_buffers();

    /**
     * @brief assigns the internal confidence map that will tie an `std::pair` of {modality, class} to a weight that is applied when fusing
     * to bias the final output class of the fuser
     * @param map the map, this is best constructed in place using initilizer list syntax for brevity and clarity
     * @param default_val the default value for class confidences that arent in the provided map
     */
    void assign_class_confidence_map(std::unordered_map<std::pair<std::string, std::string>, double, PairHash>&& map, double default_val = 1);

    /**
     * @brief assigns the internal confidence map that ties a modality passed as a string and a weight that is applied when fusing
     * to bias the position of the fused detections
     * @param map the map of `{modality, weight}` that biases the fusion
     * @param default_val the default value for class confidences that arent in the provided map
     */
    void assign_modality_pos_confidence_map(std::unordered_map<std::string, double>&& map, double default_val = 1);

    /**
     * @brief assigns the internal confidence map that ties a modality passed as a string and a weight that is applied when fusing to bias the final
     * dimensions of any fusion produced
     * @param map the map of `{modality, weight}` that biases the fusion
     * @param default_val the default value for class confidences that arent in the provided map
     */
    void assign_modality_dim_confidence_map(std::unordered_map<std::string, double>&& map, double default_val = 1);

    /**
     * @brief assign the current global (lat, long, alt) position that serves as the global reference origin
     * @param new_origin the new vec3D to position the reference origin at
     */
    void set_reference_origin(Vec3D new_origin);

    /**
     * @brief returns the status of the internal state of the fuser. If false, then something messed up,
     * consider calling `empty_buffers()` and evaluate the problem with `get_error()`
     * @returns a bool representing good status (true), or an internal error (false)
     */
    bool is_ok();

    /**
     * @brief returns and empties the error message buffer. This is populated whenever `is_ok()` returns false
     * @returns an `std::string` representing the error message from `e.what()`
     */
    std::string get_error();

/*=====================================================================================================
                                       Testing and Logging Functions
=====================================================================================================*/

#ifdef DEBUGGING

    #include <iostream>
    #include <syncstream> //Uncomment me if you want to add synced output for debugging
    /**
     * @brief a debug function that can drop the current state of the infrence buffer to the console in a formatted manner,
     * in case you are tweaking internals, this should NOT be a part of any external API, but is public so that you can use
     * it with C++ debugging
     */
    void debug_buff();

#endif

private:
/*=====================================================================================================
                                 Main Internal Fusion Functions
=====================================================================================================*/

    /**
     * @brief sorts all infrences added into the system along a Z-order curve, trimming out any items that do not fit
     * within the systems designated bounding volume. This will populate the Z-order value, and the local_center value
     * of the inferences, as well as resize the `inferences` vector
     */
    void order_inferences();

    /**
     * @brief applys `is_intersecting()` to a sorted subsection of the inferences to identify clusters that intersect.
     * This adds merge requests into `merges` so that they can be joined into a single root
     */
    void identify_collisions();

    /**
     * @brief the only serial stage of the pipeline, which takes in all the merge-requests and joins them together
     * so that all objects that collide will share the same root, this is highly efficient thanks to path compression
     * so the serial aspect should not be a slow down
     */
    void form_clusters();

    /**
     * @brief takes in the clusters and computes the weighted averages, populating the `output` vector
     */
    void merge_boxes();

/*=====================================================================================================
                                 Utility Functions and Objects
=====================================================================================================*/

    /**
     * @brief an implementation of bounding box collision under the seperating axis theorem
     * @param a an ObjectDetection that you want to check for bounding
     * @param b another ObjectDetection that you want to check for bounding
     * @return a `bool`, `true` if `a` and `b` collide, `false`, otherwise
     * @note https://en.wikipedia.org/wiki/Hyperplane_separation_theorem
     * @warning THIS DOES NOT HANDLE ROTATION, SMALL ROTATED OBJECTS MIGHT BE MISSED FOR FUSION
     */
    bool is_intersecting(const ObjectDetection& a, const ObjectDetection& b);

    /**
     * @brief an implementation of union-find using `is_intersecting` as our predicate
     * @param indx is the index you want to find the parent / root of
     */
    std::size_t union_find(std::size_t indx);

    /**
     * @brief given two indices, merges them so that they have the same root. This is used to
     * process all the merge requests created by `identify_collisions()`
     * @param i an index into `inferences` for an object that should set-join with `j`
     * @param j an index into `inferences` for an object that should set-join with `i`
     */
    void set_unite(std::size_t i, std::size_t j);

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

    //A map which keys {modality, class} -> confidence for weighted averaging of class
    std::unordered_map<std::pair<std::string, std::string>, double, PairHash> class_confidence_map {};

    //A map which keys {modality} -> confidence for weighted averaging of pos
    std::unordered_map<std::string, double> position_confidence_map {};

    //A map which keys {modality} -> confidence for weighted averaging of dimensions
    std::unordered_map<std::string, double> dimension_confidence_map {};

    //Default values for unmaped keys
    double class_conf_default = 1;
    double pos_conf_default = 1;
    double dim_conf_default = 1;


    //The total size of the input, a useful constant to have
    std::size_t input_size;

/*=====================================================================================================
                                    Input and Output Structures
=====================================================================================================*/

    //The bulk storage where objects are kept
    std::vector<ObjectDetection> inferences {};

    //A Threadsafe queue that holds the results of fusion
    TSVector<ObjectDetection> output {};

    //A flag that is thrown whenever an exception is thrown in the fusion engine, hopefully never
    bool good_flag = true;

    //A buffer to dump error messages
    std::string error_buffer {};

/*=====================================================================================================
                                Disjoint Set Data and Structures
=====================================================================================================*/

    //The flat map of parents used in Union Find
    std::vector<std::size_t> parents;

    //Create a queue to manage the union merges found
    TSVector<std::pair<std::size_t, std::size_t>> merges;

    //Create a map which keys roots to the children that are within it, these roots form the clusters
    std::unordered_map<std::size_t, std::vector<std::size_t>> clusters {};

}; //End definition for Fuser Class

} //End namespace fusion system