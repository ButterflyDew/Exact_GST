#include <chrono>
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
        const std::vector<gst::Query> queries = gst::LoadQueriesFromFolder(graph_folder, query_selector);
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

        std::ostringstream header;
        header << "# Run started=" << CurrentTimeString()
               << " graph=" << graph_name
               << " method=" << method_name
               << " mode=" << mode_name
               << " data_root=" << data_root
               << " graph_folder=" << graph_folder
               << " query_file=" << fs::path(query_file).filename().string()
               << " run_subdir=" << run_subdir
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
            auto start = std::chrono::steady_clock::now();
            bool feasible = false;
            double best_weight = -1.0;
            std::unique_ptr<gst::AnswerTreeBase> answer;
            gst::methods::half_dpbf::HalfDpbfStats half_stats;
            gst::methods::test1::Test1Stats test1_stats;
            gst::methods::test2::Test2Stats test2_stats;
            gst::methods::test3::Test3Stats test3_stats;
            gst::methods::test4::Test4Stats test4_stats;
            gst::methods::test5::Test5Stats test5_stats;
            gst::methods::test6::Test6Stats test6_stats;
            gst::methods::test7::Test7Stats test7_stats;
            gst::methods::test8::Test8Stats test8_stats;
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
            else if (is_pruned_dp)
            {
                auto solve_result = gst::methods::pruned_dp::SolveOneQuery(graph, queries[qi], output_mode);
                feasible = solve_result.feasible;
                best_weight = solve_result.best_weight;
                answer = std::move(solve_result.answer);
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

            std::cout << "[Query " << (qi + 1) << "] time=" << FormatDouble(sec, 6) << " sec, weight=" << weight_str << "\n";

            if (output_mode != gst::methods::dpbf::OutputMode::kWeightOnly && feasible && answer)
            {
                const std::string mode_name =
                    (output_mode == gst::methods::dpbf::OutputMode::kConcreteTree) ? "tree" : "virtual";
                std::ostringstream answer_text;
                answer_text << header.str() << " query=" << (qi + 1) << "\n" << answer->ToText();
                output_manager.WriteAnswerDetail(qi, mode_name, answer_text.str());
            }

            // Half_DPBF 额外统计：每个查询结束后立即追加写入同一文件。
            if (is_half_dpbf)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << (qi + 1) << " V=" << weight_str
                           << " stage1_total=" << half_stats.stage1_total << " ltV=" << half_stats.lt_v
                           << " ltV2=" << half_stats.lt_v2 << " ltV4=" << half_stats.lt_v4
                           << " ltV8=" << half_stats.lt_v8;
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test1)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << (qi + 1) << " BestQ=" << weight_str
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
                stats_line << "query=" << (qi + 1) << " best=" << weight_str
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
                stats_line << "query=" << (qi + 1) << " best=" << weight_str
                           << " valid_total=" << test3_stats.total_valid;
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test4)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << (qi + 1) << " best=" << weight_str
                           << " valid_total=" << test4_stats.total_valid;
                output_manager.AppendResultLine(stats_filename, stats_line.str());
            }
            if (is_test5)
            {
                std::ostringstream stats_line;
                stats_line << "query=" << (qi + 1) << " best=" << weight_str
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
                stats_line << "query=" << (qi + 1) << " best=" << weight_str
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
                stats_line << "query=" << (qi + 1) << " best=" << weight_str
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
                stats_line << "query=" << (qi + 1) << " best=" << weight_str
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
