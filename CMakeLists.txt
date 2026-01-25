#
# CMakeLists for project wide orchestration of builds
#

#Enforce version, declare project
cmake_minimum_required(VERSION 3.25)
project(sahfte_project VERSION 1.0 LANGUAGES CXX)

#Enforce this is a CPP20 project
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#Dump binaries to a bin folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#Build the library first
add_subdirectory(lib)

#Test to see what we should be building
if(DEFINED ENV{PY_BINDS} AND NOT "$ENV{PY_BINDS}" STREQUAL "")
    add_subdirectory(pybind)
endif()

if(DEFINED ENV{CPP_FE} AND NOT "$ENV{CPP_FE}" STREQUAL "")
    add_subdirectory(cpp_fe)
endif()