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

#Make the code position independent which is a python requirement
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#Build the library first
add_subdirectory(lib)

#Check if we are building for Python bindings
option(BUILD_PYTHON_BINDINGS "Build the pybind11 Python extension" OFF)
if(BUILD_PYTHON_BINDINGS OR (DEFINED ENV{PY_BINDS} AND NOT "$ENV{PY_BINDS}" STREQUAL ""))
    find_package(pybind11 CONFIG REQUIRED)
    add_subdirectory(pybind)
endif()

#Build the test front end if necessary
if(DEFINED ENV{CPP_FE} AND NOT "$ENV{CPP_FE}" STREQUAL "")
    add_subdirectory(cpp_fe)
endif()