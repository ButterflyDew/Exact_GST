cmake_minimum_required(VERSION 3.16)
project(GST_RandomCompare CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gst_random_compare random_compare.cpp)
set_target_properties(gst_random_compare PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
