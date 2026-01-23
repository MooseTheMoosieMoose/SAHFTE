#
# CMakeLists to build the cpp core library
#

#Declare the library
add_library(core_lib STATIC
    src/fusion_system.cpp
    src/common.cpp
    src/threadpool.cpp
)

#Make a public interface for the headers of this library
target_include_directories(core_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)