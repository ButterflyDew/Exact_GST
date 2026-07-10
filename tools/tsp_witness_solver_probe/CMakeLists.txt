add_executable(gst_tsp_witness_solver_probe
    witness_main.cpp
    witness_solver.cpp
    ${CMAKE_SOURCE_DIR}/graph_io.cpp
    ${CMAKE_SOURCE_DIR}/query_io.cpp
    ${CMAKE_SOURCE_DIR}/query_feasibility.cpp
    ${CMAKE_SOURCE_DIR}/methods/DPBF/dpbf_solver.cpp
    ${CMAKE_SOURCE_DIR}/methods/Release/release_v1.cpp
)
target_include_directories(gst_tsp_witness_solver_probe PRIVATE ${CMAKE_SOURCE_DIR})

