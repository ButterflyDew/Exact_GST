add_executable(gst_paid_block_completion_probe
    paid_block_completion_probe.cpp
    ${CMAKE_SOURCE_DIR}/graph_io.cpp
    ${CMAKE_SOURCE_DIR}/query_io.cpp
    ${CMAKE_SOURCE_DIR}/query_feasibility.cpp
    ${CMAKE_SOURCE_DIR}/methods/Test/test19_future_bounds.cpp)
target_include_directories(gst_paid_block_completion_probe PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(gst_paid_block_completion_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools/paid_block_completion_probe)
