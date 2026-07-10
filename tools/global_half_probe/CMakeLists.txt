add_executable(gst_global_half_probe
    global_half_probe.cpp
    ${CMAKE_SOURCE_DIR}/graph_io.cpp
    ${CMAKE_SOURCE_DIR}/query_io.cpp
    ${CMAKE_SOURCE_DIR}/methods/DPBF/dpbf_solver.cpp
    ${CMAKE_SOURCE_DIR}/methods/Test/test19_future_bounds.cpp
)
target_include_directories(gst_global_half_probe PRIVATE ${CMAKE_SOURCE_DIR})
