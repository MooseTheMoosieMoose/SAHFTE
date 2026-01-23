#
# CMakeLists to build the cpp front end
#

# add the main file to build
add_executable(cpp_fe src/main.cpp)

#Include the Nlohmann JSON install thats a part of the container
target_include_directories(cpp_fe PRIVATE /usr/local/include/nlohmann)

#Link the project to the core library
target_link_libraries(cpp_fe PRIVATE core_lib)