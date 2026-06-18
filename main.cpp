#include <chrono>
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "graph_io.h"
#include "methods/DPBF/dpbf_solver.h"
#include "methods/Half_DPBF/half_dpbf_solver.h"
#include "methods/PrunedDP/pruned_dp_solver.h"
#include "methods/Test/test1.h"
#include "methods/Test/test2.h"
#include "methods/Test/test3.h"
#include "methods/Test/test4.h"
#include "methods/Test/test5.h"
#include "methods/Test/test6.h"
#include "methods/Test/test7.h"
#include "methods/Test/test8.h"
#include "methods/Test/test9.h"
#include "methods/Test/test10.h"
#include "methods/Test/test11.h"
#include "methods/Test/test12.h"
#include "output_manager.h"
#include "output_naming.h"
#include "query_io.h"

namespace fs = std::filesystem;

namespace
{

gst::methods::dpbf::OutputMode ParseOutputMode(const std::string& mode_str)
{
    if (mode_str.empty() || mode_str == "weight")
    {
        return gst::methods::dpbf::OutputMode::kWeightOnly;
    }
    if (mode_str == "tree" || mode_str == "concrete")
    {
        return gst::methods::dpbf::OutputMode::kConcreteTree;
    }
    if (mode_str == "virtual")
    {
        return gst::methods::dpbf::OutputMode::kVirtualTree;
    }
    throw std::runtime_error("Unknown output mode: " + mode_str + ". use weight|tree|virtual");
}

gst::VirtualRootPolicy ParseRootPolicy(const std::string& policy_str)
{
    if (policy_str.empty() || policy_str == "child_first")
    {
        return gst::VirtualRootPolicy::kMinMaxChildThenHeight;
    }
    if (policy_str == "height_first")
    {
        return gst::VirtualRootPolicy::kMinHeightThenMaxChild;
    }
    throw std::runtime_error("Unknown root policy: " + policy_str + ". use child_first|height_first");
}

std::string OutputModeName(gst::methods::dpbf::OutputMode mode)
{
    if (mode == gst::methods::dpbf::OutputMode::kWeightOnly)
    {
        return "weight";
    }
    if (mode == gst::methods::dpbf::OutputMode::kConcreteTree)
    {
        return "tree";
    }
    return "virtual";
}

std::string FormatDouble(double x, int precision = 10)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << x;
    return oss.str();
}

std::string CurrentTimeString()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &now_time);
#else
    localtime_r(&now_time, &local_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        const std::string graph_selector = (argc >= 2) ? argv[1] : "";
        const std::string mode_str = (argc >= 3) ? argv[2] : "weight";
        const auto output_mode = ParseOutputMode(mode_str);
        const std::string result_base_dir = (argc >= 4) ? argv[3] : "result";
        const std::string debug_base_dir = (argc >= 5) ? argv[4] : "debug";
        const std::string root_policy_str = (argc >= 6) ? argv[5] : "child_first";
        const std::string query_selector = (argc >= 7) ? argv[6] : "";
        const std::string data_root = (argc >= 8) ? argv[7] : "data";
        const int query_begin = (argc >= 9) ? std::stoi(argv[8]) : 1;
        const int query_limit = (argc >= 10) ? std::stoi(argv[9]) : -1;
        const auto root_policy = ParseRootPolicy(root_policy_str);

        const std::string graph_folder = gst::ResolveGraphFolder(data_root, graph_selector);
        const auto graph_name = fs::path(graph_folder).filename().string();
        const std::string method_name =
#ifdef GST_DEFAULT_METHOD
            GST_DEFAULT_METHOD;
#else
            "DPBF";
#endif
        const std::string mode_name = OutputModeName(output_mode);
        const std::string query_file = gst::ResolveQueryFile(graph_folder, query_selector);
        const std::string run_subdir = gst::RunSubdirFromQueryFile(fs::path(query_file));
        const std::string weights_filename = gst::WeightsFilename();
        const std::string stats_filename = gst::StatsFilename(method_name);
        const std::string result_dir =
            (fs::path(result_base_dir) / mode_name / graph_name / method_name / run_subdir).string();
        const std::string debug_dir =
            (fs::path(debug_base_dir) / mode_name / graph_name / method_name / run_subdir).string();
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const std::vector<gst::Query> all_queries = gst::LoadQueriesFromFolder(graph_folder, query_selector);
        if (query_begin < 1 || query_begin > static_cast<int>(all_queries.size()) + 1)
        {
            throw std::runtime_error("query_begin out of range.");
        }
        const int begin_idx = query_begin - 1;
        int end_idx = static_cast<int>(all_queries.size());
        if (query_limit >= 0)
        {
            end_idx = std::min(end_idx, begin_idx + query_limit);
        }
        const std::vector<gst::Query> queries(all_queries.begin() + begin_idx, all_queries.begin() + end_idx);
        gst::OutputManager output_manager(result_dir, debug_dir, weights_filename);

        const bool is_half_dpbf = (method_name == "Half_DPBF");
        const bool is_pruned_dp = (method_name == "PrunedDP");
        const bool is_test1 = (method_name == "Test1");
        const bool is_test2 = (method_name == "Test2");
        const bool is_test3 = (method_name == "Test3");
        const bool is_test4 = (method_name == "Test4");
        const bool is_test5 = (method_name == "Test5");
        const bool is_test6 = (method_name == "Test6");
        const bool is_test7 = (method_name == "Test7");
        const bool is_test8 = (method_name == "Test8");
        const bool is_test9 = (method_name == "Test9");
        const bool is_test10 = (method_name == "Test10");
        const bool is_test11 = (method_name == "Test11");
        const bool is_test12 = (method_name == "Test12");

        std::ostringstream header;
        header << "# Run started=" << CurrentTimeString()
               << " graph=" << graph_name
               << " method=" << method_name
               << " mode=" << mode_name
               << " data_root=" << data_root
               << " graph_folder=" << graph_folder
               << " query_file=" << fs::path(query_file).filename().string()
               << " run_subdir=" << run_subdir
               << " original_query_count=" << all_queries.size()
               << " query_begin=" << query_begin
               << " query_limit=" << query_limit
               << " query_count=" << queries.size()
               << " result_dir=" << result_dir;
        output_manager.BeginResultRun(weights_filename, header.str());
        if (!stats_filename.empty())
        {
            output_manager.BeginResultRun(stats_filename, header.str());
        }

        // 按查询逐个求解并记录运行时间。
        for (int qi = 0; qi < static_cast<int>(queries.size()); ++qi)
        {
            const int query_id = query_begin + qi;
            auto start = std::chrono::steady_clock::now();
            bool feasible = false;
            double best_weight = -1.0;
            std::unique_ptr<gst::AnswerTreeBase> answer;
            gst::methods::half_dpbf::HalfDpbfStats half_stats;
            gst::methods::pruned_dp::PrunedDpStats pruned_stats;
            gst::methods::test1::Test1Stats test1_stats;
            gst::methods::test2::Test2Stats test2_stats;
            gst::methods::test3::Test3Stats test3_stats;
            gst::methods::test4::Test4Stats test4_stats;
            gst::methods::test5::Test5Stats test5_stats;
            gst::methods::test6::Test6Stats test6_stats;
            gst::methods::test7::Test7Stats test7_stats;
            gst::methods::test8::Test8Stats test8_stats;
            gst::methods::test9::Test9Stats test9_stats;
            gst::methods::test10::Test10Stats test10_stats;
            gst::methods::test11::Test11Stats test11_stats;
            gst::methods::test12::Test12Stats test12_stats;
            if (is_half_dpbf)
            {
                auto solve_result =
                    gst::methods::half_dpbf::SolveOneQuery(graph, queries[qi], output_mode, root_policy);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                half_stats = solve_result.stats;
            }
            else if (is_test1)
            {
                auto solve_result = gst::methods::test1::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                test1_stats = std::move(solve_result.stats);
            }
            else if (is_test2)
            {
                auto solve_result = gst::methods::test2::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                test2_stats = std::move(solve_result.stats);
            }
            else if (is_test3)
            {
                auto solve_result = gst::methods::test3::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                test3_stats = std::move(solve_result.stats);
            }
            else if (is_test4)
            {
                auto solve_result = gst::methods::test4::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                test4_stats = std::move(solve_result.stats);
            }
            else if (is_test5)
            {
                auto solve_result = gst::methods::test5::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                test5_stats = std::move(solve_result.stats);
            }
            else if (is_test6)
            {
                auto solve_result = gst::methods::test6::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                test6_stats = std::move(solve_result.stats);
            }
            else if (is_test7)
            {
                auto solve_result = gst::methods::test7::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                test7_stats = std::move(solve_result.stats);
            }
            else if (is_test8)
            {
                auto solve_result = gst::methods::test8::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                test8_stats = std::move(solve_result.stats);
            }
            else if (is_test9)
            {
                auto solve_result = gst::methods::test9::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                test9_stats = std::move(solve_result.stats);
            }
            else if (is_test10)
            {
                auto solve_result = gst::methods::test10::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                test10_stats = std::move(solve_result.stats);
            }
            else if (is_test11)
            {
                auto solve_result = gst::methods::test11::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                test11_stats = std::move(solve_result.stats);
            }
            else if (is_test12)
            {
                auto solve_result = gst::methods::test12::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                test12_stats = std::move(solve_result.stats);
            }
            else if (is_pruned_dp)
            {
                auto solve_result = gst::methods::pruned_dp::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
                pruned_stats = std::move(solve_result.stats);
            }
            else
            {
                auto solve_result = gst::methods::dpbf::SolveOneQuery(graph, queries[qi], output_mode, root_policy);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
            }
            auto end = std::chrono::steady_clock::now();

            double sec = std::chrono::duration<double>(end - start).count();
            std::string weight_str = feasible ? FormatDouble(best_weight) : "-1";
            output_manager.AppendMainResultLine(FormatDouble(sec, 6) + " " + weight_str);

            std::cout << "[Query " << query_id << "] time=" << FormatDouble(sec, 6) << " sec, weight=" << weight_str << "\n";

            if (output_mode != gst::methods::dpbf::OutputMode::kWeightOnly && feasible && answer)
            {
                const std::string mode_name =
                    (output_mode == gst::methods::dpbf::OutputMode::kConcreteTree) ? "tree" : "virtual";
                std::ostringstream answer_text;
                answer_text << header.str() << " query=" << query_id << "\n" << answer->ToText();
                output_manager.WriteAnswerDetail(query_id - 1, mode_name, answer_text.str());
            }

            // Half_DPBF 额外统计：每个查询结束后立即追加写入同一文件。
            if (is_half_dpbf)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " V=" << weight_str
                           << " stage1_total=" << half_stats.stage1_total << " ltV=" << half_stats.lt_v
                           << " ltV2=" << half_stats.lt_v2 << " ltV4=" << half_stats.lt_v4
                           << " ltV8=" << half_stats.lt_v8;
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_pruned_dp)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " best=" << weight_str
                           << " n=" << pruned_stats.n
                           << " m=" << pruned_stats.m
                           << " g=" << pruned_stats.g
                           << " group_vertices=" << pruned_stats.total_group_vertices
                           << " max_group=" << pruned_stats.max_group_size
                           << " build_ms=" << FormatDouble(pruned_stats.build_ms, 3)
                           << " dist_ms=" << FormatDouble(pruned_stats.dist_ms, 3)
                           << " calw_ms=" << FormatDouble(pruned_stats.calw_ms, 3)
                           << " search_ms=" << FormatDouble(pruned_stats.search_ms, 3)
                           << " total_ms=" << FormatDouble(pruned_stats.total_ms, 3)
                           << " init_push=" << pruned_stats.initial_pushes
                           << " pq_push=" << pruned_stats.pq_pushes
                           << " pq_pop=" << pruned_stats.pq_pops
                           << " stale=" << pruned_stats.stale_pops
                           << " finalized=" << pruned_stats.finalized_labels
                           << " cost_ge_best_skip=" << pruned_stats.cost_ge_best_skips
                           << " update_calls=" << pruned_stats.update_calls
                           << " update_finalized_skip=" << pruned_stats.update_finalized_skip
                           << " update_bound_pruned=" << pruned_stats.update_bound_pruned
                           << " update_push=" << pruned_stats.update_pushes
                           << " best_full=" << pruned_stats.best_full_updates
                           << " best_expect=" << pruned_stats.best_expect_updates
                           << " edge_relax=" << pruned_stats.edge_relax_attempts
                           << " merge_enum=" << pruned_stats.merge_submask_attempts
                           << " merge_hit=" << pruned_stats.merge_state_hits
                           << " merge_gate=" << pruned_stats.merge_cost_gate_pass;
                for (int k = 0; k < static_cast<int>(pruned_stats.finalized_by_size.size()); ++k)
                {
                    if (pruned_stats.finalized_by_size[k] == 0 && pruned_stats.update_push_by_size[k] == 0)
                    {
                        continue;
                    }
                    stats_line << " k=" << k
                               << " final=" << pruned_stats.finalized_by_size[k]
                               << " push=" << pruned_stats.update_push_by_size[k];
                }
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test1)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " BestQ=" << weight_str
                           << " valid_total=" << test1_stats.total_valid;
                for (int k = 0; k < static_cast<int>(test1_stats.valid_by_size.size()); ++k)
                {
                    if (test1_stats.total_by_size[k] <= 0)
                    {
                        continue;
                    }
                    stats_line << " k=" << k << " valid=" << test1_stats.valid_by_size[k]
                               << " total=" << test1_stats.total_by_size[k];
                }
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test2)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " best=" << weight_str
                           << " valid_total=" << test2_stats.total_valid;
                for (int k = 0; k < static_cast<int>(test2_stats.valid_by_size.size()); ++k)
                {
                    if (test2_stats.total_by_size[k] <= 0)
                    {
                        continue;
                    }
                    stats_line << " k=" << k << " valid=" << test2_stats.valid_by_size[k]
                               << " total=" << test2_stats.total_by_size[k];
                }
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test3)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " best=" << weight_str
                           << " valid_total=" << test3_stats.total_valid;
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test4)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " best=" << weight_str
                           << " valid_total=" << test4_stats.total_valid;
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test5)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " best=" << weight_str
                           << " valid_total=" << test5_stats.total_valid
                           << " up_subset=" << test5_stats.total_up_subset
                           << " inqueue=" << test5_stats.total_inqueue;
                for (int k = 0; k < static_cast<int>(test5_stats.valid_by_size.size()); ++k)
                {
                    if (test5_stats.total_by_size[k] <= 0)
                    {
                        continue;
                    }
                    stats_line << " k=" << k << " valid=" << test5_stats.valid_by_size[k]
                                << " total=" << test5_stats.total_by_size[k] << " ";
                }
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test6)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " best=" << weight_str
                           << " valid_total=" << test6_stats.total_valid
                           << " up_subset=" << test6_stats.total_up_subset
                           << " inqueue=" << test6_stats.total_inqueue;
                for (int k = 0; k < static_cast<int>(test6_stats.valid_by_size.size()); ++k)
                {
                    if (test6_stats.total_by_size[k] <= 0)
                    {
                        continue;
                    }
                    stats_line << " k=" << k << " valid=" << test6_stats.valid_by_size[k]
                                << " total=" << test6_stats.total_by_size[k] << " ";
                }
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test7)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " best=" << weight_str
                           << " valid_total=" << test7_stats.total_valid
                           << " up_subset=" << test7_stats.total_up_subset
                           << " dense_would=" << test7_stats.dense_up_subset_would
                           << " inqueue=" << test7_stats.total_inqueue
                           << " lb_seed=" << test7_stats.lb_filtered_seed
                           << " lb_relax=" << test7_stats.lb_filtered_relax
                           << " init_ub=" << FormatDouble(test7_stats.initial_upper);
                for (int k = 0; k < static_cast<int>(test7_stats.valid_by_size.size()); ++k)
                {
                    if (test7_stats.total_by_size[k] <= 0)
                    {
                        continue;
                    }
                    stats_line << " k=" << k << " valid=" << test7_stats.valid_by_size[k]
                                << " total=" << test7_stats.total_by_size[k] << " ";
                }
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test8)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " best=" << weight_str
                           << " valid_total=" << test8_stats.total_valid
                           << " up_subset=" << test8_stats.total_up_subset
                           << " inqueue=" << test8_stats.total_inqueue
                           << " merge_scan=" << test8_stats.merge_scan_checks
                           << " merge_dense=" << test8_stats.merge_dense_checks
                           << " active_seed=" << test8_stats.active_seed_vertices
                           << " full_seed=" << test8_stats.full_seed_vertices_would
                           << " lb_calls=" << test8_stats.lb_calls
                           << " pq_push=" << test8_stats.pq_pushes
                           << " pq_pop=" << test8_stats.pq_pops
                           << " relax_try=" << test8_stats.relax_attempts
                           << " relax_ok=" << test8_stats.relax_success
                           << " masks=" << test8_stats.masks_processed
                           << " max_active=" << test8_stats.max_active_vertices
                           << " star_ub=" << FormatDouble(test8_stats.root_star_upper)
                           << " greedy_ub=" << FormatDouble(test8_stats.greedy_upper)
                           << " greedy_pop=" << test8_stats.greedy_pops
                           << " prep_ms=" << FormatDouble(test8_stats.preprocess_ms, 3)
                           << " dp_ms=" << FormatDouble(test8_stats.dp_ms, 3);
                for (int k = 0; k < static_cast<int>(test8_stats.valid_by_size.size()); ++k)
                {
                    if (test8_stats.total_by_size[k] <= 0)
                    {
                        continue;
                    }
                    stats_line << " k=" << k << " valid=" << test8_stats.valid_by_size[k]
                                << " total=" << test8_stats.total_by_size[k]
                                << " active=" << test8_stats.active_by_size[k]
                                << " inq=" << test8_stats.inqueue_by_size[k]
                                << " merge=" << test8_stats.merge_by_size[k] << " ";
                }
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test9)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " best=" << weight_str
                           << " valid_total=" << test9_stats.total_valid
                           << " up_subset=" << test9_stats.total_up_subset
                           << " inqueue=" << test9_stats.total_inqueue
                           << " merge_scan=" << test9_stats.merge_scan_checks
                           << " merge_dense=" << test9_stats.merge_dense_checks
                           << " active_seed=" << test9_stats.active_seed_vertices
                           << " full_seed=" << test9_stats.full_seed_vertices_would
                           << " lb_calls=" << test9_stats.lb_calls
                           << " pq_push=" << test9_stats.pq_pushes
                           << " pq_pop=" << test9_stats.pq_pops
                           << " relax_try=" << test9_stats.relax_attempts
                           << " relax_ok=" << test9_stats.relax_success
                           << " masks=" << test9_stats.masks_processed
                           << " max_active=" << test9_stats.max_active_vertices
                           << " star_ub=" << FormatDouble(test9_stats.root_star_upper)
                           << " greedy_ub=" << FormatDouble(test9_stats.greedy_upper)
                           << " greedy_pop=" << test9_stats.greedy_pops
                           << " prep_ms=" << FormatDouble(test9_stats.preprocess_ms, 3)
                           << " dp_ms=" << FormatDouble(test9_stats.dp_ms, 3)
                           << " dense_one=" << test9_stats.dense_one_table_cells
                           << " dense_dp_h=" << test9_stats.dense_dp_h_cells
                           << " dp_seen=" << test9_stats.dp_first_seen_total
                           << " h_seen=" << test9_stats.h_first_seen_total
                           << " dp_sparse_zero=" << test9_stats.dp_sparse_cells_with_zero
                           << " sparse_dp_h_zero=" << test9_stats.sparse_dp_h_cells_with_zero
                           << " dp_large=" << test9_stats.dp_large_side_cells
                           << " h_large=" << test9_stats.h_large_side_cells
                           << " dead_nxt_skip=" << test9_stats.dead_nxt_skip_would
                           << " dead_h_skip=" << test9_stats.dead_h_skip_would
                           << " t_size_filter=" << test9_stats.t_size_filtered_would
                           << " full_best_hits=" << test9_stats.full_best_only_hits
                           << " full_best_improve=" << test9_stats.full_best_only_improvements
                           << " dp_skip_high=" << test9_stats.dp_store_skipped_high
                           << " h_skip_low=" << test9_stats.h_store_skipped_low
                           << " h_skip_high=" << test9_stats.h_store_skipped_high
                           << " h_skip_full=" << test9_stats.h_store_skipped_full
                           << " dp_try=" << test9_stats.dp_live_attempts
                           << " dp_succ=" << test9_stats.dp_update_success
                           << " dp_first=" << test9_stats.dp_update_first
                           << " dp_improve=" << test9_stats.dp_update_improve
                           << " dp_old=" << test9_stats.dp_attempt_old_finite
                           << " dp_ge_best_try=" << test9_stats.dp_attempt_cand_ge_best
                           << " dp_ge_best_succ=" << test9_stats.dp_success_cand_ge_best
                           << " dp_far_dead_try=" << test9_stats.dp_attempt_cand_far_ge_best
                           << " dp_far_dead_succ=" << test9_stats.dp_success_cand_far_ge_best
                           << " dp_lb_dead_try=" << test9_stats.dp_attempt_cand_lb_ge_best
                           << " dp_lb_dead_succ=" << test9_stats.dp_success_cand_lb_ge_best
                           << " h_try=" << test9_stats.h_live_attempts
                           << " h_succ=" << test9_stats.h_update_success
                           << " h_first=" << test9_stats.h_update_first
                           << " h_improve=" << test9_stats.h_update_improve
                           << " h_old=" << test9_stats.h_attempt_old_set
                           << " h_ge_best_try=" << test9_stats.h_attempt_value_ge_best
                           << " h_ge_best_succ=" << test9_stats.h_success_value_ge_best
                           << " h_far_dead_try=" << test9_stats.h_attempt_value_far_ge_best
                           << " h_far_dead_succ=" << test9_stats.h_success_value_far_ge_best
                           << " h_lb_dead_try=" << test9_stats.h_attempt_value_lb_ge_best
                           << " h_lb_dead_succ=" << test9_stats.h_success_value_lb_ge_best
                           << " full_dom=" << test9_stats.full_best_dominated
                           << " full_dom_improve=" << test9_stats.full_best_dominated_improve
                           << " dp_cur_le_t_try=" << test9_stats.dp_try_cur_le_t
                           << " dp_cur_le_t_succ=" << test9_stats.dp_succ_cur_le_t
                           << " dp_cur_gt_t_try=" << test9_stats.dp_try_cur_gt_t
                           << " dp_cur_gt_t_succ=" << test9_stats.dp_succ_cur_gt_t
                           << " h_cur_le_t_try=" << test9_stats.h_try_cur_le_t
                           << " h_cur_le_t_succ=" << test9_stats.h_succ_cur_le_t
                           << " h_cur_gt_t_try=" << test9_stats.h_try_cur_gt_t
                           << " h_cur_gt_t_succ=" << test9_stats.h_succ_cur_gt_t
                           << " dp_masks=" << test9_stats.dp_mask_count
                           << " h_masks=" << test9_stats.h_mask_count
                           << " root_mask_entries=" << test9_stats.root_mask_entries
                           << " max_root_masks=" << test9_stats.max_root_masks
                           << " peak_window_layer=" << test9_stats.peak_window_layer
                           << " peak_window_dp=" << test9_stats.peak_window_dp_cells
                           << " peak_window_h=" << test9_stats.peak_window_h_cells
                           << " peak_window_dp_h=" << test9_stats.peak_window_dp_h_cells
                           << " peak_lifecycle_layer=" << test9_stats.peak_lifecycle_layer
                           << " peak_lifecycle_dp=" << test9_stats.peak_lifecycle_dp_cells
                           << " peak_lifecycle_h=" << test9_stats.peak_lifecycle_h_cells
                           << " peak_lifecycle_dp_h=" << test9_stats.peak_lifecycle_dp_h_cells;
                for (int k = 0; k < static_cast<int>(test9_stats.dp_finite_by_size.size()); ++k)
                {
                    if (test9_stats.dp_finite_by_size[k] == 0 && test9_stats.h_finite_by_size[k] == 0)
                    {
                        continue;
                    }
                    stats_line << " size=" << k
                               << " dp=" << test9_stats.dp_finite_by_size[k]
                               << " h=" << test9_stats.h_finite_by_size[k]
                               << " dpm=" << test9_stats.dp_mask_count_by_size[k]
                               << " hm=" << test9_stats.h_mask_count_by_size[k];
                }
                for (int k = 1; k < static_cast<int>(test9_stats.window_keep_dp_by_layer.size()); ++k)
                {
                    if (test9_stats.window_keep_dp_by_layer[k] == 0 &&
                        test9_stats.window_keep_h_by_layer[k] == 0)
                    {
                        continue;
                    }
                    stats_line << " win_k=" << k
                               << " keep_dp=" << test9_stats.window_keep_dp_by_layer[k]
                               << " keep_h=" << test9_stats.window_keep_h_by_layer[k]
                               << " drop_low_dp=" << test9_stats.window_drop_low_dp_by_layer[k]
                               << " drop_high_dp=" << test9_stats.window_drop_high_dp_by_layer[k];
                }
                for (int k = 1; k < static_cast<int>(test9_stats.lifecycle_keep_dp_by_layer.size()); ++k)
                {
                    if (test9_stats.lifecycle_keep_dp_by_layer[k] == 0 &&
                        test9_stats.lifecycle_keep_h_by_layer[k] == 0)
                    {
                        continue;
                    }
                    stats_line << " life_k=" << k
                               << " keep_dp=" << test9_stats.lifecycle_keep_dp_by_layer[k]
                               << " keep_h=" << test9_stats.lifecycle_keep_h_by_layer[k]
                               << " drop_dp=" << test9_stats.lifecycle_drop_dp_by_layer[k]
                               << " drop_h=" << test9_stats.lifecycle_drop_h_by_layer[k];
                }
                for (int k = 0; k < static_cast<int>(test9_stats.dp_try_by_src_size.size()); ++k)
                {
                    if (test9_stats.dp_try_by_src_size[k] == 0 && test9_stats.h_try_by_src_size[k] == 0)
                    {
                        continue;
                    }
                    stats_line << " src=" << k
                               << " dp_try=" << test9_stats.dp_try_by_src_size[k]
                               << " dp_succ=" << test9_stats.dp_succ_by_src_size[k]
                               << " h_try=" << test9_stats.h_try_by_src_size[k]
                               << " h_succ=" << test9_stats.h_succ_by_src_size[k];
                }
                for (int k = 0; k < static_cast<int>(test9_stats.dp_try_by_t_size.size()); ++k)
                {
                    if (test9_stats.dp_try_by_t_size[k] == 0 && test9_stats.h_try_by_t_size[k] == 0)
                    {
                        continue;
                    }
                    stats_line << " t=" << k
                               << " dp_try=" << test9_stats.dp_try_by_t_size[k]
                               << " dp_succ=" << test9_stats.dp_succ_by_t_size[k]
                               << " h_try=" << test9_stats.h_try_by_t_size[k]
                               << " h_succ=" << test9_stats.h_succ_by_t_size[k];
                }
                for (int k = 0; k < static_cast<int>(test9_stats.dp_try_by_nxt_size.size()); ++k)
                {
                    if (test9_stats.dp_try_by_nxt_size[k] == 0 && test9_stats.h_try_by_nxt_size[k] == 0)
                    {
                        continue;
                    }
                    stats_line << " nxt=" << k
                               << " dp_try=" << test9_stats.dp_try_by_nxt_size[k]
                               << " dp_succ=" << test9_stats.dp_succ_by_nxt_size[k]
                               << " h_try=" << test9_stats.h_try_by_nxt_size[k]
                               << " h_succ=" << test9_stats.h_succ_by_nxt_size[k];
                }
                for (int k = 0; k < static_cast<int>(test9_stats.full_try_by_src_size.size()); ++k)
                {
                    if (test9_stats.full_try_by_src_size[k] == 0)
                    {
                        continue;
                    }
                    stats_line << " full_src=" << k
                               << " try=" << test9_stats.full_try_by_src_size[k]
                               << " improve=" << test9_stats.full_improve_by_src_size[k];
                }
                const int pair_width = static_cast<int>(test9_stats.dp_try_by_src_size.size());
                for (int src = 0; src < pair_width; ++src)
                {
                    for (int ts = 0; ts < pair_width; ++ts)
                    {
                        const int idx = src * pair_width + ts;
                        if (idx >= static_cast<int>(test9_stats.dp_try_by_src_t.size()))
                        {
                            continue;
                        }
                        if (test9_stats.dp_try_by_src_t[idx] == 0 && test9_stats.h_try_by_src_t[idx] == 0)
                        {
                            continue;
                        }
                        stats_line << " pair=" << src << "," << ts
                                   << " dp_try=" << test9_stats.dp_try_by_src_t[idx]
                                   << " dp_succ=" << test9_stats.dp_succ_by_src_t[idx]
                                   << " h_try=" << test9_stats.h_try_by_src_t[idx]
                                   << " h_succ=" << test9_stats.h_succ_by_src_t[idx];
                    }
                }
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test10)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " best=" << weight_str
                           << " valid_total=" << test10_stats.total_valid
                           << " up_subset=" << test10_stats.total_up_subset
                           << " inqueue=" << test10_stats.total_inqueue
                           << " bucket_scan=" << test10_stats.merge_bucket_scans
                           << " best_checks=" << test10_stats.merge_best_checks
                           << " live_dp_checks=" << test10_stats.merge_live_dp_checks
                           << " future_h_checks=" << test10_stats.merge_future_h_checks
                           << " active_seed=" << test10_stats.active_seed_vertices
                           << " full_seed=" << test10_stats.full_seed_vertices_would
                           << " lb_calls=" << test10_stats.lb_calls
                           << " pq_push=" << test10_stats.pq_pushes
                           << " pq_pop=" << test10_stats.pq_pops
                           << " relax_try=" << test10_stats.relax_attempts
                           << " relax_ok=" << test10_stats.relax_success
                           << " masks=" << test10_stats.masks_processed
                           << " max_active=" << test10_stats.max_active_vertices
                           << " star_ub=" << FormatDouble(test10_stats.root_star_upper)
                           << " greedy_ub=" << FormatDouble(test10_stats.greedy_upper)
                           << " greedy_pop=" << test10_stats.greedy_pops
                           << " prep_ms=" << FormatDouble(test10_stats.preprocess_ms, 3)
                           << " dp_ms=" << FormatDouble(test10_stats.dp_ms, 3);
                for (int k = 0; k < static_cast<int>(test10_stats.valid_by_size.size()); ++k)
                {
                    if (test10_stats.total_by_size[k] <= 0)
                    {
                        continue;
                    }
                    stats_line << " k=" << k
                               << " valid=" << test10_stats.valid_by_size[k]
                               << " total=" << test10_stats.total_by_size[k]
                               << " active=" << test10_stats.active_by_size[k]
                               << " inq=" << test10_stats.inqueue_by_size[k]
                               << " merge=" << test10_stats.merge_by_size[k] << " ";
                }
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test11)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " best=" << weight_str
                           << " valid_total=" << test11_stats.total_valid
                           << " up_subset=" << test11_stats.total_up_subset
                           << " inqueue=" << test11_stats.total_inqueue
                           << " n=" << test11_stats.n
                           << " m=" << test11_stats.m
                           << " g=" << test11_stats.g
                           << " group_vertices=" << test11_stats.total_group_vertices
                           << " max_group=" << test11_stats.max_group_size
                           << " bucket_scan=" << test11_stats.merge_bucket_scans
                           << " best_checks=" << test11_stats.merge_best_checks
                           << " live_dp_checks=" << test11_stats.merge_live_dp_checks
                           << " future_h_checks=" << test11_stats.merge_future_h_checks
                           << " prune_ge_best=" << test11_stats.live_dp_pruned_ge_best
                           << " prune_far=" << test11_stats.live_dp_pruned_far
                           << " closed_layers=" << test11_stats.live_dp_closed_layers
                           << " active_seed=" << test11_stats.active_seed_vertices
                           << " full_seed=" << test11_stats.full_seed_vertices_would
                           << " lb_calls=" << test11_stats.lb_calls
                           << " pq_push=" << test11_stats.pq_pushes
                           << " pq_pop=" << test11_stats.pq_pops
                           << " relax_try=" << test11_stats.relax_attempts
                           << " relax_ok=" << test11_stats.relax_success
                           << " masks=" << test11_stats.masks_processed
                           << " max_active=" << test11_stats.max_active_vertices
                           << " star_ub=" << FormatDouble(test11_stats.root_star_upper)
                           << " greedy_ub=" << FormatDouble(test11_stats.greedy_upper)
                           << " greedy_pop=" << test11_stats.greedy_pops
                           << " prep_ms=" << FormatDouble(test11_stats.preprocess_ms, 3)
                           << " group_dist_ms=" << FormatDouble(test11_stats.group_dist_ms, 3)
                           << " group_pair_ms=" << FormatDouble(test11_stats.group_pair_ms, 3)
                           << " greedy_ms=" << FormatDouble(test11_stats.greedy_ms, 3)
                           << " dp_ms=" << FormatDouble(test11_stats.dp_ms, 3);
                for (int k = 0; k < static_cast<int>(test11_stats.valid_by_size.size()); ++k)
                {
                    if (test11_stats.total_by_size[k] <= 0)
                    {
                        continue;
                    }
                    stats_line << " k=" << k
                               << " valid=" << test11_stats.valid_by_size[k]
                               << " total=" << test11_stats.total_by_size[k]
                               << " active=" << test11_stats.active_by_size[k]
                               << " inq=" << test11_stats.inqueue_by_size[k]
                               << " merge=" << test11_stats.merge_by_size[k] << " ";
                }
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test12)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << query_id << " best=" << weight_str
                           << " valid_total=" << test12_stats.total_valid
                           << " up_subset=" << test12_stats.total_up_subset
                           << " inqueue=" << test12_stats.total_inqueue
                           << " bucket_scan=" << test12_stats.merge_bucket_scans
                           << " best_checks=" << test12_stats.merge_best_checks
                           << " live_dp_checks=" << test12_stats.merge_live_dp_checks
                           << " future_h_checks=" << test12_stats.merge_future_h_checks
                           << " prune_ge_best=" << test12_stats.live_dp_pruned_ge_best
                           << " prune_far=" << test12_stats.live_dp_pruned_far
                           << " target_seed=" << test12_stats.target_seed
                           << " prefix_considered=" << test12_stats.prefix_considered
                           << " prefix_pushed=" << test12_stats.prefix_pushed
                           << " prefix_pruned_lb=" << test12_stats.prefix_pruned_lb
                           << " target_promoted=" << test12_stats.target_promoted
                           << " target_demoted=" << test12_stats.target_demoted
                           << " modify_original=" << test12_stats.modify_original_target
                           << " modify_promoted=" << test12_stats.modify_promoted_target
                           << " modify_init=" << test12_stats.modify_init
                           << " modify_calls=" << test12_stats.modify_calls
                           << " modify_no_effect=" << test12_stats.modify_no_effect
                           << " modify_best_effect=" << test12_stats.modify_best_effect
                           << " modify_live_effect=" << test12_stats.modify_live_effect
                           << " modify_h_effect=" << test12_stats.modify_h_effect
                           << " modify_only_h=" << test12_stats.modify_only_h_effect
                           << " modify_only_live=" << test12_stats.modify_only_live_effect
                           << " modify_only_best=" << test12_stats.modify_only_best_effect
                           << " modify_multi=" << test12_stats.modify_multi_effect
                           << " no_best_cand=" << test12_stats.modify_no_best_candidate
                           << " no_live_cand=" << test12_stats.modify_no_live_candidate
                           << " no_h_cand=" << test12_stats.modify_no_h_candidate
                           << " no_cand_all=" << test12_stats.modify_no_candidate_all
                           << " init_no_effect=" << test12_stats.init_no_effect
                           << " init_only_h=" << test12_stats.init_only_h
                           << " original_no_effect=" << test12_stats.original_no_effect
                           << " original_only_h=" << test12_stats.original_only_h
                           << " promoted_no_effect=" << test12_stats.promoted_no_effect
                           << " promoted_only_h=" << test12_stats.promoted_only_h
                           << " fast_no_candidate=" << test12_stats.fast_no_candidate_return
                           << " precheck_calls=" << test12_stats.precheck_calls
                           << " precheck_scans=" << test12_stats.precheck_scans
                           << " precheck_no_candidate=" << test12_stats.precheck_no_candidate
                           << " pending_promoted=" << test12_stats.pending_promoted
                           << " pending_unique=" << test12_stats.pending_promoted_unique
                           << " pending_layer_roots=" << test12_stats.pending_layer_roots
                           << " pending_layer_repeat=" << test12_stats.pending_layer_repeat_roots
                           << " max_pending_roots_layer=" << test12_stats.max_pending_roots_in_layer
                           << " pop_target=" << test12_stats.pop_target
                           << " pop_prefix=" << test12_stats.pop_prefix
                           << " pop_skip_stale=" << test12_stats.pop_skip_stale
                           << " pop_skip_lb=" << test12_stats.pop_skip_lb
                           << " active_seed=" << test12_stats.active_seed_vertices
                           << " full_seed=" << test12_stats.full_seed_vertices_would
                           << " lb_calls=" << test12_stats.lb_calls
                           << " pq_push=" << test12_stats.pq_pushes
                           << " pq_pop=" << test12_stats.pq_pops
                           << " relax_try=" << test12_stats.relax_attempts
                           << " relax_ok=" << test12_stats.relax_success
                           << " masks=" << test12_stats.masks_processed
                           << " max_active=" << test12_stats.max_active_vertices
                           << " star_ub=" << FormatDouble(test12_stats.root_star_upper)
                           << " greedy_ub=" << FormatDouble(test12_stats.greedy_upper)
                           << " greedy_pop=" << test12_stats.greedy_pops
                           << " prep_ms=" << FormatDouble(test12_stats.preprocess_ms, 3)
                           << " dp_ms=" << FormatDouble(test12_stats.dp_ms, 3);
                for (int k = 0; k < static_cast<int>(test12_stats.valid_by_size.size()); ++k)
                {
                    if (test12_stats.total_by_size[k] <= 0)
                    {
                        continue;
                    }
                    stats_line << " k=" << k
                               << " valid=" << test12_stats.valid_by_size[k]
                               << " total=" << test12_stats.total_by_size[k]
                               << " active=" << test12_stats.active_by_size[k]
                               << " inq=" << test12_stats.inqueue_by_size[k]
                               << " merge=" << test12_stats.merge_by_size[k] << " ";
                }
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
        }

        std::cout << "Graph: " << graph_name << ", queries: " << queries.size() << "\n";
        std::cout << "Result file: " << (fs::path(result_dir) / weights_filename).string() << "\n";
        if (!stats_filename.empty())
        {
            std::cout << "Stats file: " << (fs::path(result_dir) / stats_filename).string() << "\n";
        }
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
