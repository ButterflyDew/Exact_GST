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
    answer_tree.cpp
    methods/DPBF/dpbf_solver.cpp
    methods/Half_DPBF/half_dpbf_solver.cpp
    methods/PrunedDP/pruned_dp_solver.cpp
    methods/Test/test1.cpp
    methods/Test/test2.cpp
    methods/Test/test3.cpp
    methods/Test/test4.cpp
    methods/Test/test5.cpp
    methods/Test/test6.cpp
    methods/Test/test7.cpp
    methods/Test/test8.cpp
    methods/Test/test9.cpp
    methods/Test/test10.cpp
    methods/Test/test11.cpp
    methods/Test/test12.cpp
)

add_executable(gst_dpbf_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_dpbf_main PRIVATE GST_DEFAULT_METHOD="DPBF")

add_executable(gst_half_dpbf_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_half_dpbf_main PRIVATE GST_DEFAULT_METHOD="Half_DPBF")

add_executable(gst_pruned_dp_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_pruned_dp_main PRIVATE GST_DEFAULT_METHOD="PrunedDP")

add_executable(gst_test1_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test1_main PRIVATE GST_DEFAULT_METHOD="Test1")

add_executable(gst_test2_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test2_main PRIVATE GST_DEFAULT_METHOD="Test2")

add_executable(gst_test3_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test3_main PRIVATE GST_DEFAULT_METHOD="Test3")

add_executable(gst_test4_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test4_main PRIVATE GST_DEFAULT_METHOD="Test4")

add_executable(gst_test5_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test5_main PRIVATE GST_DEFAULT_METHOD="Test5")

add_executable(gst_test6_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test6_main PRIVATE GST_DEFAULT_METHOD="Test6")

add_executable(gst_test7_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test7_main PRIVATE GST_DEFAULT_METHOD="Test7")

add_executable(gst_test8_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test8_main PRIVATE GST_DEFAULT_METHOD="Test8")

add_executable(gst_test9_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test9_main PRIVATE GST_DEFAULT_METHOD="Test9")

add_executable(gst_test10_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test10_main PRIVATE GST_DEFAULT_METHOD="Test10")

add_executable(gst_test11_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test11_main PRIVATE GST_DEFAULT_METHOD="Test11")

add_executable(gst_test12_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_test12_main PRIVATE GST_DEFAULT_METHOD="Test12")

# 兼容旧命名：默认仍指向 DPBF。
add_executable(gst_main ${GST_COMMON_SOURCES})
target_compile_definitions(gst_main PRIVATE GST_DEFAULT_METHOD="DPBF")
