cmake_minimum_required(VERSION 3.25)

project(sahfte VERSION 1.0 LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB_RECURSE PROJECT_SOURCES "source/*.cpp")

add_executable(sahfte ${PROJECT_SOURCES})

target_include_directories(sahfte PRIVATE inc lib)

target_link_libraries(sahfte PRIVATE nlohmann_json::nlohmann_json)