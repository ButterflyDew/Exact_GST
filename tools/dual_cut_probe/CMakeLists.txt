add_executable(gst_dual_cut_probe
    dual_cut_probe.cpp
    ${CMAKE_SOURCE_DIR}/graph_io.cpp
    ${CMAKE_SOURCE_DIR}/query_io.cpp
    ${CMAKE_SOURCE_DIR}/query_feasibility.cpp
    ${CMAKE_SOURCE_DIR}/methods/Test/test19_future_bounds.cpp
)
target_include_directories(gst_dual_cut_probe PRIVATE ${CMAKE_SOURCE_DIR})
