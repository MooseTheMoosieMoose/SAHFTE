cmake_minimum_required(VERSION 3.25)

project(sahfe VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB_RECURSE PROJECT_SOURCES "source/*.cpp")

add_executable(sahfe ${PROJECT_SOURCES})

target_include_directories(sahfe PRIVATE inc lib)