
#Create the python module
pybind11_add_module(sahfte sahfte_py.cpp)

#Make it so that we dont get extra gunk at the end of the module
set_target_properties(sahfte PROPERTIES SUFFIX ".so")

#Bind the module to the core lib that holds everything
target_link_libraries(sahfte PRIVATE core_lib)

#Make sure the bindings build with visibility of the core lib headers
target_include_directories(sahfte PRIVATE ../lib/inc)

#Force the final pybindings to get put in a folder to be installed
set_target_properties(sahfte PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/sahfte"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/sahfte"
)

option(USE_INTEL_TBB "Enable use of Intel Threading Building Blocks" OFF)

if(USE_INTEL_TBB)
    #Add threading blocks for cross build
    find_package(TBB REQUIRED)
    target_link_libraries(sahfte PRIVATE TBB::tbb)
endif()

add_custom_command(
    TARGET sahfte POST_BUILD
    COMMAND ${PYTHON_EXECUTABLE} -m pybind11_stubgen sahfte 
            --output-dir ${CMAKE_SOURCE_DIR}/sahfte
    WORKING_DIRECTORY $<TARGET_FILE_DIR:sahfte>
    COMMENT "Generating type stubs for sahfte..."
)