cmake_minimum_required(VERSION 3.16)
project(GST_Framework CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Keep source parsing stable on MSVC.
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Make optimized release builds explicit. MSVC Release already defaults to
# /O2 /Ob2, but keeping the flags here prevents timing runs from depending on
# generator defaults after a clean configure.
if(MSVC)
    foreach(config_name RELEASE RELWITHDEBINFO)
        if(NOT CMAKE_CXX_FLAGS_${config_name} MATCHES "(^| )/O[123x]")
            string(APPEND CMAKE_CXX_FLAGS_${config_name} " /O2")
        endif()
    endforeach()
    if(NOT CMAKE_CXX_FLAGS_RELEASE MATCHES "(^| )/Ob")
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Ob2")
    endif()
else()
    foreach(config_name RELEASE RELWITHDEBINFO)
        if(NOT CMAKE_CXX_FLAGS_${config_name} MATCHES "(^| )-O[123s]")
            string(APPEND CMAKE_CXX_FLAGS_${config_name} " -O2")
        endif()
    endforeach()
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
    methods/Release/release_v1.cpp
    methods/Release/release_v2.cpp
    methods/Test/test16.cpp
    methods/Test/test17.cpp
    methods/Test/test18.cpp
    methods/Test/test19.cpp
    methods/Test/test19_future_bounds.cpp
)

add_executable(gst_dpbf_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_dpbf_main PRIVATE GST_DEFAULT_METHOD="DPBF")

add_executable(gst_half_dpbf_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_half_dpbf_main PRIVATE GST_DEFAULT_METHOD="Half_DPBF")

add_executable(gst_pruned_dp_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_pruned_dp_main PRIVATE GST_DEFAULT_METHOD="PrunedDP")

add_executable(gst_release_v1_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_release_v1_main PRIVATE GST_DEFAULT_METHOD="ReleaseV1")

add_executable(gst_release_v2_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_release_v2_main PRIVATE GST_DEFAULT_METHOD="ReleaseV2")

add_executable(gst_test16_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test16_main PRIVATE GST_DEFAULT_METHOD="Test16")

add_executable(gst_test17_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test17_main PRIVATE GST_DEFAULT_METHOD="Test17")

add_executable(gst_test18_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test18_main PRIVATE GST_DEFAULT_METHOD="Test18")

add_executable(gst_test19_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test19_main PRIVATE GST_DEFAULT_METHOD="Test19")

if(WIN32)
    foreach(target_name
            gst_dpbf_main
            gst_half_dpbf_main
            gst_pruned_dp_main
            gst_release_v1_main
            gst_release_v2_main
            gst_test16_main
            gst_test17_main
            gst_test18_main
            gst_test19_main)
        target_link_libraries(${target_name} PRIVATE psapi)
    endforeach()
endif()

add_subdirectory(tools/random_compare)
add_subdirectory(tools/snapshot_benchmark)
add_subdirectory(tools/structure_probe)
add_subdirectory(tools/future_lb_probe)
add_subdirectory(tools/group_separator_probe)
add_subdirectory(tools/global_half_probe)
add_subdirectory(tools/group_replacement_probe)
add_subdirectory(tools/pair_forest_probe)
add_subdirectory(tools/tsp_witness_solver_probe)
add_subdirectory(tools/distance_epoch_solver_probe)
add_subdirectory(tools/dual_cut_probe)
