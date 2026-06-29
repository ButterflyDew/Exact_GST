#include <algorithm>
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
#include "methods/Test/test11.h"
#include "methods/Test/test12.h"
#include "methods/Test/test13.h"
#include "methods/Test/test14.h"
#include "methods/Test/test15.h"
#include "methods/Test/test16.h"
#include "memory_usage.h"
#include "output_manager.h"
#include "output_naming.h"
#include "query_io.h"

namespace fs = std::filesystem;

namespace
{

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

template <class Stats>
void AppendSizeBuckets(std::ostringstream& out,
                       const std::vector<long long>& valid,
                       const std::vector<long long>& total,
                       const std::vector<long long>& active,
                       const std::vector<long long>& inqueue,
                       const std::vector<long long>& merge)
{
    const int limit = static_cast<int>(valid.size());
    for (int k = 0; k < limit; ++k)
    {
        if (k >= static_cast<int>(total.size()) || total[k] <= 0)
        {
            continue;
        }
        out << " k=" << k
            << " valid=" << valid[k]
            << " total=" << total[k];
        if (k < static_cast<int>(active.size()))
        {
            out << " active=" << active[k];
        }
        if (k < static_cast<int>(inqueue.size()))
        {
            out << " inq=" << inqueue[k];
        }
        if (k < static_cast<int>(merge.size()))
        {
            out << " merge=" << merge[k];
        }
    }
}

void AppendRuntimeStats(std::ostringstream& out,
                        double sec,
                        const gst::ProcessMemoryUsage& before,
                        const gst::ProcessMemoryUsage& after)
{
    out << " wall_ms=" << FormatDouble(sec * 1000.0, 3)
        << " rss_before_mb=" << FormatDouble(gst::BytesToMiB(before.current_rss_bytes), 3)
        << " rss_after_mb=" << FormatDouble(gst::BytesToMiB(after.current_rss_bytes), 3)
        << " peak_rss_mb=" << FormatDouble(gst::BytesToMiB(after.peak_rss_bytes), 3);
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        const std::string graph_selector = (argc >= 2) ? argv[1] : "";
        const std::string result_root = (argc >= 3) ? argv[2] : "result";
        const std::string query_selector = (argc >= 4) ? argv[3] : "";
        const std::string data_root = (argc >= 5) ? argv[4] : "data";
        const int query_begin = (argc >= 6) ? std::stoi(argv[5]) : 1;
        const int query_limit = (argc >= 7) ? std::stoi(argv[6]) : -1;

        const std::string graph_folder = gst::ResolveGraphFolder(data_root, graph_selector);
        const std::string graph_name = fs::path(graph_folder).filename().string();
        const std::string method_name =
#ifdef GST_DEFAULT_METHOD
            GST_DEFAULT_METHOD;
#else
            "DPBF";
#endif

        const std::string query_file = gst::ResolveQueryFile(graph_folder, query_selector);
        const std::string run_subdir = gst::RunSubdirFromQueryFile(fs::path(query_file));
        const std::string result_dir = (fs::path(result_root) / graph_name / method_name / run_subdir).string();
        const std::string weights_filename = gst::WeightsFilename();
        const std::string stats_filename = gst::StatsFilename(method_name);

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

        gst::OutputManager output_manager(result_dir, weights_filename);
        const gst::ProcessMemoryUsage run_memory = gst::GetProcessMemoryUsage();

        std::ostringstream header;
        header << "# Run started=" << CurrentTimeString()
               << " graph=" << graph_name
               << " method=" << method_name
               << " data_root=" << data_root
               << " graph_folder=" << graph_folder
               << " query_file=" << fs::path(query_file).filename().string()
               << " run_subdir=" << run_subdir
               << " original_query_count=" << all_queries.size()
               << " query_begin=" << query_begin
               << " query_limit=" << query_limit
               << " query_count=" << queries.size()
               << " result_dir=" << result_dir
               << " rss_mb=" << FormatDouble(gst::BytesToMiB(run_memory.current_rss_bytes), 3)
               << " peak_rss_mb=" << FormatDouble(gst::BytesToMiB(run_memory.peak_rss_bytes), 3);
        output_manager.BeginResultRun(weights_filename, header.str());
        if (!stats_filename.empty())
        {
            output_manager.BeginResultRun(stats_filename, header.str());
        }

        const bool is_half = (method_name == "Half_DPBF");
        const bool is_pruned = (method_name == "PrunedDP");
        const bool is_test11 = (method_name == "Test11");
        const bool is_test12 = (method_name == "Test12");
        const bool is_test13 = (method_name == "Test13");
        const bool is_test14 = (method_name == "Test14");
        const bool is_test15 = (method_name == "Test15");
        const bool is_test16 = (method_name == "Test16");

        for (int qi = 0; qi < static_cast<int>(queries.size()); ++qi)
        {
            const int query_id = query_begin + qi;
            const gst::ProcessMemoryUsage memory_before = gst::GetProcessMemoryUsage();
            const auto start = std::chrono::steady_clock::now();

            bool feasible = false;
            double best_weight = -1.0;
            gst::methods::half_dpbf::HalfDpbfStats half_stats;
            gst::methods::pruned_dp::PrunedDpStats pruned_stats;
            gst::methods::test11::Test11Stats test11_stats;
            gst::methods::test12::Test12Stats test12_stats;
            gst::methods::test13::Test13Stats test13_stats;
            gst::methods::test14::Test14Stats test14_stats;
            gst::methods::test15::Test15Stats test15_stats;
            gst::methods::test16::Test16Stats test16_stats;

            if (is_half)
            {
                auto result = gst::methods::half_dpbf::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                half_stats = std::move(result.stats);
            }
            else if (is_pruned)
            {
                auto result = gst::methods::pruned_dp::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                pruned_stats = std::move(result.stats);
            }
            else if (is_test11)
            {
                auto result = gst::methods::test11::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                test11_stats = std::move(result.stats);
            }
            else if (is_test12)
            {
                auto result = gst::methods::test12::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                test12_stats = std::move(result.stats);
            }
            else if (is_test13)
            {
                auto result = gst::methods::test13::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                test13_stats = std::move(result.stats);
            }
            else if (is_test14)
            {
                auto result = gst::methods::test14::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                test14_stats = std::move(result.stats);
            }
            else if (is_test15)
            {
                auto result = gst::methods::test15::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                test15_stats = std::move(result.stats);
            }
            else if (is_test16)
            {
                auto result = gst::methods::test16::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                test16_stats = std::move(result.stats);
            }
            else
            {
                auto result = gst::methods::dpbf::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
            }

            const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const gst::ProcessMemoryUsage memory_after = gst::GetProcessMemoryUsage();
            const std::string weight_str = feasible ? FormatDouble(best_weight) : "-1";
            output_manager.AppendMainResultLine(FormatDouble(sec, 6) + " " + weight_str);
            std::cout << "[Query " << query_id << "] time=" << FormatDouble(sec, 6)
                      << " sec, weight=" << weight_str
                      << ", rss=" << FormatDouble(gst::BytesToMiB(memory_after.current_rss_bytes), 3)
                      << " MiB, peak=" << FormatDouble(gst::BytesToMiB(memory_after.peak_rss_bytes), 3)
                      << " MiB\n";

            if (is_half)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                     << " stage1_total=" << half_stats.stage1_total
                     << " ltV=" << half_stats.lt_v
                     << " ltV2=" << half_stats.lt_v2
                     << " ltV4=" << half_stats.lt_v4
                     << " ltV8=" << half_stats.lt_v8;
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_pruned)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
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
                     << " update_calls=" << pruned_stats.update_calls
                     << " update_bound_pruned=" << pruned_stats.update_bound_pruned
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
                    line << " k=" << k
                         << " final=" << pruned_stats.finalized_by_size[k]
                         << " push=" << pruned_stats.update_push_by_size[k];
                }
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_test11)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                     << " valid_total=" << test11_stats.total_valid
                     << " up_subset=" << test11_stats.total_up_subset
                     << " inqueue=" << test11_stats.total_inqueue
                     << " n=" << test11_stats.n
                     << " m=" << test11_stats.m
                     << " g=" << test11_stats.g
                     << " group_vertices=" << test11_stats.total_group_vertices
                     << " max_group=" << test11_stats.max_group_size
                     << " bucket_scan=" << test11_stats.merge_bucket_scans
                     << " live_dp_checks=" << test11_stats.merge_live_dp_checks
                     << " future_h_checks=" << test11_stats.merge_future_h_checks
                     << " prune_ge_best=" << test11_stats.live_dp_pruned_ge_best
                     << " prune_far=" << test11_stats.live_dp_pruned_far
                     << " active_seed=" << test11_stats.active_seed_vertices
                     << " lb_calls=" << test11_stats.lb_calls
                     << " pq_push=" << test11_stats.pq_pushes
                     << " pq_pop=" << test11_stats.pq_pops
                     << " relax_try=" << test11_stats.relax_attempts
                     << " relax_ok=" << test11_stats.relax_success
                     << " prep_ms=" << FormatDouble(test11_stats.preprocess_ms, 3)
                     << " group_dist_ms=" << FormatDouble(test11_stats.group_dist_ms, 3)
                     << " greedy_ms=" << FormatDouble(test11_stats.greedy_ms, 3)
                     << " dp_ms=" << FormatDouble(test11_stats.dp_ms, 3);
                AppendSizeBuckets<gst::methods::test11::Test11Stats>(
                    line, test11_stats.valid_by_size, test11_stats.total_by_size,
                    test11_stats.active_by_size, test11_stats.inqueue_by_size, test11_stats.merge_by_size);
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_test12)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                     << " valid_total=" << test12_stats.total_valid
                     << " up_subset=" << test12_stats.total_up_subset
                     << " inqueue=" << test12_stats.total_inqueue
                     << " bucket_scan=" << test12_stats.merge_bucket_scans
                     << " live_dp_checks=" << test12_stats.merge_live_dp_checks
                     << " future_h_checks=" << test12_stats.merge_future_h_checks
                     << " prune_ge_best=" << test12_stats.live_dp_pruned_ge_best
                     << " prune_far=" << test12_stats.live_dp_pruned_far
                     << " target_seed=" << test12_stats.target_seed
                     << " prefix_pushed=" << test12_stats.prefix_pushed
                     << " prefix_pruned_lb=" << test12_stats.prefix_pruned_lb
                     << " modify_no_effect=" << test12_stats.modify_no_effect
                     << " fast_no_candidate=" << test12_stats.fast_no_candidate_return
                     << " pending_promoted=" << test12_stats.pending_promoted
                     << " pending_layer_repeat=" << test12_stats.pending_layer_repeat_roots
                     << " pq_push=" << test12_stats.pq_pushes
                     << " pq_pop=" << test12_stats.pq_pops
                     << " relax_try=" << test12_stats.relax_attempts
                     << " relax_ok=" << test12_stats.relax_success
                     << " prep_ms=" << FormatDouble(test12_stats.preprocess_ms, 3)
                     << " dp_ms=" << FormatDouble(test12_stats.dp_ms, 3);
                AppendSizeBuckets<gst::methods::test12::Test12Stats>(
                    line, test12_stats.valid_by_size, test12_stats.total_by_size,
                    test12_stats.active_by_size, test12_stats.inqueue_by_size, test12_stats.merge_by_size);
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_test13)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                     << " n=" << test13_stats.n
                     << " m=" << test13_stats.m
                     << " g=" << test13_stats.g
                     << " group_vertices=" << test13_stats.total_group_vertices
                     << " max_group=" << test13_stats.max_group_size
                     << " valid_total=" << test13_stats.total_valid
                     << " inqueue=" << test13_stats.total_inqueue
                     << " merge_enum=" << test13_stats.merge_enum
                     << " live_dp_checks=" << test13_stats.merge_live_checks
                     << " complement_calls=" << test13_stats.complement_calls
                     << " complement_enum=" << test13_stats.complement_enum
                     << " complement_hits=" << test13_stats.complement_hits
                     << " prune_ge_best=" << test13_stats.prune_ge_best
                     << " prune_far=" << test13_stats.prune_far
                     << " active_seed=" << test13_stats.active_seed
                     << " h_calls=" << test13_stats.h_calls
                     << " h_checks=" << test13_stats.h_checks
                     << " h_hits=" << test13_stats.h_hits
                     << " lb_calls=" << test13_stats.lb_calls
                     << " pq_push=" << test13_stats.pq_push
                     << " pq_pop=" << test13_stats.pq_pop
                     << " relax_try=" << test13_stats.relax_try
                     << " relax_ok=" << test13_stats.relax_ok
                     << " prep_ms=" << FormatDouble(test13_stats.preprocess_ms, 3)
                     << " group_dist_ms=" << FormatDouble(test13_stats.group_dist_ms, 3)
                     << " greedy_ms=" << FormatDouble(test13_stats.greedy_ms, 3)
                     << " dp_ms=" << FormatDouble(test13_stats.dp_ms, 3);
                AppendSizeBuckets<gst::methods::test13::Test13Stats>(
                    line, test13_stats.valid_by_size, test13_stats.total_by_size,
                    test13_stats.active_by_size, test13_stats.inqueue_by_size,
                    test13_stats.merge_by_size);
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_test15)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                     << " n=" << test15_stats.n
                     << " m=" << test15_stats.m
                     << " g=" << test15_stats.g
                     << " valid_total=" << test15_stats.total_valid
                     << " inqueue=" << test15_stats.total_inqueue
                     << " merge_enum=" << test15_stats.merge_enum
                     << " complement_enum=" << test15_stats.complement_enum
                     << " active_seed=" << test15_stats.active_seed
                     << " h_calls=" << test15_stats.h_calls
                     << " h_checks=" << test15_stats.h_checks
                     << " h_hits=" << test15_stats.h_hits
                     << " certify_try=" << test15_stats.certify_try
                     << " certify_success=" << test15_stats.certify_success
                     << " certify_by_lb=" << test15_stats.certify_by_lb
                     << " cover_upgrade_try=" << test15_stats.cover_upgrade_try
                     << " cover_upgrade_success=" << test15_stats.cover_upgrade_success
                     << " lb_calls=" << test15_stats.lb_calls
                     << " prune_ge_best=" << test15_stats.prune_ge_best
                     << " prune_far=" << test15_stats.prune_far
                     << " relax_try=" << test15_stats.relax_try
                     << " relax_ok=" << test15_stats.relax_ok
                     << " pq_push=" << test15_stats.pq_push
                     << " pq_pop=" << test15_stats.pq_pop
                     << " prep_ms=" << FormatDouble(test15_stats.preprocess_ms, 3)
                     << " pair_ms=" << FormatDouble(test15_stats.pair_ms, 3)
                     << " dp_ms=" << FormatDouble(test15_stats.dp_ms, 3);
                AppendSizeBuckets<gst::methods::test15::Test15Stats>(
                    line, test15_stats.valid_by_size, test15_stats.total_by_size,
                    test15_stats.active_by_size, test15_stats.inqueue_by_size,
                    test15_stats.merge_by_size);
                for (int k = 1; k < static_cast<int>(test15_stats.certify_try_by_size.size()); ++k)
                    if (test15_stats.certify_try_by_size[k])
                        line << " cert_k=" << k
                             << " try=" << test15_stats.certify_try_by_size[k]
                             << " ok=" << test15_stats.certify_success_by_size[k];
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_test16)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                     << " n=" << test16_stats.n
                     << " m=" << test16_stats.m
                     << " g=" << test16_stats.g
                     << " group_vertices=" << test16_stats.total_group_vertices
                     << " max_group=" << test16_stats.max_group_size
                     << " valid_total=" << test16_stats.total_valid
                     << " inqueue=" << test16_stats.total_inqueue
                     << " pull_pairs=" << test16_stats.pull_pairs
                     << " pull_scan=" << test16_stats.pull_scan
                     << " pull_hits=" << test16_stats.pull_hits
                     << " h_calls=" << test16_stats.h_calls
                     << " h_pairs=" << test16_stats.h_pairs
                     << " h_scan=" << test16_stats.h_scan
                     << " h_hits=" << test16_stats.h_hits
                     << " complement_pairs=" << test16_stats.complement_pairs
                     << " complement_scan=" << test16_stats.complement_scan
                     << " complement_hits=" << test16_stats.complement_hits
                     << " prune_ge_best=" << test16_stats.prune_ge_best
                     << " prune_far=" << test16_stats.prune_far
                     << " active_seed=" << test16_stats.active_seed
                     << " finite_states=" << test16_stats.finite_states
                     << " confirmed_states=" << test16_stats.confirmed_states
                     << " certify_try=" << test16_stats.certify_try
                     << " certify_success=" << test16_stats.certify_success
                     << " certify_by_lb=" << test16_stats.certify_by_lb
                     << " certify_by_pop=" << test16_stats.certify_by_pop
                     << " lb_calls=" << test16_stats.lb_calls
                     << " pq_push=" << test16_stats.pq_push
                     << " pq_pop=" << test16_stats.pq_pop
                     << " relax_try=" << test16_stats.relax_try
                     << " relax_ok=" << test16_stats.relax_ok
                     << " prep_ms=" << FormatDouble(test16_stats.preprocess_ms, 3)
                     << " group_dist_ms=" << FormatDouble(test16_stats.group_dist_ms, 3)
                     << " greedy_ms=" << FormatDouble(test16_stats.greedy_ms, 3)
                     << " pull_ms=" << FormatDouble(test16_stats.pull_ms, 3)
                     << " h_ms=" << FormatDouble(test16_stats.h_ms, 3)
                     << " complement_ms=" << FormatDouble(test16_stats.complement_ms, 3)
                     << " search_ms=" << FormatDouble(test16_stats.search_ms, 3)
                     << " dp_ms=" << FormatDouble(test16_stats.dp_ms, 3);
                AppendSizeBuckets<gst::methods::test16::Test16Stats>(
                    line, test16_stats.valid_by_size, test16_stats.total_by_size,
                    test16_stats.active_by_size, test16_stats.inqueue_by_size,
                    test16_stats.merge_by_size);
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_test14)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                     << " n=" << test14_stats.n
                     << " m=" << test14_stats.m
                     << " g=" << test14_stats.g
                     << " group_vertices=" << test14_stats.total_group_vertices
                     << " max_group=" << test14_stats.max_group_size
                     << " valid_total=" << test14_stats.total_valid
                     << " inqueue=" << test14_stats.total_inqueue
                     << " pull_pairs=" << test14_stats.pull_pairs
                     << " pull_scan=" << test14_stats.pull_scan
                     << " pull_hits=" << test14_stats.pull_hits
                     << " h_pairs=" << test14_stats.h_pairs
                     << " h_scan=" << test14_stats.h_scan
                     << " h_hits=" << test14_stats.h_hits
                     << " complement_pairs=" << test14_stats.complement_pairs
                     << " complement_scan=" << test14_stats.complement_scan
                     << " complement_hits=" << test14_stats.complement_hits
                     << " prune_ge_best=" << test14_stats.prune_ge_best
                     << " prune_far=" << test14_stats.prune_far
                     << " active_seed=" << test14_stats.active_seed
                     << " finite_states=" << test14_stats.finite_states
                     << " confirmed_states=" << test14_stats.confirmed_states
                     << " lb_calls=" << test14_stats.lb_calls
                     << " pq_push=" << test14_stats.pq_push
                     << " pq_pop=" << test14_stats.pq_pop
                     << " relax_try=" << test14_stats.relax_try
                     << " relax_ok=" << test14_stats.relax_ok
                     << " prep_ms=" << FormatDouble(test14_stats.preprocess_ms, 3)
                     << " group_dist_ms=" << FormatDouble(test14_stats.group_dist_ms, 3)
                     << " greedy_ms=" << FormatDouble(test14_stats.greedy_ms, 3)
                     << " pull_ms=" << FormatDouble(test14_stats.pull_ms, 3)
                     << " h_ms=" << FormatDouble(test14_stats.h_ms, 3)
                     << " complement_ms=" << FormatDouble(test14_stats.complement_ms, 3)
                     << " search_ms=" << FormatDouble(test14_stats.search_ms, 3)
                     << " dp_ms=" << FormatDouble(test14_stats.dp_ms, 3);
                AppendSizeBuckets<gst::methods::test14::Test14Stats>(
                    line, test14_stats.valid_by_size, test14_stats.total_by_size,
                    test14_stats.active_by_size, test14_stats.inqueue_by_size,
                    test14_stats.merge_by_size);
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str;
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
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
