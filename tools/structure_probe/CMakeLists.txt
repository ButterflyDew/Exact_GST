add_executable(gst_structure_probe
    structure_probe.cpp
    ${CMAKE_SOURCE_DIR}/graph_io.cpp
    ${CMAKE_SOURCE_DIR}/query_io.cpp
)
target_include_directories(gst_structure_probe PRIVATE ${CMAKE_SOURCE_DIR})
