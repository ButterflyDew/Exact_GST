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
#include "methods/Test/test16.h"
#include "methods/Test/test17.h"
#include "methods/Test/test18.h"
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

void AppendDoubleBuckets(std::ostringstream& out,
                         const std::string& prefix,
                         const std::vector<double>& value)
{
    for (int k = 0; k < static_cast<int>(value.size()); ++k)
    {
        if (value[k] < 1e90)
        {
            out << " " << prefix << k << "=" << FormatDouble(value[k], 10);
        }
    }
}

void AppendLongBuckets(std::ostringstream& out,
                       const std::string& prefix,
                       const std::vector<long long>& value)
{
    for (int k = 0; k < static_cast<int>(value.size()); ++k)
    {
        if (value[k])
        {
            out << " " << prefix << k << "=" << value[k];
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

        const bool is_dpbf = (method_name == "DPBF");
        const bool is_half = (method_name == "Half_DPBF");
        const bool is_pruned = (method_name == "PrunedDP");
        const bool is_test16 = (method_name == "Test16");
        const bool is_test17 = (method_name == "Test17");
        const bool is_test18 = (method_name == "Test18");
        if (!is_dpbf && !is_half && !is_pruned && !is_test16 && !is_test17 && !is_test18)
        {
            throw std::runtime_error("Unknown GST method: " + method_name);
        }

        for (int qi = 0; qi < static_cast<int>(queries.size()); ++qi)
        {
            const int query_id = query_begin + qi;
            const gst::ProcessMemoryUsage memory_before = gst::GetProcessMemoryUsage();
            const auto start = std::chrono::steady_clock::now();

            bool feasible = false;
            double best_weight = -1.0;
            gst::methods::half_dpbf::HalfDpbfStats half_stats;
            gst::methods::pruned_dp::PrunedDpStats pruned_stats;
            gst::methods::test16::Test16Stats test16_stats;
            gst::methods::test17::Test17Stats test17_stats;
            gst::methods::test18::Test18Stats test18_stats;

            if (is_dpbf)
            {
                auto result = gst::methods::dpbf::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
            }
            else if (is_half)
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
            else if (is_test16)
            {
                auto result = gst::methods::test16::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                test16_stats = std::move(result.stats);
            }
            else if (is_test17)
            {
                auto result = gst::methods::test17::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                test17_stats = std::move(result.stats);
            }
            else if (is_test18)
            {
                auto result = gst::methods::test18::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                test18_stats = std::move(result.stats);
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
                AppendSizeBuckets(line, test16_stats.valid_by_size, test16_stats.total_by_size,
                                  test16_stats.active_by_size, test16_stats.inqueue_by_size,
                                  test16_stats.merge_by_size);
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_test17)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                     << " n=" << test17_stats.n
                     << " m=" << test17_stats.m
                     << " g=" << test17_stats.g
                     << " group_vertices=" << test17_stats.total_group_vertices
                     << " max_group=" << test17_stats.max_group_size
                     << " valid_total=" << test17_stats.total_valid
                     << " inqueue=" << test17_stats.total_inqueue
                     << " pull_pairs=" << test17_stats.pull_pairs
                     << " pull_scan=" << test17_stats.pull_scan
                     << " pull_hits=" << test17_stats.pull_hits
                     << " h_calls=" << test17_stats.h_calls
                     << " h_pairs=" << test17_stats.h_pairs
                     << " h_scan=" << test17_stats.h_scan
                     << " h_hits=" << test17_stats.h_hits
                     << " complement_calls=" << test17_stats.complement_calls
                     << " complement_skip_early=" << test17_stats.complement_skip_early
                     << " complement_mask_pairs=" << test17_stats.complement_mask_pairs
                     << " complement_mask_empty=" << test17_stats.complement_mask_empty
                     << " complement_cover_smaller=" << test17_stats.complement_cover_smaller
                     << " complement_cover_possible=" << test17_stats.complement_cover_possible
                     << " complement_pairs=" << test17_stats.complement_pairs
                     << " complement_scan=" << test17_stats.complement_scan
                     << " complement_hits=" << test17_stats.complement_hits
                     << " prune_ge_best=" << test17_stats.prune_ge_best
                     << " prune_far=" << test17_stats.prune_far
                     << " active_seed=" << test17_stats.active_seed
                     << " finite_states=" << test17_stats.finite_states
                     << " confirmed_states=" << test17_stats.confirmed_states
                     << " certify_try=" << test17_stats.certify_try
                     << " certify_success=" << test17_stats.certify_success
                     << " certify_by_lb=" << test17_stats.certify_by_lb
                     << " certify_by_pop=" << test17_stats.certify_by_pop
                     << " lb_calls=" << test17_stats.lb_calls
                     << " pq_push=" << test17_stats.pq_push
                     << " pq_pop=" << test17_stats.pq_pop
                     << " relax_try=" << test17_stats.relax_try
                     << " relax_ok=" << test17_stats.relax_ok
                     << " prep_ms=" << FormatDouble(test17_stats.preprocess_ms, 3)
                     << " group_dist_ms=" << FormatDouble(test17_stats.group_dist_ms, 3)
                     << " greedy_ms=" << FormatDouble(test17_stats.greedy_ms, 3)
                     << " pull_ms=" << FormatDouble(test17_stats.pull_ms, 3)
                     << " h_ms=" << FormatDouble(test17_stats.h_ms, 3)
                     << " complement_ms=" << FormatDouble(test17_stats.complement_ms, 3)
                     << " search_ms=" << FormatDouble(test17_stats.search_ms, 3)
                     << " dp_ms=" << FormatDouble(test17_stats.dp_ms, 3);
                AppendSizeBuckets(line, test17_stats.valid_by_size, test17_stats.total_by_size,
                                  test17_stats.active_by_size, test17_stats.inqueue_by_size,
                                  test17_stats.merge_by_size);
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_test18)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                     << " n=" << test18_stats.n
                     << " m=" << test18_stats.m
                     << " g=" << test18_stats.g
                     << " group_vertices=" << test18_stats.total_group_vertices
                     << " max_group=" << test18_stats.max_group_size
                     << " inqueue=" << test18_stats.total_inqueue
                     << " pull_pairs=" << test18_stats.pull_pairs
                     << " pull_scan=" << test18_stats.pull_scan
                     << " pull_hits=" << test18_stats.pull_hits
                     << " complement_calls=" << test18_stats.complement_calls
                     << " complement_skip_early=" << test18_stats.complement_skip_early
                     << " complement_mask_pairs=" << test18_stats.complement_mask_pairs
                     << " complement_mask_empty=" << test18_stats.complement_mask_empty
                     << " complement_cover_smaller=" << test18_stats.complement_cover_smaller
                     << " complement_cover_possible=" << test18_stats.complement_cover_possible
                     << " pair_partition_roots=" << test18_stats.pair_partition_roots
                     << " pair_partition_updates=" << test18_stats.pair_partition_updates
                     << " pair_partition_upper=" << FormatDouble(test18_stats.pair_partition_upper, 10)
                     << " pair_saved_cover_extra=" << test18_stats.pair_saved_cover_extra
                     << " pair_saved_cover_extra_groups=" << test18_stats.pair_saved_cover_extra_groups
                     << " pair_dense_rows=" << test18_stats.pair_dense_rows
                     << " pair_dense_states=" << test18_stats.pair_dense_states
                     << " dense_rows=" << test18_stats.dense_rows
                     << " dense_states=" << test18_stats.dense_states
                     << " pair_saved_slack_count=" << test18_stats.pair_saved_slack_count
                     << " pair_saved_slack_rel_avg="
                     << FormatDouble(test18_stats.pair_saved_slack_count
                                         ? test18_stats.pair_saved_slack_rel_sum / test18_stats.pair_saved_slack_count
                                         : -1.0,
                                     6)
                     << " pair_saved_slack_rel_max=" << FormatDouble(test18_stats.pair_saved_slack_rel_max, 6)
                     << " early_cover_extra=" << test18_stats.early_cover_extra
                     << " early_cover_extra_groups=" << test18_stats.early_cover_extra_groups
                     << " early_cover_rem_le_2k=" << test18_stats.early_cover_rem_le_2k
                     << " early_cover_pair_ready=" << test18_stats.early_cover_pair_ready
                     << " early_cover_pair_better=" << test18_stats.early_cover_pair_better
                     << " early_cover_pair_checks=" << test18_stats.early_cover_pair_checks
                     << " early_cover_pair_splits=" << test18_stats.early_cover_pair_splits
                     << " early_cover_best_updates=" << test18_stats.early_cover_best_updates
                     << " early_cover_first_better_size=" << test18_stats.early_cover_first_better_size
                     << " early_cover_best_candidate=" << FormatDouble(test18_stats.early_cover_best_candidate, 10)
                     << " early_cover_min_rem=" << test18_stats.early_cover_min_rem
                     << " complement_pairs=" << test18_stats.complement_pairs
                     << " complement_scan=" << test18_stats.complement_scan
                     << " complement_hits=" << test18_stats.complement_hits
                     << " global_root_alive=" << test18_stats.global_root_alive
                     << " global_root_pruned=" << test18_stats.global_root_pruned
                     << " tryset_calls=" << test18_stats.tryset_calls
                     << " tryset_keep=" << test18_stats.tryset_keep
                     << " tryset_pruned_full=" << test18_stats.tryset_pruned_full
                     << " tryset_pruned_ge_best=" << test18_stats.tryset_pruned_ge_best
                     << " tryset_pruned_far=" << test18_stats.tryset_pruned_far
                     << " tryset_pruned_lb=" << test18_stats.tryset_pruned_lb
                     << " seed_try=" << test18_stats.seed_try
                     << " seed_push=" << test18_stats.seed_push
                     << " seed_block_far=" << test18_stats.seed_block_far
                     << " seed_block_lb=" << test18_stats.seed_block_lb
                     << " pop_pruned_full=" << test18_stats.pop_pruned_full
                     << " pop_pruned_far=" << test18_stats.pop_pruned_far
                     << " pop_pruned_lb=" << test18_stats.pop_pruned_lb
                     << " final_pruned_full=" << test18_stats.final_pruned_full
                     << " final_pruned_need=" << test18_stats.final_pruned_need
                     << " stale_need_skips=" << test18_stats.stale_need_skips
                     << " lookup_need_skips=" << test18_stats.lookup_need_skips
                     << " compact_calls=" << test18_stats.compact_calls
                     << " compact_removed=" << test18_stats.compact_removed
                     << " compact_ms=" << FormatDouble(test18_stats.compact_ms, 3)
                     << " prune_ge_best=" << test18_stats.prune_ge_best
                     << " prune_far=" << test18_stats.prune_far
                     << " active_seed=" << test18_stats.active_seed
                     << " finite_states=" << test18_stats.finite_states
                     << " best_updates=" << test18_stats.best_updates
                     << " first_best_update_size=" << test18_stats.first_best_update_size
                     << " last_best_update_size=" << test18_stats.last_best_update_size
                     << " root_star_upper=" << FormatDouble(test18_stats.root_star_upper, 10)
                     << " greedy_upper=" << FormatDouble(test18_stats.greedy_upper, 10)
                     << " multi_greedy_upper=" << FormatDouble(test18_stats.multi_greedy_upper, 10)
                     << " multi_greedy_roots=" << test18_stats.multi_greedy_roots
                     << " lb_calls=" << test18_stats.lb_calls
                     << " pq_push=" << test18_stats.pq_push
                     << " pq_pop=" << test18_stats.pq_pop
                     << " relax_try=" << test18_stats.relax_try
                     << " relax_ok=" << test18_stats.relax_ok
                     << " prep_ms=" << FormatDouble(test18_stats.preprocess_ms, 3)
                     << " group_dist_ms=" << FormatDouble(test18_stats.group_dist_ms, 3)
                     << " greedy_ms=" << FormatDouble(test18_stats.greedy_ms, 3)
                     << " pair_partition_ms=" << FormatDouble(test18_stats.pair_partition_ms, 3)
                     << " pull_ms=" << FormatDouble(test18_stats.pull_ms, 3)
                     << " complement_ms=" << FormatDouble(test18_stats.complement_ms, 3)
                     << " search_ms=" << FormatDouble(test18_stats.search_ms, 3)
                     << " dp_ms=" << FormatDouble(test18_stats.dp_ms, 3);
                AppendLongBuckets(line, "total_k", test18_stats.total_by_size);
                AppendLongBuckets(line, "active_k", test18_stats.active_by_size);
                AppendLongBuckets(line, "inq_k", test18_stats.inqueue_by_size);
                AppendLongBuckets(line, "merge_k", test18_stats.merge_by_size);
                AppendLongBuckets(line, "early_cover_rem", test18_stats.early_cover_by_rem_size);
                AppendLongBuckets(line, "early_cover_ready_rem", test18_stats.early_cover_ready_by_rem_size);
                AppendLongBuckets(line, "early_cover_better_rem", test18_stats.early_cover_better_by_rem_size);
                AppendLongBuckets(line, "pair_saved_cover", test18_stats.pair_saved_by_cover_size);
                AppendLongBuckets(line, "pair_saved_slack_rel", test18_stats.pair_saved_slack_rel_bucket);
                AppendLongBuckets(line, "dense_rows_k", test18_stats.dense_rows_by_size);
                AppendLongBuckets(line, "dense_states_k", test18_stats.dense_states_by_size);
                AppendDoubleBuckets(line, "best_after_k", test18_stats.best_after_size);
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
