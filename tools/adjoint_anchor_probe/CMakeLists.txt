cmake_minimum_required(VERSION 3.16)
project(GST_AdjointAnchorProbe CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gst_adjoint_anchor_probe adjoint_anchor_probe.cpp)
set_target_properties(gst_adjoint_anchor_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
