add_executable(gst_paid_half_state_probe paid_half_state_probe.cpp)
target_include_directories(gst_paid_half_state_probe PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(gst_paid_half_state_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools/paid_half_state_probe)

