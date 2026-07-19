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
#include "methods/Release/release_v1.h"
#include "methods/Release/release_v2.h"
#include "methods/Release/release_v3.h"
#include "methods/Release/release_v4.h"
#include "methods/Release/release_v5.h"
#include "methods/Release/release_v6.h"
#include "methods/Test/test16.h"
#include "methods/Test/test17.h"
#include "methods/Test/test18.h"
#include "methods/Test/test19.h"
#include "methods/Test/test21_anchor_half.h"
#include "methods/Test/test80_anchor_progressive.h"
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

bool ParseOnOff(const std::string& value, const std::string& option)
{
    if (value == "on")
        return true;
    if (value == "off")
        return false;
    throw std::runtime_error(option + " expects on or off.");
}

gst::methods::pruned_dp::PrunedDpOptions ParsePrunedDpOptions(int argc, char** argv)
{
    gst::methods::pruned_dp::PrunedDpOptions options;
    for (int index = 7; index < argc; ++index)
    {
        const std::string argument = argv[index];
        const std::string storage_prefix = "--state-storage=";
        const std::string mst_prefix = "--mst-upper=";
        const std::string pathmax_prefix = "--lb2-pathmax=";
        if (argument.rfind(storage_prefix, 0) == 0)
        {
            const std::string value = argument.substr(storage_prefix.size());
            if (value == "hash")
                options.state_storage = gst::methods::pruned_dp::StateStorage::Hash;
            else if (value == "dense")
                options.state_storage = gst::methods::pruned_dp::StateStorage::Dense;
            else
                throw std::runtime_error("--state-storage expects hash or dense.");
        }
        else if (argument.rfind(mst_prefix, 0) == 0)
        {
            options.use_mst_upper_bound =
                ParseOnOff(argument.substr(mst_prefix.size()), "--mst-upper");
        }
        else if (argument.rfind(pathmax_prefix, 0) == 0)
        {
            options.enforce_lb2_pathmax =
                ParseOnOff(argument.substr(pathmax_prefix.size()), "--lb2-pathmax");
        }
        else
        {
            throw std::runtime_error("Unknown PrunedDP option: " + argument);
        }
    }
    return options;
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

template <class Stats>
void AppendTest18FamilyStats(gst::OutputManager& output_manager,
                             const std::string& stats_filename,
                             int query_id,
                             const std::string& weight_str,
                             const Stats& stats,
                             double sec,
                             const gst::ProcessMemoryUsage& memory_before,
                             const gst::ProcessMemoryUsage& memory_after)
{
    std::ostringstream line;
    line << "query=" << query_id << " best=" << weight_str
         << " n=" << stats.n
         << " m=" << stats.m
         << " original_n=" << stats.original_n
         << " original_m=" << stats.original_m
         << " g=" << stats.g
         << " group_vertices=" << stats.total_group_vertices
         << " max_group=" << stats.max_group_size
         << " degree_reduce_removed_vertices=" << stats.degree_reduce_removed_vertices
         << " degree_reduce_removed_edges=" << stats.degree_reduce_removed_edges
         << " degree_reduce_leaf_vertices=" << stats.degree_reduce_leaf_vertices
         << " degree_reduce_dead_vertices=" << stats.degree_reduce_dead_vertices
         << " degree_reduce_contracted_vertices=" << stats.degree_reduce_contracted_vertices
         << " degree_reduce_ms=" << FormatDouble(stats.degree_reduce_ms, 3)
         << " leaf_reduce_removed_vertices=" << stats.leaf_reduce_removed_vertices
         << " leaf_reduce_removed_edges=" << stats.leaf_reduce_removed_edges
         << " leaf_reduce_removed_components=" << stats.leaf_reduce_removed_components
         << " leaf_reduce_two_portal_components=" << stats.leaf_reduce_two_portal_components
         << " leaf_reduce_two_portal_vertices=" << stats.leaf_reduce_two_portal_vertices
         << " leaf_reduce_two_portal_edges_added=" << stats.leaf_reduce_two_portal_edges_added
         << " leaf_reduce_three_portal_components=" << stats.leaf_reduce_three_portal_components
         << " leaf_reduce_three_portal_vertices=" << stats.leaf_reduce_three_portal_vertices
         << " leaf_reduce_three_portal_hubs_added=" << stats.leaf_reduce_three_portal_hubs_added
         << " leaf_reduce_three_portal_edges_added=" << stats.leaf_reduce_three_portal_edges_added
         << " leaf_reduce_four_portal_components=" << stats.leaf_reduce_four_portal_components
         << " leaf_reduce_four_portal_vertices=" << stats.leaf_reduce_four_portal_vertices
         << " leaf_reduce_four_portal_hubs_added=" << stats.leaf_reduce_four_portal_hubs_added
         << " leaf_reduce_four_portal_edges_added=" << stats.leaf_reduce_four_portal_edges_added
         << " leaf_reduce_ms=" << FormatDouble(stats.leaf_reduce_ms, 3)
         << " inqueue=" << stats.total_inqueue
         << " pull_pairs=" << stats.pull_pairs
         << " pull_scan=" << stats.pull_scan
         << " pull_hits=" << stats.pull_hits
         << " complement_calls=" << stats.complement_calls
         << " complement_skip_early=" << stats.complement_skip_early
         << " complement_mask_pairs=" << stats.complement_mask_pairs
         << " complement_mask_empty=" << stats.complement_mask_empty
         << " complement_cover_smaller=" << stats.complement_cover_smaller
         << " complement_cover_possible=" << stats.complement_cover_possible
         << " pair_partition_roots=" << stats.pair_partition_roots
         << " pair_partition_updates=" << stats.pair_partition_updates
         << " pair_partition_upper=" << FormatDouble(stats.pair_partition_upper, 10)
         << " pair_saved_cover_extra=" << stats.pair_saved_cover_extra
         << " pair_saved_cover_extra_groups=" << stats.pair_saved_cover_extra_groups
         << " pair_dense_rows=" << stats.pair_dense_rows
         << " pair_dense_states=" << stats.pair_dense_states
         << " dense_rows=" << stats.dense_rows
         << " dense_states=" << stats.dense_states
         << " pair_saved_slack_count=" << stats.pair_saved_slack_count
         << " pair_saved_slack_rel_avg="
         << FormatDouble(stats.pair_saved_slack_count
                             ? stats.pair_saved_slack_rel_sum / stats.pair_saved_slack_count
                             : -1.0,
                         6)
         << " pair_saved_slack_rel_max=" << FormatDouble(stats.pair_saved_slack_rel_max, 6)
         << " early_cover_extra=" << stats.early_cover_extra
         << " early_cover_extra_groups=" << stats.early_cover_extra_groups
         << " early_cover_rem_le_2k=" << stats.early_cover_rem_le_2k
         << " early_cover_pair_ready=" << stats.early_cover_pair_ready
         << " early_cover_pair_better=" << stats.early_cover_pair_better
         << " early_cover_pair_checks=" << stats.early_cover_pair_checks
         << " early_cover_pair_splits=" << stats.early_cover_pair_splits
         << " early_cover_best_updates=" << stats.early_cover_best_updates
         << " early_cover_first_better_size=" << stats.early_cover_first_better_size
         << " early_cover_best_candidate=" << FormatDouble(stats.early_cover_best_candidate, 10)
         << " early_cover_min_rem=" << stats.early_cover_min_rem
         << " complement_pairs=" << stats.complement_pairs
         << " complement_scan=" << stats.complement_scan
         << " complement_hits=" << stats.complement_hits
         << " complement_cache_builds=" << stats.complement_cache_builds
         << " complement_cache_queries=" << stats.complement_cache_queries
         << " complement_cache_scan=" << stats.complement_cache_scan
         << " complement_cache_hits=" << stats.complement_cache_hits
         << " complement_cache_rent_cost=" << stats.complement_cache_rent_cost
         << " complement_cache_buy_cost=" << stats.complement_cache_buy_cost
         << " complement_cache_need_skips=" << stats.complement_cache_need_skips
         << " global_root_alive=" << stats.global_root_alive
         << " global_root_pruned=" << stats.global_root_pruned
         << " tryset_calls=" << stats.tryset_calls
         << " tryset_keep=" << stats.tryset_keep
         << " tryset_pruned_full=" << stats.tryset_pruned_full
         << " tryset_pruned_ge_best=" << stats.tryset_pruned_ge_best
         << " tryset_pruned_far=" << stats.tryset_pruned_far
         << " tryset_pruned_lb=" << stats.tryset_pruned_lb
         << " seed_try=" << stats.seed_try
         << " seed_push=" << stats.seed_push
         << " seed_block_far=" << stats.seed_block_far
         << " seed_block_lb=" << stats.seed_block_lb
         << " pop_pruned_full=" << stats.pop_pruned_full
         << " pop_pruned_far=" << stats.pop_pruned_far
         << " pop_pruned_lb=" << stats.pop_pruned_lb
         << " astar_order_enabled=" << stats.astar_order_enabled
         << " astar_relax_lb_pruned=" << stats.astar_relax_lb_pruned
         << " onetree_lb_diag_enabled=" << stats.onetree_lb_diag_enabled
         << " onetree_lb_diag_checks=" << stats.onetree_lb_diag_checks
         << " onetree_lb_diag_stronger_lb=" << stats.onetree_lb_diag_stronger_lb
         << " onetree_lb_diag_stronger_need=" << stats.onetree_lb_diag_stronger_need
         << " onetree_lb_diag_extra_seed_prune=" << stats.onetree_lb_diag_extra_seed_prune
         << " onetree_lb_diag_extra_final_prune=" << stats.onetree_lb_diag_extra_final_prune
         << " onetree_lb_diag_gain_avg="
         << FormatDouble(stats.onetree_lb_diag_stronger_need
                             ? stats.onetree_lb_diag_gain_sum / stats.onetree_lb_diag_stronger_need
                             : 0.0,
                         6)
         << " onetree_lb_diag_gain_max=" << FormatDouble(stats.onetree_lb_diag_gain_max, 6)
         << " onetree_lb_diag_ms=" << FormatDouble(stats.onetree_lb_diag_ms, 3)
         << " onetree_lb_prune_enabled=" << stats.onetree_lb_prune_enabled
         << " onetree_lb_seed_pruned=" << stats.onetree_lb_seed_pruned
         << " onetree_lb_final_pruned=" << stats.onetree_lb_final_pruned
         << " tsp_lb_diag_enabled=" << stats.tsp_lb_diag_enabled
         << " tsp_lb_diag_checks=" << stats.tsp_lb_diag_checks
         << " tsp_lb_diag_stronger_lb=" << stats.tsp_lb_diag_stronger_lb
         << " tsp_lb_diag_stronger_need=" << stats.tsp_lb_diag_stronger_need
         << " tsp_lb_diag_extra_seed_prune=" << stats.tsp_lb_diag_extra_seed_prune
         << " tsp_lb_diag_extra_final_prune=" << stats.tsp_lb_diag_extra_final_prune
         << " tsp_lb_diag_gain_avg="
         << FormatDouble(stats.tsp_lb_diag_stronger_need
                             ? stats.tsp_lb_diag_gain_sum / stats.tsp_lb_diag_stronger_need
                             : 0.0,
                         6)
         << " tsp_lb_diag_gain_max=" << FormatDouble(stats.tsp_lb_diag_gain_max, 6)
         << " tsp_lb_diag_ms=" << FormatDouble(stats.tsp_lb_diag_ms, 3)
         << " tsp_lb_order_enabled=" << stats.tsp_lb_order_enabled
         << " tsp_lb_order_seed_pruned=" << stats.tsp_lb_order_seed_pruned
         << " tsp_lb_order_pop_pruned=" << stats.tsp_lb_order_pop_pruned
         << " tsp_lb_order_relax_pruned=" << stats.tsp_lb_order_relax_pruned
         << " tsp_lb_save_enabled=" << stats.tsp_lb_save_enabled
         << " tsp_lb_save_pruned=" << stats.tsp_lb_save_pruned
         << " tsp_lb_queries=" << stats.tsp_lb_queries
         << " tsp_lb_precompute_ms=" << FormatDouble(stats.tsp_lb_precompute_ms, 3)
         << " tsp_lb_ms=" << FormatDouble(stats.tsp_lb_ms, 3)
         << " final_pruned_full=" << stats.final_pruned_full
         << " final_pruned_need=" << stats.final_pruned_need
         << " stale_need_skips=" << stats.stale_need_skips
         << " lookup_need_skips=" << stats.lookup_need_skips
         << " compact_calls=" << stats.compact_calls
         << " compact_removed=" << stats.compact_removed
         << " compact_light_removed=" << stats.compact_light_removed
         << " compact_dense_removed=" << stats.compact_dense_removed
         << " order_pruned_rows=" << stats.order_pruned_rows
         << " order_pruned_states=" << stats.order_pruned_states
         << " order_lb_pruned_states=" << stats.order_lb_pruned_states
         << " order_split_pruned_states=" << stats.order_split_pruned_states
         << " compact_ms=" << FormatDouble(stats.compact_ms, 3)
         << " prune_ge_best=" << stats.prune_ge_best
         << " prune_far=" << stats.prune_far
         << " active_seed=" << stats.active_seed
         << " finite_states=" << stats.finite_states
         << " live_states=" << (stats.finite_states - stats.compact_removed)
         << " best_updates=" << stats.best_updates
         << " first_best_update_size=" << stats.first_best_update_size
         << " last_best_update_size=" << stats.last_best_update_size
         << " root_star_upper=" << FormatDouble(stats.root_star_upper, 10)
         << " greedy_upper=" << FormatDouble(stats.greedy_upper, 10)
         << " multi_greedy_upper=" << FormatDouble(stats.multi_greedy_upper, 10)
         << " multi_greedy_roots=" << stats.multi_greedy_roots
         << " lb_calls=" << stats.lb_calls
         << " pq_push=" << stats.pq_push
         << " pq_pop=" << stats.pq_pop
         << " relax_try=" << stats.relax_try
         << " relax_ok=" << stats.relax_ok
         << " prep_ms=" << FormatDouble(stats.preprocess_ms, 3)
         << " group_dist_ms=" << FormatDouble(stats.group_dist_ms, 3)
         << " greedy_ms=" << FormatDouble(stats.greedy_ms, 3)
         << " pair_partition_ms=" << FormatDouble(stats.pair_partition_ms, 3)
         << " pull_ms=" << FormatDouble(stats.pull_ms, 3)
         << " complement_ms=" << FormatDouble(stats.complement_ms, 3)
         << " complement_cache_ms=" << FormatDouble(stats.complement_cache_ms, 3)
         << " search_ms=" << FormatDouble(stats.search_ms, 3)
         << " dp_ms=" << FormatDouble(stats.dp_ms, 3);
    AppendLongBuckets(line, "total_k", stats.total_by_size);
    AppendLongBuckets(line, "active_k", stats.active_by_size);
    AppendLongBuckets(line, "inq_k", stats.inqueue_by_size);
    AppendLongBuckets(line, "merge_k", stats.merge_by_size);
    AppendLongBuckets(line, "pull_pairs_k", stats.pull_pairs_by_size);
    AppendLongBuckets(line, "pull_scan_k", stats.pull_scan_by_size);
    AppendLongBuckets(line, "pull_hits_k", stats.pull_hits_by_size);
    AppendLongBuckets(line, "pull_seed_scan_k", stats.pull_seed_scan_by_size);
    AppendLongBuckets(line, "pull_seed_hits_k", stats.pull_seed_hits_by_size);
    AppendLongBuckets(line, "pull_singleton_scan_k", stats.pull_singleton_scan_by_size);
    AppendLongBuckets(line, "pull_singleton_hits_k", stats.pull_singleton_hits_by_size);
    AppendLongBuckets(line, "pull_dense_dense_scan_k", stats.pull_dense_dense_scan_by_size);
    AppendLongBuckets(line, "pull_dense_dense_hits_k", stats.pull_dense_dense_hits_by_size);
    AppendLongBuckets(line, "pull_dense_sparse_scan_k", stats.pull_dense_sparse_scan_by_size);
    AppendLongBuckets(line, "pull_dense_sparse_hits_k", stats.pull_dense_sparse_hits_by_size);
    AppendLongBuckets(line, "pull_sparse_sparse_scan_k", stats.pull_sparse_sparse_scan_by_size);
    AppendLongBuckets(line, "pull_sparse_sparse_hits_k", stats.pull_sparse_sparse_hits_by_size);
    AppendLongBuckets(line, "search_seed_try_k", stats.search_seed_try_by_size);
    AppendLongBuckets(line, "search_seed_push_k", stats.search_seed_push_by_size);
    AppendLongBuckets(line, "search_pq_pop_k", stats.search_pq_pop_by_size);
    AppendLongBuckets(line, "search_relax_try_k", stats.search_relax_try_by_size);
    AppendLongBuckets(line, "search_relax_ok_k", stats.search_relax_ok_by_size);
    AppendLongBuckets(line, "complement_calls_k", stats.complement_calls_by_size);
    AppendLongBuckets(line, "complement_scan_k", stats.complement_scan_by_size);
    AppendLongBuckets(line, "complement_hits_k", stats.complement_hits_by_size);
    AppendLongBuckets(line, "complement_cache_builds_k", stats.complement_cache_builds_by_size);
    AppendLongBuckets(line, "complement_cache_queries_k", stats.complement_cache_queries_by_size);
    AppendLongBuckets(line, "complement_cache_scan_k", stats.complement_cache_scan_by_size);
    AppendLongBuckets(line, "complement_cache_hits_k", stats.complement_cache_hits_by_size);
    AppendLongBuckets(line, "early_cover_rem", stats.early_cover_by_rem_size);
    AppendLongBuckets(line, "early_cover_ready_rem", stats.early_cover_ready_by_rem_size);
    AppendLongBuckets(line, "early_cover_better_rem", stats.early_cover_better_by_rem_size);
    AppendLongBuckets(line, "pair_saved_cover", stats.pair_saved_by_cover_size);
    AppendLongBuckets(line, "pair_saved_slack_rel", stats.pair_saved_slack_rel_bucket);
    AppendLongBuckets(line, "dense_rows_k", stats.dense_rows_by_size);
    AppendLongBuckets(line, "dense_states_k", stats.dense_states_by_size);
    AppendDoubleBuckets(line, "pull_ms_k", stats.pull_ms_by_size);
    AppendDoubleBuckets(line, "search_ms_k", stats.search_ms_by_size);
    AppendDoubleBuckets(line, "complement_ms_k", stats.complement_ms_by_size);
    AppendDoubleBuckets(line, "complement_cache_ms_k", stats.complement_cache_ms_by_size);
    AppendDoubleBuckets(line, "best_after_k", stats.best_after_size);
    AppendRuntimeStats(line, sec, memory_before, memory_after);
    output_manager.AppendResultLine(stats_filename, line.str());
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
        gst::methods::pruned_dp::PrunedDpOptions pruned_options;
        if (method_name == "PrunedDP")
            pruned_options = ParsePrunedDpOptions(argc, argv);
        else if (argc > 7)
            throw std::runtime_error("Extra solver options are only supported by PrunedDP.");

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
               << " memory_metric=query_peak_rss_mb"
               << " rss_sample_interval_ms=" << gst::kQueryRssSampleIntervalMs;
        if (method_name == "PrunedDP")
        {
            header << " state_storage="
                   << gst::methods::pruned_dp::StateStorageName(
                          pruned_options.state_storage)
                   << " mst_upper=" << (pruned_options.use_mst_upper_bound ? "on" : "off")
                   << " lb2_pathmax="
                   << (pruned_options.enforce_lb2_pathmax ? "on" : "off");
        }
        output_manager.BeginResultRun(weights_filename, header.str());
        if (!stats_filename.empty())
        {
            output_manager.BeginResultRun(stats_filename, header.str());
        }

        const bool is_dpbf = (method_name == "DPBF");
        const bool is_half = (method_name == "Half_DPBF");
        const bool is_pruned = (method_name == "PrunedDP");
        const bool is_release_v1 = (method_name == "ReleaseV1");
        const bool is_release_v2 = (method_name == "ReleaseV2");
        const bool is_release_v3 = (method_name == "ReleaseV3");
        const bool is_release_v4 = (method_name == "ReleaseV4");
        const bool is_release_v5 = (method_name == "ReleaseV5");
        const bool is_release_v6 = (method_name == "ReleaseV6");
        const bool is_test16 = (method_name == "Test16");
        const bool is_test17 = (method_name == "Test17");
        const bool is_test18 = (method_name == "Test18");
        const bool is_test19 = (method_name == "Test19");
        const bool is_test21 = (method_name == "Test21");
        const bool is_test80 = (method_name == "Test80");
        if (!is_dpbf && !is_half && !is_pruned && !is_release_v1 && !is_release_v2 &&
            !is_release_v3 && !is_release_v4 && !is_release_v5 && !is_release_v6 &&
            !is_test16 && !is_test17 && !is_test18 && !is_test19 && !is_test21 &&
            !is_test80)
        {
            throw std::runtime_error("Unknown GST method: " + method_name);
        }

        for (int qi = 0; qi < static_cast<int>(queries.size()); ++qi)
        {
            const int query_id = query_begin + qi;
            gst::QueryPeakRssSampler query_memory;
            const gst::ProcessMemoryUsage memory_before = gst::GetProcessMemoryUsage();
            const auto start = std::chrono::steady_clock::now();

            bool feasible = false;
            double best_weight = -1.0;
            gst::methods::half_dpbf::HalfDpbfStats half_stats;
            gst::methods::pruned_dp::PrunedDpStats pruned_stats;
            gst::methods::release_v1::ReleaseStats release_stats;
            gst::methods::release_v2::ReleaseStats release_v2_stats;
            gst::methods::release_v3::ReleaseStats release_v3_stats;
            gst::methods::test16::Test16Stats test16_stats;
            gst::methods::test17::Test17Stats test17_stats;
            gst::methods::test18::Test18Stats test18_stats;
            gst::methods::test19::Test19Stats test19_stats;
            gst::methods::test21_anchor_half::Test21Stats test21_stats;
            gst::methods::test80_anchor_progressive::Test80Stats test80_stats;

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
                auto result = gst::methods::pruned_dp::SolveOneQuery(
                    graph, queries[qi], pruned_options);
                feasible = result.feasible;
                best_weight = result.best_weight;
                pruned_stats = std::move(result.stats);
            }
            else if (is_release_v1)
            {
                auto result = gst::methods::release_v1::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                release_stats = std::move(result.stats);
            }
            else if (is_release_v2)
            {
                auto result = gst::methods::release_v2::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                release_v2_stats = std::move(result.stats);
            }
            else if (is_release_v3)
            {
                auto result = gst::methods::release_v3::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                release_v3_stats = std::move(result.stats);
            }
            else if (is_release_v4)
            {
                const auto result =
                    gst::methods::release_v4::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
            }
            else if (is_release_v5)
            {
                const auto result =
                    gst::methods::release_v5::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
            }
            else if (is_release_v6)
            {
                const auto result =
                    gst::methods::release_v6::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
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
            else if (is_test19)
            {
                auto result = gst::methods::test19::SolveOneQuery(graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                test19_stats = std::move(result.stats);
            }
            else if (is_test21)
            {
                auto result = gst::methods::test21_anchor_half::SolveOneQuery(
                    graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                test21_stats = std::move(result.stats);
            }
            else if (is_test80)
            {
                auto result = gst::methods::test80_anchor_progressive::SolveOneQuery(
                    graph, queries[qi]);
                feasible = result.feasible;
                best_weight = result.best_weight;
                test80_stats = std::move(result.stats);
            }

            const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const std::uint64_t query_peak_rss_bytes = query_memory.Stop();
            gst::ProcessMemoryUsage memory_after = gst::GetProcessMemoryUsage();
            // Keep the established stats field name, but make its value local
            // to this query rather than the process-lifetime prefix maximum.
            memory_after.peak_rss_bytes = query_peak_rss_bytes;
            const std::string weight_str = feasible ? FormatDouble(best_weight) : "-1";
            output_manager.AppendMainResultLine(
                FormatDouble(sec, 6) + " " + weight_str + " " +
                FormatDouble(gst::BytesToMiB(query_peak_rss_bytes), 3));
            std::cout << "[Query " << query_id << "] time=" << FormatDouble(sec, 6)
                      << " sec, weight=" << weight_str
                      << ", rss=" << FormatDouble(gst::BytesToMiB(memory_after.current_rss_bytes), 3)
                      << " MiB, query_peak="
                      << FormatDouble(gst::BytesToMiB(query_peak_rss_bytes), 3)
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
                     << " merge_gate=" << pruned_stats.merge_cost_gate_pass
                     << " state_storage=" << pruned_stats.state_storage
                     << " mst_upper="
                     << (pruned_stats.use_mst_upper_bound ? "on" : "off")
                     << " lb2_pathmax="
                     << (pruned_stats.enforce_lb2_pathmax ? "on" : "off")
                     << " discovered=" << pruned_stats.discovered_states
                     << " reopened=" << pruned_stats.reopened_states
                     << " mst_calls=" << pruned_stats.mst_calls
                     << " mst_updates=" << pruned_stats.mst_improvements
                     << " mst_edges=" << pruned_stats.mst_input_edges;
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
            else if (is_release_v1)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                     << " n=" << release_stats.n
                     << " m=" << release_stats.m
                     << " g=" << release_stats.g
                     << " saved_states=" << release_stats.saved_states
                     << " peak_live_states=" << release_stats.peak_live_states
                     << " group_dist_ms=" << FormatDouble(release_stats.group_distance_ms, 3)
                     << " tsp_ms=" << FormatDouble(release_stats.tsp_ms, 3)
                     << " upper_ms=" << FormatDouble(release_stats.upper_bound_ms, 3)
                     << " dp_ms=" << FormatDouble(release_stats.dp_ms, 3)
                     << " total_ms=" << FormatDouble(release_stats.total_ms, 3);
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_release_v2)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                     << " n=" << release_v2_stats.n
                     << " m=" << release_v2_stats.m
                     << " g=" << release_v2_stats.g
                     << " settled_labels=" << release_v2_stats.settled_labels
                     << " created_labels=" << release_v2_stats.created_labels
                     << " peak_open_labels=" << release_v2_stats.peak_open_labels
                     << " group_dist_ms=" << FormatDouble(release_v2_stats.group_distance_ms, 3)
                     << " lower_ms=" << FormatDouble(release_v2_stats.lower_bound_ms, 3)
                     << " upper_ms=" << FormatDouble(release_v2_stats.upper_bound_ms, 3)
                     << " dual_ms=" << FormatDouble(release_v2_stats.dual_ms, 3)
                     << " search_ms=" << FormatDouble(release_v2_stats.search_ms, 3)
                     << " total_ms=" << FormatDouble(release_v2_stats.total_ms, 3);
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_release_v3)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                     << " n=" << release_v3_stats.n
                     << " m=" << release_v3_stats.m
                     << " g=" << release_v3_stats.g
                     << " row_work=" << release_v3_stats.row_work
                     << " buy_work=" << release_v3_stats.dual_cut_build_work
                     << " row_bytes=" << release_v3_stats.distance_bytes
                     << " switch_size=" << release_v3_stats.global_switch_size
                     << " switch_masks=" << release_v3_stats.global_switch_masks_in_size
                     << " anchor_group=" << release_v3_stats.global_anchor_group
                     << " anchor_distance="
                     << FormatDouble(release_v3_stats.global_anchor_distance, 10)
                     << " global_used=" << release_v3_stats.global_used
                     << " settled_labels=" << release_v3_stats.global_settled_labels
                     << " created_labels=" << release_v3_stats.global_created_labels
                     << " peak_open_labels=" << release_v3_stats.global_peak_open_labels
                     << " group_dist_ms="
                     << FormatDouble(release_v3_stats.group_distance_ms, 3)
                     << " tsp_ms=" << FormatDouble(release_v3_stats.tsp_ms, 3)
                     << " upper_ms=" << FormatDouble(release_v3_stats.upper_bound_ms, 3)
                     << " rows_ms=" << FormatDouble(release_v3_stats.dp_ms, 3)
                     << " dual_ms=" << FormatDouble(release_v3_stats.dual_cut_ms, 3)
                     << " global_ms=" << FormatDouble(release_v3_stats.global_ms, 3)
                     << " total_ms=" << FormatDouble(release_v3_stats.total_ms, 3);
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_release_v4)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str;
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_release_v5)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str;
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_release_v6)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str;
                AppendRuntimeStats(line, sec, memory_before, memory_after);
                output_manager.AppendResultLine(stats_filename, line.str());
            }
            else if (is_test80)
            {
                std::ostringstream line;
                line << "query=" << query_id << " best=" << weight_str
                      << " n=" << test80_stats.n
                      << " m=" << test80_stats.m
                      << " g=" << test80_stats.g
                      << " ablation_stage=" << test80_stats.ablation_stage
                      << " root=" << test80_stats.root_star_root
                     << " anchor_group=" << test80_stats.anchor_group
                     << " min_group_size=" << test80_stats.min_group_size
                     << " max_group_size=" << test80_stats.max_group_size
                     << " anchor_group_size=" << test80_stats.anchor_group_size
                     << " group_vertices=" << test80_stats.total_group_vertices
                     << " group_dist_bytes=" << test80_stats.group_distance_bytes
                     << " root_star_upper="
                     << FormatDouble(test80_stats.root_star_upper, 10)
                     << " dual_primal_upper="
                     << FormatDouble(test80_stats.dual_primal_upper, 10)
                     << " root_tour_lower="
                     << FormatDouble(test80_stats.root_tour_lower, 10)
                     << " root_dual_lower="
                     << FormatDouble(test80_stats.root_dual_lower, 10)
                     << " dual_built=" << test80_stats.dual_built
                     << " dual_groups_built=" << test80_stats.dual_groups_built
                     << " dual_trigger_size=" << test80_stats.dual_trigger_size
                     << " dual_trigger_rows=" << test80_stats.dual_trigger_rows
                     << " dual_rent_work=" << test80_stats.dual_rent_work
                     << " dual_buy_work=" << test80_stats.dual_buy_work
                     << " dual_step_buy_work=" << test80_stats.dual_step_buy_work
                     << " dual_bank_work=" << test80_stats.dual_bank_work
                     << " root_group_dist_min="
                     << FormatDouble(test80_stats.root_group_distance_min, 10)
                     << " root_group_dist_avg="
                     << FormatDouble(test80_stats.root_group_distance_average, 10)
                     << " root_group_dist_second_max="
                     << FormatDouble(test80_stats.root_group_distance_second_max, 10)
                     << " root_group_dist_max="
                     << FormatDouble(test80_stats.root_group_distance_max, 10)
                     << " junction_before="
                     << FormatDouble(test80_stats.junction_before, 10)
                     << " junction_upper="
                     << FormatDouble(test80_stats.junction_upper, 10)
                     << " junction_after="
                     << FormatDouble(test80_stats.junction_after, 10)
                     << " junction_path_vertices="
                     << test80_stats.junction_path_vertices
                     << " junction_candidates="
                     << test80_stats.junction_candidate_roots
                     << " junction_tree_vertices="
                     << test80_stats.junction_tree_vertices
                     << " junction_triple_scans="
                     << test80_stats.junction_triple_scans
                     << " junction_convolutions="
                     << test80_stats.junction_convolutions
                     << " junction_work=" << test80_stats.junction_work
                     << " pair_work=" << test80_stats.pair_work
                     << " packing_budget=" << test80_stats.packing_budget
                     << " packing_trigger_pair_rows="
                     << test80_stats.packing_trigger_pair_rows
                     << " packing_trigger_pair_work="
                     << test80_stats.packing_trigger_pair_work
                     << " packing_trigger_pair_values="
                     << test80_stats.packing_trigger_pair_values
                     << " packing_rounds=" << test80_stats.packing_rounds
                     << " packing_scale_min="
                     << FormatDouble(test80_stats.packing_min_scale, 10)
                     << " packing_scale_avg="
                     << FormatDouble(test80_stats.packing_average_scale, 10)
                     << " packing_scale_max="
                     << FormatDouble(test80_stats.packing_max_scale, 10)
                     << " ordinary_values=" << test80_stats.ordinary_values
                      << " ordinary_branches="
                      << test80_stats.ordinary_branch_values
                      << " ordinary_pops=" << test80_stats.ordinary_queue_pops
                      << " anchored_values=" << test80_stats.anchored_values
                     << " anchored_pops=" << test80_stats.anchored_queue_pops
                     << " anchored_touched=" << test80_stats.anchored_touched_values
                     << " anchored_settled=" << test80_stats.anchored_settled_values
                     << " anchored_peak_queue=" << test80_stats.anchored_peak_queue
                     << " anchored_merge_probes=" << test80_stats.anchored_merge_probes
                     << " anchored_stream_peak_rows="
                     << test80_stats.anchored_stream_peak_rows
                     << " anchored_stream_peak_bytes="
                     << test80_stats.anchored_stream_peak_bytes
                     << " anchored_stream_released_rows="
                     << test80_stats.anchored_stream_released_rows
                     << " anchored_stream_consumers="
                     << test80_stats.anchored_stream_consumers
                     << " d_join_direct_calls=" << test80_stats.ordinary_join_direct_calls
                     << " d_join_direct_work=" << test80_stats.ordinary_join_direct_work
                     << " d_join_binary_calls=" << test80_stats.ordinary_join_binary_calls
                     << " d_join_binary_work=" << test80_stats.ordinary_join_binary_work
                     << " d_join_linear_calls=" << test80_stats.ordinary_join_linear_calls
                     << " d_join_linear_work=" << test80_stats.ordinary_join_linear_work
                     << " a_join_direct_calls=" << test80_stats.anchored_join_direct_calls
                     << " a_join_direct_work=" << test80_stats.anchored_join_direct_work
                     << " a_join_binary_calls=" << test80_stats.anchored_join_binary_calls
                     << " a_join_binary_work=" << test80_stats.anchored_join_binary_work
                     << " a_join_linear_calls=" << test80_stats.anchored_join_linear_calls
                     << " a_join_linear_work=" << test80_stats.anchored_join_linear_work
                     << " completion_rows=" << test80_stats.completion_rows
                     << " completion_vertices=" << test80_stats.completion_vertices
                      << " completion_scan_vertices="
                      << test80_stats.completion_scan_vertices
                      << " completion_checks=" << test80_stats.completion_checks
                      << " completion_partitions="
                      << test80_stats.completion_partitions
                      << " completion_partition_min_rejects="
                      << test80_stats.completion_partition_min_rejects
                      << " completion_root_min_rejects="
                      << test80_stats.completion_root_min_rejects
                      << " completion_component_min_rejects="
                      << test80_stats.completion_component_min_rejects
                      << " completion_bitmap_root_rows="
                      << test80_stats.completion_bitmap_root_rows
                      << " completion_bitmap_calls="
                      << test80_stats.completion_bitmap_calls
                      << " completion_bitmap_words="
                      << test80_stats.completion_bitmap_words
                      << " streamed_a0_partitions="
                      << test80_stats.streamed_a0_partitions
                      << " streamed_a0_checks=" << test80_stats.streamed_a0_checks
                      << " streamed_a0_updates=" << test80_stats.streamed_a0_updates
                      << " streamed_a0_before="
                      << FormatDouble(test80_stats.streamed_a0_before, 10)
                      << " streamed_a0_after="
                      << FormatDouble(test80_stats.streamed_a0_after, 10)
                      << " streamed_a0_ms="
                      << FormatDouble(test80_stats.streamed_a0_ms, 3)
                     << " early_upper_partitions="
                     << test80_stats.early_upper_partitions
                     << " early_upper_probes=" << test80_stats.early_upper_probes
                     << " early_witness_values="
                     << test80_stats.early_witness_values
                     << " early_witness_pops=" << test80_stats.early_witness_pops
                     << " early_upper_before="
                     << FormatDouble(test80_stats.early_upper_before, 10)
                     << " early_upper_after="
                     << FormatDouble(test80_stats.early_upper_after, 10)
                     << " early_witness_before="
                     << FormatDouble(test80_stats.early_witness_before, 10)
                     << " early_witness_after="
                     << FormatDouble(test80_stats.early_witness_after, 10)
                     << " early_upper_ms="
                     << FormatDouble(test80_stats.early_upper_ms, 3)
                     << " group_dist_ms="
                     << FormatDouble(test80_stats.group_distance_ms, 3)
                     << " dual_ms=" << FormatDouble(test80_stats.dual_ms, 3)
                     << " dual_seed_arc_scans="
                     << test80_stats.dual_seed_arc_scans
                     << " dual_full_seed_arc_scans="
                     << test80_stats.dual_full_seed_arc_scans
                     << " dual_changed_arcs=" << test80_stats.dual_changed_arcs
                     << " junction_ms=" << FormatDouble(test80_stats.junction_ms, 3)
                     << " packing_ms=" << FormatDouble(test80_stats.packing_ms, 3)
                     << " ordinary_ms=" << FormatDouble(test80_stats.ordinary_ms, 3)
                     << " anchored_ms=" << FormatDouble(test80_stats.anchored_ms, 3)
                     << " completion_ms="
                     << FormatDouble(test80_stats.completion_ms, 3)
                     << " total_ms=" << FormatDouble(test80_stats.total_ms, 3);
                for (int size = 1; size <= test80_stats.half; ++size)
                    line << " d_masks_s" << size << '='
                         << test80_stats.ordinary_masks_by_size[size]
                         << " d_values_s" << size << '='
                         << test80_stats.ordinary_values_by_size[size]
                         << " d_branches_s" << size << '='
                         << test80_stats.ordinary_branch_values_by_size[size]
                         << " d_pops_s" << size << '='
                         << test80_stats.ordinary_pops_by_size[size]
                         << " d_dense_rows_s" << size << '='
                         << test80_stats.ordinary_dense_rows_by_size[size]
                         << " d_bitmap_rows_s" << size << '='
                         << test80_stats.ordinary_bitmap_rows_by_size[size]
                         << " d_sparse_rows_s" << size << '='
                         << test80_stats.ordinary_sparse_rows_by_size[size]
                         << " d_row_bytes_s" << size << '='
                         << test80_stats.ordinary_row_bytes_by_size[size]
                         << " d_seed_candidates_s" << size << '='
                         << test80_stats.ordinary_seed_candidates_by_size[size]
                         << " d_seed_old_s" << size << '='
                         << test80_stats.ordinary_seed_reject_old_by_size[size]
                         << " d_seed_bound_s" << size << '='
                         << test80_stats.ordinary_seed_reject_bound_by_size[size]
                         << " d_seed_accept_s" << size << '='
                         << test80_stats.ordinary_seed_accept_by_size[size]
                         << " d_relax_s" << size << '='
                         << test80_stats.ordinary_relax_attempts_by_size[size]
                         << " d_relax_old_s" << size << '='
                         << test80_stats.ordinary_relax_reject_old_by_size[size]
                         << " d_relax_bound_s" << size << '='
                         << test80_stats.ordinary_relax_reject_bound_by_size[size]
                         << " d_relax_accept_s" << size << '='
                         << test80_stats.ordinary_relax_accept_by_size[size]
                         << " d_pop_stale_s" << size << '='
                         << test80_stats.ordinary_pop_stale_by_size[size]
                         << " d_pop_bound_s" << size << '='
                         << test80_stats.ordinary_pop_bound_by_size[size]
                         << " d_h_evals_s" << size << '='
                         << test80_stats.ordinary_h_evals_by_size[size]
                         << " d_h_far_evals_s" << size << '='
                         << test80_stats.ordinary_h_farthest_evals_by_size[size]
                         << " d_h_tour_evals_s" << size << '='
                         << test80_stats.ordinary_h_tour_evals_by_size[size]
                         << " d_h_far_s" << size << '='
                         << test80_stats.ordinary_h_farthest_by_size[size]
                         << " d_h_tour_s" << size << '='
                         << test80_stats.ordinary_h_tour_by_size[size]
                         << " d_h_dual_s" << size << '='
                         << test80_stats.ordinary_h_dual_by_size[size]
                         << " d_ms_s" << size << '='
                         << FormatDouble(test80_stats.ordinary_ms_by_size[size], 3)
                         << " best_after_d_s" << size << '='
                         << FormatDouble(test80_stats.best_after_ordinary_size[size], 10)
                         << " anchor_facility_upper_s" << size << '='
                         << FormatDouble(
                                test80_stats.anchor_facility_upper_by_size[size], 10)
                         << " anchor_facility_probes_s" << size << '='
                         << test80_stats.anchor_facility_probes_by_size[size]
                         << " anchor_facility_ms_s" << size << '='
                         << FormatDouble(
                                test80_stats.anchor_facility_ms_by_size[size], 3)
                         << " anchor_tree_upper_s" << size << '='
                         << FormatDouble(
                                test80_stats.anchor_tree_upper_by_size[size], 10)
                         << " anchor_tree_probes_s" << size << '='
                         << test80_stats.anchor_tree_probes_by_size[size]
                         << " anchor_tree_vertices_s" << size << '='
                         << test80_stats.anchor_tree_vertices_by_size[size]
                         << " anchor_tree_rent_s" << size << '='
                         << test80_stats.anchor_tree_rent_by_size[size]
                         << " anchor_tree_evaluated_s" << size << '='
                         << test80_stats.anchor_tree_evaluated_by_size[size]
                         << " anchor_tree_ms_s" << size << '='
                         << FormatDouble(test80_stats.anchor_tree_ms_by_size[size], 3);
                line << " anchor_tree_buy_work="
                     << test80_stats.anchor_tree_buy_work
                     << " anchor_tree_evaluations="
                     << test80_stats.anchor_tree_evaluations;
                for (int first = 0; first < test80_stats.g; ++first)
                    for (int second = first + 1; second < test80_stats.g; ++second)
                    {
                        const long long values =
                            test80_stats.ordinary_pair_values[first * 16 + second];
                        if (values)
                            line << " d2_pair_values_g" << first + 1 << "_g"
                                 << second + 1 << '=' << values;
                    }
#ifdef GST_TEST80_ADJOINT_ANCHOR
                line << " adjoint_cut=" << test80_stats.adjoint_cut
                     << " adjoint_rows=" << test80_stats.adjoint_rows
                     << " adjoint_values=" << test80_stats.adjoint_values
                     << " adjoint_pops=" << test80_stats.adjoint_pops
                     << " adjoint_join_checks="
                     << test80_stats.adjoint_join_checks
                     << " adjoint_terminal_checks="
                     << test80_stats.adjoint_terminal_checks
                     << " adjoint_transition_checks="
                     << test80_stats.adjoint_transition_checks
                     << " adjoint_boundary_checks="
                     << test80_stats.adjoint_boundary_checks
                     << " adjoint_boundary_updates="
                     << test80_stats.adjoint_boundary_updates
                     << " adjoint_row_bytes=" << test80_stats.adjoint_row_bytes
                     << " adjoint_transpose_values="
                     << test80_stats.adjoint_transpose_values
                     << " adjoint_transpose_pair_probes="
                     << test80_stats.adjoint_transpose_pair_probes
                     << " adjoint_transpose_global_pair_probes="
                     << test80_stats.adjoint_transpose_global_pair_probes
                     << " adjoint_transpose_submask_pair_probes="
                     << test80_stats.adjoint_transpose_submask_pair_probes
                     << " adjoint_transpose_global_vertices="
                     << test80_stats.adjoint_transpose_global_vertices
                     << " adjoint_transpose_submask_vertices="
                     << test80_stats.adjoint_transpose_submask_vertices
                     << " adjoint_transpose_disjoint_pairs="
                     << test80_stats.adjoint_transpose_disjoint_pairs
                     << " adjoint_transpose_events="
                     << test80_stats.adjoint_transpose_events
                     << " adjoint_transpose_row_bytes="
                     << test80_stats.adjoint_transpose_row_bytes
                     << " adjoint_transpose_ms="
                     << FormatDouble(test80_stats.adjoint_transpose_ms, 3)
                     << " adjoint_ms="
                     << FormatDouble(test80_stats.adjoint_ms, 3);
                for (int size = test80_stats.adjoint_cut + 1;
                     size < test80_stats.half;
                     ++size)
                    line << " adjoint_terminal_checks_s" << size << '='
                         << test80_stats.adjoint_terminal_checks_by_size[size]
                         << " adjoint_transition_checks_s" << size << '='
                         << test80_stats.adjoint_transition_checks_by_size[size]
                         << " adjoint_terminal_ms_s" << size << '='
                         << FormatDouble(
                                test80_stats.adjoint_terminal_ms_by_size[size], 3)
                         << " adjoint_transition_ms_s" << size << '='
                         << FormatDouble(
                                test80_stats.adjoint_transition_ms_by_size[size], 3)
                         << " adjoint_closure_ms_s" << size << '='
                         << FormatDouble(
                                test80_stats.adjoint_closure_ms_by_size[size], 3)
                         << " adjoint_boundary_checks_s" << size << '='
                         << test80_stats.adjoint_boundary_checks_by_size[size]
                         << " adjoint_boundary_ms_s" << size << '='
                         << FormatDouble(
                                test80_stats.adjoint_boundary_ms_by_size[size], 3)
                         << " best_after_adjoint_s" << size << '='
                         << FormatDouble(
                                test80_stats.best_after_adjoint_size[size], 10);
#endif
                for (int size = 0; size < test80_stats.half; ++size)
                    line << " a_masks_s" << size << '='
                         << test80_stats.anchored_masks_by_size[size]
                         << " a_values_s" << size << '='
                         << test80_stats.anchored_values_by_size[size]
                         << " a_touched_s" << size << '='
                         << test80_stats.anchored_touched_by_size[size]
                         << " a_settled_s" << size << '='
                         << test80_stats.anchored_settled_by_size[size]
                         << " a_pops_s" << size << '='
                         << test80_stats.anchored_pops_by_size[size]
                         << " a_merge_probes_s" << size << '='
                         << test80_stats.anchored_merge_probes_by_size[size]
                         << " a_dense_rows_s" << size << '='
                         << test80_stats.anchored_dense_rows_by_size[size]
                         << " a_bitmap_rows_s" << size << '='
                         << test80_stats.anchored_bitmap_rows_by_size[size]
                         << " a_sparse_rows_s" << size << '='
                         << test80_stats.anchored_sparse_rows_by_size[size]
                         << " a_row_bytes_s" << size << '='
                         << test80_stats.anchored_row_bytes_by_size[size]
                         << " a_seed_candidates_s" << size << '='
                         << test80_stats.anchored_seed_candidates_by_size[size]
                         << " a_seed_old_s" << size << '='
                         << test80_stats.anchored_seed_reject_old_by_size[size]
                         << " a_seed_bound_s" << size << '='
                         << test80_stats.anchored_seed_reject_bound_by_size[size]
                         << " a_seed_accept_s" << size << '='
                         << test80_stats.anchored_seed_accept_by_size[size]
                         << " a_relax_s" << size << '='
                         << test80_stats.anchored_relax_attempts_by_size[size]
                         << " a_relax_old_s" << size << '='
                         << test80_stats.anchored_relax_reject_old_by_size[size]
                         << " a_relax_bound_s" << size << '='
                         << test80_stats.anchored_relax_reject_bound_by_size[size]
                         << " a_relax_accept_s" << size << '='
                         << test80_stats.anchored_relax_accept_by_size[size]
                         << " a_pop_stale_s" << size << '='
                         << test80_stats.anchored_pop_stale_by_size[size]
                         << " a_pop_bound_s" << size << '='
                         << test80_stats.anchored_pop_bound_by_size[size]
                         << " a_h_evals_s" << size << '='
                         << test80_stats.anchored_h_evals_by_size[size]
                         << " a_h_far_evals_s" << size << '='
                         << test80_stats.anchored_h_farthest_evals_by_size[size]
                         << " a_h_tour_evals_s" << size << '='
                         << test80_stats.anchored_h_tour_evals_by_size[size]
                         << " a_h_far_s" << size << '='
                         << test80_stats.anchored_h_farthest_by_size[size]
                         << " a_h_tour_s" << size << '='
                         << test80_stats.anchored_h_tour_by_size[size]
                         << " a_h_dual_s" << size << '='
                         << test80_stats.anchored_h_dual_by_size[size]
                         << " completion_rows_s" << size << '='
                         << test80_stats.completion_rows_by_size[size]
                         << " completion_scan_s" << size << '='
                         << test80_stats.completion_scan_vertices_by_size[size]
                         << " completion_checks_s" << size << '='
                         << test80_stats.completion_checks_by_size[size]
                         << " completion_partitions_s" << size << '='
                         << test80_stats.completion_partitions_by_size[size]
                         << " completion_partition_min_rejects_s" << size << '='
                         << test80_stats.completion_partition_min_rejects_by_size[size]
                         << " completion_root_min_rejects_s" << size << '='
                         << test80_stats.completion_root_min_rejects_by_size[size]
                         << " completion_component_min_rejects_s" << size << '='
                         << test80_stats.completion_component_min_rejects_by_size[size]
                         << " completion_bitmap_calls_s" << size << '='
                         << test80_stats.completion_bitmap_calls_by_size[size]
                         << " completion_bitmap_words_s" << size << '='
                         << test80_stats.completion_bitmap_words_by_size[size]
                         << " completion_ms_s" << size << '='
                         << FormatDouble(test80_stats.completion_ms_by_size[size], 3)
                         << " a_ms_s" << size << '='
                         << FormatDouble(test80_stats.anchored_ms_by_size[size], 3)
                         << " best_after_a_s" << size << '='
                         << FormatDouble(test80_stats.best_after_anchored_size[size], 10);
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
                AppendTest18FamilyStats(output_manager, stats_filename, query_id, weight_str,
                                        test18_stats, sec, memory_before, memory_after);
            }
            else if (is_test19)
            {
                AppendTest18FamilyStats(output_manager, stats_filename, query_id, weight_str,
                                        test19_stats, sec, memory_before, memory_after);
            }
            else if (is_test21)
            {
                std::ostringstream line;
                line << "query=" << query_id
                     << " best=" << weight_str
                     << " n=" << test21_stats.n
                     << " m=" << test21_stats.m
                     << " g=" << test21_stats.g
                     << " anchor_group=" << test21_stats.anchor_group
                     << " half=" << test21_stats.half
                     << " original_half_masks=" << test21_stats.original_half_masks
                     << " ordinary_masks=" << test21_stats.ordinary_masks
                     << " anchored_masks=" << test21_stats.anchored_masks
                      << " ordinary_values=" << test21_stats.ordinary_values
                     << " ordinary_branch_values=" << test21_stats.ordinary_branch_values
                     << " anchored_values=" << test21_stats.anchored_values
                     << " ordinary_pops=" << test21_stats.ordinary_queue_pops
                     << " anchored_pops=" << test21_stats.anchored_queue_pops
                     << " anchored_touched=" << test21_stats.anchored_touched_values
                     << " anchored_settled=" << test21_stats.anchored_settled_values
                     << " anchored_peak_queue=" << test21_stats.anchored_peak_queue
                     << " anchored_merge_probes=" << test21_stats.anchored_merge_probes
                     << " completion_rows=" << test21_stats.completion_rows
                     << " completion_vertices=" << test21_stats.completion_vertices
                     << " completion_checks=" << test21_stats.completion_checks
                     << " completion_scan_vertices="
                     << test21_stats.completion_scan_vertices
                     << " early_upper_partitions=" << test21_stats.early_upper_partitions
                     << " early_upper_probes=" << test21_stats.early_upper_probes
                     << " early_witness_values=" << test21_stats.early_witness_values
                     << " early_witness_pops=" << test21_stats.early_witness_pops
                     << " quarter_upper_halves=" << test21_stats.quarter_upper_halves
                     << " quarter_upper_pair_probes="
                     << test21_stats.quarter_upper_pair_probes
                     << " quarter_upper_join_probes="
                     << test21_stats.quarter_upper_join_probes
                     << " quarter_witness_values="
                     << test21_stats.quarter_witness_values
                     << " quarter_witness_pops=" << test21_stats.quarter_witness_pops
                     << " group_dist_ms=" << FormatDouble(test21_stats.group_distance_ms, 3)
                     << " ordinary_ms=" << FormatDouble(test21_stats.ordinary_ms, 3)
                     << " dual_ms=" << FormatDouble(test21_stats.dual_ms, 3)
                     << " anchored_ms=" << FormatDouble(test21_stats.anchored_ms, 3)
                     << " completion_ms=" << FormatDouble(test21_stats.completion_ms, 3)
                     << " early_upper_before="
                     << FormatDouble(test21_stats.early_upper_before, 10)
                     << " early_upper_after="
                     << FormatDouble(test21_stats.early_upper_after, 10)
                     << " early_witness_before="
                     << FormatDouble(test21_stats.early_witness_before, 10)
                     << " early_witness_after="
                     << FormatDouble(test21_stats.early_witness_after, 10)
                     << " early_upper_ms=" << FormatDouble(test21_stats.early_upper_ms, 3)
                     << " quarter_upper_before="
                     << FormatDouble(test21_stats.quarter_upper_before, 10)
                     << " quarter_upper_after="
                     << FormatDouble(test21_stats.quarter_upper_after, 10)
                     << " quarter_upper_ms="
                     << FormatDouble(test21_stats.quarter_upper_ms, 3)
                     << " quarter_witness_before="
                     << FormatDouble(test21_stats.quarter_witness_before, 10)
                     << " quarter_witness_after="
                     << FormatDouble(test21_stats.quarter_witness_after, 10)
                     << " total_ms=" << FormatDouble(test21_stats.total_ms, 3);
                for (int size = 1; size <= test21_stats.half; ++size)
                    line << " d_values_s" << size << '='
                         << test21_stats.ordinary_values_by_size[size]
                         << " d_branches_s" << size << '='
                         << test21_stats.ordinary_branch_values_by_size[size]
                         << " d_seeds_s" << size << '='
                         << test21_stats.ordinary_seeds_by_size[size]
                         << " d_pops_s" << size << '='
                         << test21_stats.ordinary_pops_by_size[size]
                         << " d_ms_s" << size << '='
                         << FormatDouble(test21_stats.ordinary_ms_by_size[size], 3);
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
