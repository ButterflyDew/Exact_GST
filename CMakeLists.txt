cmake_minimum_required(VERSION 3.16)
project(GST_Framework CXX)

find_package(Threads REQUIRED)

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
    methods/Release/release_v3.cpp
    methods/Release/release_v4.cpp
    methods/Release/release_v5.cpp
    methods/Release/release_v6.cpp
    methods/Common/anchor_junction_upper.cpp
    methods/Test/test16.cpp
    methods/Test/test17.cpp
    methods/Test/test18.cpp
    methods/Test/test19.cpp
    methods/Test/test19_future_bounds.cpp
    methods/Test/test21_anchor_half.cpp
    methods/Test/test80_anchor_progressive.cpp
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

add_executable(gst_release_v3_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_release_v3_main PRIVATE GST_DEFAULT_METHOD="ReleaseV3")

add_executable(gst_release_v4_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_release_v4_main PRIVATE GST_DEFAULT_METHOD="ReleaseV4")

add_executable(gst_release_v5_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_release_v5_main PRIVATE GST_DEFAULT_METHOD="ReleaseV5")

add_executable(gst_release_v6_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_release_v6_main PRIVATE GST_DEFAULT_METHOD="ReleaseV6")

add_executable(gst_test16_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test16_main PRIVATE GST_DEFAULT_METHOD="Test16")

add_executable(gst_test17_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test17_main PRIVATE GST_DEFAULT_METHOD="Test17")

add_executable(gst_test18_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test18_main PRIVATE GST_DEFAULT_METHOD="Test18")

add_executable(gst_test19_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test19_main PRIVATE GST_DEFAULT_METHOD="Test19")

add_executable(gst_test21_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test21_main PRIVATE GST_DEFAULT_METHOD="Test21")

add_executable(gst_test80_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test80_main PRIVATE GST_DEFAULT_METHOD="Test80")

add_executable(gst_test80_progress_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test80_progress_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1)

add_executable(gst_test80_d1_probe_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test80_d1_probe_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D1_PROBE=1)

add_executable(gst_test80_d2_probe_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test80_d2_probe_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D2_PROBE=1)

add_executable(gst_test80_d3_probe_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test80_d3_probe_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D3_PROBE=1)

add_executable(gst_test80_anchor_facility_probe_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test80_anchor_facility_probe_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D3_PROBE=1
    GST_TEST80_ANCHOR_FACILITY_PROBE=1)

add_executable(gst_test80_anchor_facility_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test80_anchor_facility_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_ANCHOR_FACILITY=1)

add_executable(gst_test80_anchor_facility_d3_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test80_anchor_facility_d3_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D3_PROBE=1
    GST_TEST80_ANCHOR_FACILITY=1)

add_executable(gst_test80_anchor_facility_d4_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test80_anchor_facility_d4_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D4_PROBE=1
    GST_TEST80_ANCHOR_FACILITY=1)

add_executable(gst_test80_anchor_tree_probe_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test80_anchor_tree_probe_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D3_PROBE=1
    GST_TEST80_ANCHOR_FACILITY_PROBE=1
    GST_TEST80_ANCHOR_TREE_FACILITY_PROBE=1)

add_executable(gst_test80_anchor_tree_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test80_anchor_tree_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1)

add_executable(gst_test80_anchor_tree_d3_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test80_anchor_tree_d3_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D3_PROBE=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1)

add_executable(gst_test142_adjoint_anchor_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test142_adjoint_anchor_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ADJOINT_ANCHOR=1)

add_executable(gst_test142_adjoint_anchor_progress_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test142_adjoint_anchor_progress_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ADJOINT_ANCHOR=1)

add_executable(gst_test144_adjoint_eager_boundary_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test144_adjoint_eager_boundary_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1)

add_executable(gst_test145_transposed_terminal_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test145_transposed_terminal_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

# Prefix profilers use exactly the Test145 configuration, then stop at a
# natural ordinary-D layer boundary. They are diagnostics, not solver variants.
add_executable(gst_test145_d2_profile_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test145_d2_profile_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D2_PROBE=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

add_executable(gst_test145_d3_profile_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test145_d3_profile_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D3_PROBE=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

add_executable(gst_test145_no_tree_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test145_no_tree_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

add_executable(gst_test145_no_tree_d3_profile_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test145_no_tree_d3_profile_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D3_PROBE=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

add_executable(gst_test146_amortized_anchor_tree_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test146_amortized_anchor_tree_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ANCHOR_TREE_AMORTIZED=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

add_executable(gst_test146_amortized_anchor_tree_d3_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test146_amortized_anchor_tree_d3_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D3_PROBE=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ANCHOR_TREE_AMORTIZED=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

add_executable(gst_test146_amortized_anchor_tree_d2_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test146_amortized_anchor_tree_d2_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D2_PROBE=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ANCHOR_TREE_AMORTIZED=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

add_executable(gst_test145_no_dual_d2_profile_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test145_no_dual_d2_profile_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D2_PROBE=1
    GST_TEST80_NO_DUAL_CUT=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

# Online dual construction in one-group units. Every prefix is already a
# feasible lower bound; accumulated search work pays for later prefixes.
add_executable(gst_test152_progressive_dual_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test152_progressive_dual_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESSIVE_DUAL=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ANCHOR_TREE_AMORTIZED=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

add_executable(gst_test149_changed_arc_dual_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test149_changed_arc_dual_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_DUAL_CHANGED_ARC_SEEDS=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ANCHOR_TREE_AMORTIZED=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

add_executable(gst_test149_changed_arc_dual_d2_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test149_changed_arc_dual_d2_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D2_PROBE=1
    GST_DUAL_CHANGED_ARC_SEEDS=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ANCHOR_TREE_AMORTIZED=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

add_executable(gst_test149_changed_arc_dual_d3_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test149_changed_arc_dual_d3_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST80_PROGRESS=1
    GST_TEST80_D3_PROBE=1
    GST_DUAL_CHANGED_ARC_SEEDS=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ANCHOR_TREE_AMORTIZED=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

# Test157 uses one solver source and one-difference compile-time stages. M0 is
# the Iwata-style global half-state reference; M1-M3 isolate the paper-core
# evaluation changes; M4-M5 add production pruning and dual scheduling.
add_executable(gst_test157_m0_iwata_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test157_m0_iwata_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST157_ABLATION_STAGE=0
    GST_TEST157_IWATA_M0=1)

add_executable(gst_test157_m1_full_a_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test157_m1_full_a_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST157_ABLATION_STAGE=1)

add_executable(gst_test157_m2_adjoint_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test157_m2_adjoint_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST157_ABLATION_STAGE=2
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1)

add_executable(gst_test157_m3_transpose_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test157_m3_transpose_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST157_ABLATION_STAGE=3
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

add_executable(gst_test157_m4_pruning_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test157_m4_pruning_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST157_ABLATION_STAGE=4
    GST_DUAL_CHANGED_ARC_SEEDS=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ANCHOR_TREE_AMORTIZED=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

add_executable(gst_test157_m5_progressive_dual_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test157_m5_progressive_dual_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST157_ABLATION_STAGE=5
    GST_TEST80_PROGRESSIVE_DUAL=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ANCHOR_TREE_AMORTIZED=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

# Test158 isolates the strict root-irreducible branch basis. It keeps the
# frozen M5 algorithm but publishes every stored ordinary value as a branch.
add_executable(gst_test158_all_branches_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test158_all_branches_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST157_ABLATION_STAGE=5
    GST_TEST158_ALL_BRANCHES=1
    GST_TEST80_PROGRESSIVE_DUAL=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ANCHOR_TREE_AMORTIZED=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

# Same algorithm as the frozen M5 candidate, with representation invariants
# checked after every ordinary, anchored, and backward row is stored.
add_executable(gst_test157_production_audit_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test157_production_audit_main PRIVATE
    GST_DEFAULT_METHOD="Test80"
    GST_TEST157_ABLATION_STAGE=5
    GST_TEST157_VALIDATE_ROWS=1
    GST_TEST80_PROGRESSIVE_DUAL=1
    GST_DUAL_CHANGED_ARC_SEEDS=1
    GST_TEST80_ANCHOR_FACILITY=1
    GST_TEST80_ANCHOR_TREE_FACILITY=1
    GST_TEST80_ANCHOR_TREE_AMORTIZED=1
    GST_TEST80_ADJOINT_ANCHOR=1
    GST_TEST80_ADJOINT_EAGER_BOUNDARY=1
    GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL=1)

foreach(target_name
        gst_dpbf_main
        gst_half_dpbf_main
        gst_pruned_dp_main
        gst_release_v1_main
        gst_release_v2_main
        gst_release_v3_main
        gst_release_v4_main
        gst_release_v5_main
        gst_release_v6_main
        gst_test16_main
        gst_test17_main
        gst_test18_main
        gst_test19_main
        gst_test21_main
        gst_test80_main
        gst_test80_progress_main
        gst_test80_d1_probe_main
        gst_test80_d2_probe_main
        gst_test80_d3_probe_main
        gst_test80_anchor_facility_probe_main
        gst_test80_anchor_facility_main
        gst_test80_anchor_facility_d3_main
        gst_test80_anchor_facility_d4_main
        gst_test80_anchor_tree_probe_main
        gst_test80_anchor_tree_main
        gst_test80_anchor_tree_d3_main
        gst_test142_adjoint_anchor_main
        gst_test142_adjoint_anchor_progress_main
        gst_test144_adjoint_eager_boundary_main
        gst_test145_transposed_terminal_main
        gst_test145_d2_profile_main
        gst_test145_d3_profile_main
        gst_test145_no_tree_main
        gst_test145_no_tree_d3_profile_main
        gst_test146_amortized_anchor_tree_main
        gst_test146_amortized_anchor_tree_d2_main
        gst_test146_amortized_anchor_tree_d3_main
        gst_test145_no_dual_d2_profile_main
        gst_test152_progressive_dual_main
        gst_test149_changed_arc_dual_main
        gst_test149_changed_arc_dual_d2_main
        gst_test149_changed_arc_dual_d3_main
        gst_test157_m0_iwata_main
        gst_test157_m1_full_a_main
        gst_test157_m2_adjoint_main
        gst_test157_m3_transpose_main
        gst_test157_m4_pruning_main
        gst_test157_m5_progressive_dual_main
        gst_test158_all_branches_main
        gst_test157_production_audit_main)
    target_link_libraries(${target_name} PRIVATE Threads::Threads)
    if(WIN32)
        target_link_libraries(${target_name} PRIVATE psapi)
    endif()
endforeach()

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
add_subdirectory(tools/goal_root_star_probe)
add_subdirectory(tools/paid_half_state_probe)
add_subdirectory(tools/paid_pair_profile_probe)
add_subdirectory(tools/paid_block_completion_probe)
add_subdirectory(tools/paid_core_growth_probe)
add_subdirectory(tools/adjoint_anchor_probe)
add_subdirectory(tools/test157_core_verifier)
add_subdirectory(tools/gpu4gst_data)
