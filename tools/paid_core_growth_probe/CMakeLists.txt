add_executable(gst_paid_core_growth_probe
    paid_core_growth_probe.cpp
    ${CMAKE_SOURCE_DIR}/graph_io.cpp
    ${CMAKE_SOURCE_DIR}/query_io.cpp
    ${CMAKE_SOURCE_DIR}/query_feasibility.cpp)
target_include_directories(gst_paid_core_growth_probe PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(gst_paid_core_growth_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools/paid_core_growth_probe)
