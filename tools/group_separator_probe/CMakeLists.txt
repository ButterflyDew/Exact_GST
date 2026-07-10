add_executable(gst_group_separator_probe
    group_separator_probe.cpp
    ${CMAKE_SOURCE_DIR}/graph_io.cpp
    ${CMAKE_SOURCE_DIR}/query_io.cpp
)
target_include_directories(gst_group_separator_probe PRIVATE ${CMAKE_SOURCE_DIR})
