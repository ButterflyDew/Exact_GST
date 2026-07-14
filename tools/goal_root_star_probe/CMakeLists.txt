add_executable(gst_goal_root_star_probe
    goal_root_star_probe.cpp
    ${CMAKE_SOURCE_DIR}/graph_io.cpp
    ${CMAKE_SOURCE_DIR}/query_io.cpp
    ${CMAKE_SOURCE_DIR}/methods/DPBF/dpbf_solver.cpp
)

target_include_directories(gst_goal_root_star_probe PRIVATE ${CMAKE_SOURCE_DIR})
