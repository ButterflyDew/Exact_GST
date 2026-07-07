cmake_minimum_required(VERSION 3.16)
project(GST_Framework CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 在 MSVC 下强制按 UTF-8 解析源码，避免中文注释触发 C4819 及连锁语法误判。
if(MSVC)
    add_compile_options(/utf-8)
endif()

set(GST_COMMON_SOURCES
    main.cpp
    graph_io.cpp
    query_io.cpp
    query_feasibility.cpp
    output_manager.cpp
    output_naming.cpp
    memory_usage.cpp
    methods/DPBF/dpbf_solver.cpp
    methods/Half_DPBF/half_dpbf_solver.cpp
    methods/PrunedDP/pruned_dp_solver.cpp
    methods/Test/test16.cpp
    methods/Test/test17.cpp
    methods/Test/test18.cpp
)

add_executable(gst_dpbf_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_dpbf_main PRIVATE GST_DEFAULT_METHOD="DPBF")

add_executable(gst_half_dpbf_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_half_dpbf_main PRIVATE GST_DEFAULT_METHOD="Half_DPBF")

add_executable(gst_pruned_dp_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_pruned_dp_main PRIVATE GST_DEFAULT_METHOD="PrunedDP")

add_executable(gst_test16_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test16_main PRIVATE GST_DEFAULT_METHOD="Test16")

add_executable(gst_test17_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test17_main PRIVATE GST_DEFAULT_METHOD="Test17")

add_executable(gst_test18_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test18_main PRIVATE GST_DEFAULT_METHOD="Test18")

if(WIN32)
    foreach(target_name
            gst_dpbf_main
            gst_half_dpbf_main
            gst_pruned_dp_main
            gst_test16_main
            gst_test17_main
            gst_test18_main)
        target_link_libraries(${target_name} PRIVATE psapi)
    endforeach()
endif()

add_subdirectory(tools/random_compare)
add_subdirectory(tools/snapshot_benchmark)
