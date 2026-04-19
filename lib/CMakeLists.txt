#
# CMakeLists to build the cpp core library
#

#Declare the library
add_library(core_lib STATIC
    src/fusion_system.cpp
    src/common.cpp
    src/threadpool.cpp
)

option(USE_STD_FORMAT "Enable use of std::format" OFF)

if (USE_STD_FORMAT)
    target_compile_definitions(core_lib PRIVATE USE_STD_FORMAT)
endif()

#Make a public interface for the headers of this library
target_include_directories(core_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)