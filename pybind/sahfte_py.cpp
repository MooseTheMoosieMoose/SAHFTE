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
 * @brief this file has the Pybind11 code to generate the Python bindings for the fuser
 * @copyright `26, Lisenced under whatever Paccar Inc.'s requirements are
 */

#include <pybind11/pybind11.h>     //Gets us the Pybind macros to create the Python interface
#include <pybind11/stl.h>          //Interopts with the STL
#include <map>                     //We have to interopt between map and unordered map :(

#include "common.hpp"              //Allows us to port some utility functions
#include "fusion_system.hpp"       //The main thing we are porting

//Scope out pybind11 to be less verbose
namespace py = pybind11;

//Put our fusion system in scope
using namespace FusionSystem;

/*=====================================================================================================
                                        Bindings
=====================================================================================================*/


PYBIND11_MODULE(sahfte, m) {
    /**
     * @brief add module wide docs
     */
    m.doc() = "The module for FUSION!";

    /**
     * @brief bindings for the Vec3D class with rw members
     */
    py::class_<Vec3D>(
        m, 
        "Vec3D", 
        R"doc(
        Vec3D is essentially a named wrapper of double[3], used to pass around 3D values
        like position, rotation, dimensions, etc

        Attributes:
            x (float): the x value (+forwards, -backwards)
            y (float): the y value (+left, -right)
            z (float): the z value (+up, -down)
        )doc")

        .def(py::init<double, double, double>(), 
            py::arg("x") = 0,
            py::arg("y") = 0,
            py::arg("z") = 0
        )
        .def_readwrite(
            "x", 
            &Vec3D::x
        )
        .def_readwrite(
            "y", 
            &Vec3D::y
        )
        .def_readwrite(
            "z", 
            &Vec3D::z
        );

    /**
     * @brief bindings for the object detection class with read only members
     */
    py::class_<Fuser::ObjectDetection>(
        m, 
        "ObjectDetection", 
        R"doc(
        ObjectDetection is a wrapper around an input inference, the intermediate
        values the fuser processes, and the final detections it makes. It is not
        explicitly defined for construction on the python end, but can be read
        as it comes out the fusion system

        Attributes:
            global_center (Vec3D): the center of the detection in global space (lat, long, alt)
            local_center (Vec3D): the center of the detection in local space (meters from GPS ref)
            dimensions (Vec3D): the size of the detection in meters
            rotation (float): the rotation of the object about the Z-axis on [0, 2pi]
            modality (str): a string with the modality the object detection came from
            class_name (str): the name of the class for the detection
            confidence (float): the confidence of the detection on [0, 1]
            uuid (str): a uuid for the detection, allowing you to track specific items through time 
        )doc")

        .def(py::init([](Vec3D center, Vec3D local, std::size_t z, Vec3D dim, double rot, std::string mod, std::string cls, double conf, std::string uuid) {
            return Fuser::ObjectDetection{center, local, z, dim, rot, mod, cls, conf, uuid};
        }))

        .def_readonly(
            "global_center", 
            &Fuser::ObjectDetection::center
        )
        .def_readonly(
            "local_center", 
            &Fuser::ObjectDetection::local_center
        )
        .def_readonly(
            "dimensions", 
            &Fuser::ObjectDetection::dim
        )
        .def_readonly(
            "rotation", 
            &Fuser::ObjectDetection::rotation
        )
        .def_readonly(
            "modality",
            &Fuser::ObjectDetection::modality
        )
        .def_readonly(
            "class_name", 
            &Fuser::ObjectDetection::class_name
        )
        .def_readonly(
            "confidence", 
            &Fuser::ObjectDetection::det_confidence
        )
        .def_readonly(
            "uuid", 
            &Fuser::ObjectDetection::uuid
        );


    /**
     * @brief bindings for the fusion object
     */
    py::class_<Fuser>(
        m, 
        "Fuser", 
        R"doc(
        The main fusion object for the library. Maintains its own input/output buffers,
        a custom threadpool, and encapsulated error handling. Biases can be added to the fusion
        with `assign_modality_dim_confidence_map()`, `assign_modality_pos_confidence_map()`, and
        `assign_class_confidence_map()`

        1. Construct an instance of the `Fuser` object with a given number of auxillary threads, a bit_depth, a 
        fusion volume and the current origin

        2. Add any extra details needed like `assign_confidence_map()` to tweak the system as needed

        3. In a loop perform the following:
        
        3.a. Ensure your origin is up to date with `set_reference_origin()`

        3.b. Add infrences using the `add_inference()` methods

        3.c. Instruct the system to perform a fusion with the `fuse()` method

        3.d. Check for errors with `is_ok()`, if there are errors read with `get_error()`
        
        3.e. Pop fused outputs using the `get_output()` method
        
        3.f. Flush the input/output buffers with `empty_buffers()`

        Attributes:
            thread_count (int): the number of *auxillary* threads this object should manage, effective thread count is thread_count + 1
            spatial_bit_depth (int): the degree of spatial division to use on [1, 21], higher numbers are better but have diminishing returns
            bounding_volume (Vec3D): the clipping volume in meters, anything inside is kept, everything else is thrown
            starting_origin (Vec3D): the base GPS reference to initially use (lat, long, alt) 
        )doc")

        .def(py::init([](std::size_t threads, uint8_t depth, 
                std::optional<Vec3D> volume, 
                std::optional<Vec3D> origin) {
                return new Fuser(
                    threads, 
                    depth, 
                    volume.value_or(Vec3D{100, 100, 100}), 
                    origin.value_or(Vec3D{0, 0, 0})
                );
            }), 
            py::arg("thread_count") = 1,
            py::arg("spatial_bit_depth") = 8,
            py::arg("bounding_volume") = py::none(), // Stubgen sees 'None'
            py::arg("starting_origin") = py::none()
        )

        .def("reserve_inferences", 
            &Fuser::reserve_inferences, 
            py::arg("count"),
            R"doc(
            Reserves space in the internal inference buffer. This is not strictly necessary but
            can improve performance for the first few fusions.

            Args:
                count (int): the requested buffer size
            )doc")

        .def("add_inference", 
            static_cast<void (Fuser::*)(Vec3D&, Vec3D&, double, std::string&, std::string&, double, std::string&, bool)>(
                &Fuser::add_inference
            ),
            py::arg("pos"), 
            py::arg("dim"), 
            py::arg("rotation"), 
            py::arg("mod_name"), 
            py::arg("class_name"), 
            py::arg("confidence"),
            py::arg("uuid"),
            py::arg("global_position") = true, 
            R"doc(
            Inserts a new inference into the internal buffer that stages for fusion
            
            Args:
                pos (Vec3D): the center of the detection
                dim (Vec3D): the dimensions of the inference in meters
                rotation (float): the rotation of the inference on [0, 2pi]
                modality (str): the modality that this detection came from
                class_name (str): the class name of the inference
                confidence (float): the confidence of the detection on [0, 2pi]
                uuid (str): the UUID of the inference so groups can be tracked
            )doc")

        .def("fuse", 
            &Fuser::fuse, 
            R"doc(
            Performs the actual fusion once all detections are pumped into the system with
            `add_inference()`, once this is complete you should check `is_ok()`, then you can
            start reading fusions off of `get_output()`
            )doc")

        .def("get_output", 
            &Fuser::get_output,
            py::return_value_policy::reference_internal,
            R"doc(
            Get a read-only view of the output buffer, once you have read this you should
            call `empty_buffers()`. If this is empty when you expect it to not be, check
            `is_ok()`

            Returns:
                A read-only view into a list that contains the results of the last call to `fuse()`
            )doc")

        .def("get_output_copy", 
            &Fuser::get_output,
            py::return_value_policy::copy,
            R"doc(
            Get an explict copy of the output buffer. If this is empty when you expect it to not be, check
            `is_ok()`

            Returns:
                A copy of the contents of the output buffer that contains the results of the last call to `fuse()`
                This should generally be preferred over `get_output` to prevent double frees when working with Python
            )doc")

        .def("empty_buffers", 
            &Fuser::empty_buffers, 
            R"doc(
            invokes the destructors on the input and output buffers, but does NOT free the allocations
            used by either buffer. Should be called once per fusion cycle
            )doc")

        .def("assign_class_confidence_map", [](Fuser &self, std::map<std::pair<std::string, std::string>, double> &m, double default_val) {
                // Create the specialized internal map type
                std::unordered_map<std::pair<std::string, std::string>, double, Fuser::PairHash> internal_map;
                
                // Transfer data from the standard map to the specialized one
                for (const auto& [key, value] : m) {
                    internal_map[key] = value;
                }
                
                // Call the actual C++ method
                self.assign_class_confidence_map(std::move(internal_map), default_val);
            },
            py::arg("map"), 
            py::arg("default_val") = 1.0,
            R"doc(
            assigns the internal confidencer map that keys {modality, class} -> weight that is used in fusion for class name

            Args:
                map (dict[tuple[str, str], float]): the bias map
                default_val (float): the value used by the fusion system when theres no matching keys in the map
            )doc")

        .def("assign_modality_pos_confidence_map", 
            &Fuser::assign_modality_pos_confidence_map,
            py::arg("map"),
            py::arg("default_val") = 1, 
            R"doc(
            assigns the internal confidencer map that keys {modality} -> weight that is used in fusion for position

            Args:
                map (dict[str, float]): the bias map
                default_val (float): the value used by the fusion system when theres no matching keys in the map
            )doc")

        .def("assign_modality_dim_confidence_map", 
            &Fuser::assign_modality_dim_confidence_map,
            py::arg("map"),
            py::arg("default_val") = 1, 
            R"doc(
            assigns the internal confidencer map that keys {modality} -> weight that is used in fusion for dimensions

            Args:
                map (dict[str, float]): the bias map
                default_val (float): the value used by the fusion system when theres no matching keys in the map
            )doc")

        .def("set_reference_origin", 
            &Fuser::set_reference_origin,
            py::arg("origin"),
            R"doc(
            Sets the internal reference origin that local and global positions are calculated off of

            Args:
                origin: (Vec3D) the new origin (lat, long, altitude)
            )doc")

        .def("is_ok", 
            &Fuser::is_ok,
            R"doc(
            Checks an internal boolean flag for if there was an error with the last fusion. Errors
            are checked with `get_error()`

            Returns:
                bool: true if everything is fine, false if theres an error
            )doc")

        .def("get_error", 
            &Fuser::get_error,
            py::return_value_policy::copy,
            R"doc(
            Gets the value inside the internal error message buffer, then clears it. Checking this
            twice will only return a value on the first call. Checking `is_ok()` will say if theres
            a value in here.

            Returns:
                str: the error message from the last fusion
            )doc");
}