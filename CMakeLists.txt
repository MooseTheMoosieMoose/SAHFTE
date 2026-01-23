#
# CMakeLists for project wide orchestration of builds
#



# file(GLOB_RECURSE PROJECT_SOURCES "source/*.cpp")

# add_executable(sahfte ${PROJECT_SOURCES})

# target_include_directories(sahfte PRIVATE inc lib)

# target_link_libraries(sahfte PRIVATE nlohmann_json::nlohmann_json)

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

#Then the CPP test front end
add_subdirectory(cpp_fe)