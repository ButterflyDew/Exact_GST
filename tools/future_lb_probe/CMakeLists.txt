add_executable(gst_future_lb_probe
    future_lb_probe.cpp
    ${CMAKE_SOURCE_DIR}/methods/DPBF/dpbf_solver.cpp
)
target_include_directories(gst_future_lb_probe PRIVATE ${CMAKE_SOURCE_DIR})
