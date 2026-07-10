add_executable(gst_distance_epoch_solver_probe
    distance_main.cpp
    distance_solver.cpp
    ${CMAKE_SOURCE_DIR}/graph_io.cpp
    ${CMAKE_SOURCE_DIR}/query_io.cpp
    ${CMAKE_SOURCE_DIR}/query_feasibility.cpp
    ${CMAKE_SOURCE_DIR}/memory_usage.cpp
    ${CMAKE_SOURCE_DIR}/methods/DPBF/dpbf_solver.cpp
    ${CMAKE_SOURCE_DIR}/methods/Release/release_v1.cpp
)
target_include_directories(gst_distance_epoch_solver_probe PRIVATE ${CMAKE_SOURCE_DIR})
