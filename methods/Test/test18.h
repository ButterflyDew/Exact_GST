#ifndef GST_METHODS_TEST_TEST18_H
#define GST_METHODS_TEST_TEST18_H

#include <vector>

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::test18
{
struct Test18Stats
{
    int n = 0, m = 0, g = 0;
    int original_n = 0, original_m = 0;
    int total_group_vertices = 0, max_group_size = 0;
    long long degree_reduce_removed_vertices = 0, degree_reduce_removed_edges = 0;
    long long degree_reduce_leaf_vertices = 0, degree_reduce_dead_vertices = 0;
    long long degree_reduce_contracted_vertices = 0;
    double degree_reduce_ms = 0.0;
    long long leaf_reduce_removed_vertices = 0, leaf_reduce_removed_edges = 0;
    long long leaf_reduce_removed_components = 0;
    long long leaf_reduce_two_portal_components = 0, leaf_reduce_two_portal_vertices = 0;
    long long leaf_reduce_two_portal_edges_added = 0;
    long long leaf_reduce_three_portal_components = 0, leaf_reduce_three_portal_vertices = 0;
    long long leaf_reduce_three_portal_hubs_added = 0, leaf_reduce_three_portal_edges_added = 0;
    long long leaf_reduce_four_portal_components = 0, leaf_reduce_four_portal_vertices = 0;
    long long leaf_reduce_four_portal_hubs_added = 0, leaf_reduce_four_portal_edges_added = 0;
    double leaf_reduce_ms = 0.0;
    long long total_inqueue = 0;
    long long pull_pairs = 0, pull_scan = 0, pull_hits = 0;
    long long complement_pairs = 0, complement_scan = 0, complement_hits = 0;
    long long complement_calls = 0, complement_skip_early = 0;
    long long complement_mask_pairs = 0, complement_mask_empty = 0;
    long long complement_cache_builds = 0, complement_cache_queries = 0;
    long long complement_cache_scan = 0, complement_cache_hits = 0;
    long long complement_cache_rent_cost = 0, complement_cache_buy_cost = 0;
    long long complement_cache_need_skips = 0;
    long long complement_cover_smaller = 0, complement_cover_possible = 0;
    long long pair_partition_roots = 0, pair_partition_updates = 0;
    long long pair_saved_cover_extra = 0, pair_saved_cover_extra_groups = 0;
    long long pair_dense_rows = 0, pair_dense_states = 0;
    long long dense_rows = 0, dense_states = 0;
    long long pair_saved_slack_count = 0;
    double pair_saved_slack_rel_sum = 0.0, pair_saved_slack_rel_max = 0.0;
    long long early_cover_extra = 0, early_cover_extra_groups = 0;
    long long early_cover_rem_le_2k = 0;
    long long early_cover_pair_ready = 0, early_cover_pair_better = 0;
    long long early_cover_pair_checks = 0, early_cover_pair_splits = 0;
    long long early_cover_best_updates = 0;
    int early_cover_first_better_size = 0;
    int early_cover_min_rem = 0;
    double early_cover_best_candidate = -1.0;
    long long global_root_alive = 0, global_root_pruned = 0;
    long long tryset_calls = 0, tryset_keep = 0;
    long long tryset_pruned_full = 0, tryset_pruned_ge_best = 0;
    long long tryset_pruned_far = 0, tryset_pruned_lb = 0;
    long long seed_try = 0, seed_push = 0;
    long long seed_block_far = 0, seed_block_lb = 0;
    long long pop_pruned_full = 0, pop_pruned_far = 0, pop_pruned_lb = 0;
    long long astar_order_enabled = 0, astar_relax_lb_pruned = 0;
    long long onetree_lb_diag_enabled = 0, onetree_lb_diag_checks = 0;
    long long onetree_lb_diag_stronger_lb = 0, onetree_lb_diag_stronger_need = 0;
    long long onetree_lb_diag_extra_seed_prune = 0, onetree_lb_diag_extra_final_prune = 0;
    double onetree_lb_diag_gain_sum = 0.0, onetree_lb_diag_gain_max = 0.0;
    double onetree_lb_diag_ms = 0.0;
    long long onetree_lb_prune_enabled = 0;
    long long onetree_lb_seed_pruned = 0, onetree_lb_final_pruned = 0;
    long long tsp_lb_diag_enabled = 0, tsp_lb_diag_checks = 0;
    long long tsp_lb_diag_stronger_lb = 0, tsp_lb_diag_stronger_need = 0;
    long long tsp_lb_diag_extra_seed_prune = 0, tsp_lb_diag_extra_final_prune = 0;
    double tsp_lb_diag_gain_sum = 0.0, tsp_lb_diag_gain_max = 0.0;
    double tsp_lb_diag_ms = 0.0;
    long long tsp_lb_order_enabled = 0;
    long long tsp_lb_order_seed_pruned = 0, tsp_lb_order_pop_pruned = 0;
    long long tsp_lb_order_relax_pruned = 0;
    long long tsp_lb_save_enabled = 0, tsp_lb_save_pruned = 0;
    long long tsp_lb_queries = 0;
    double tsp_lb_precompute_ms = 0.0, tsp_lb_ms = 0.0;
    long long final_pruned_full = 0, final_pruned_need = 0;
    long long stale_need_skips = 0, lookup_need_skips = 0;
    long long compact_calls = 0, compact_removed = 0;
    long long compact_light_removed = 0, compact_dense_removed = 0;
    long long order_pruned_rows = 0, order_pruned_states = 0;
    long long order_lb_pruned_states = 0;
    long long order_split_pruned_states = 0;
    double compact_ms = 0.0;
    long long prune_ge_best = 0, prune_far = 0;
    long long active_seed = 0, lb_calls = 0;
    long long pq_push = 0, pq_pop = 0, relax_try = 0, relax_ok = 0;
    long long masks_processed = 0, greedy_pops = 0, multi_greedy_roots = 0;
    long long finite_states = 0;
    long long best_updates = 0;
    int first_best_update_size = 0, last_best_update_size = 0;
    double root_star_upper = -1.0, greedy_upper = -1.0, multi_greedy_upper = -1.0;
    double pair_partition_upper = -1.0, pair_partition_ms = 0.0;
    double preprocess_ms = 0.0, group_dist_ms = 0.0;
    double greedy_ms = 0.0, dp_ms = 0.0;
    double pull_ms = 0.0, complement_ms = 0.0, search_ms = 0.0;
    double complement_cache_ms = 0.0;
    std::vector<long long> total_by_size;
    std::vector<long long> active_by_size, inqueue_by_size, merge_by_size;
    std::vector<long long> pull_pairs_by_size, pull_scan_by_size, pull_hits_by_size;
    std::vector<long long> pull_seed_scan_by_size, pull_seed_hits_by_size;
    std::vector<long long> pull_singleton_scan_by_size, pull_singleton_hits_by_size;
    std::vector<long long> pull_dense_dense_scan_by_size, pull_dense_dense_hits_by_size;
    std::vector<long long> pull_dense_sparse_scan_by_size, pull_dense_sparse_hits_by_size;
    std::vector<long long> pull_sparse_sparse_scan_by_size, pull_sparse_sparse_hits_by_size;
    std::vector<long long> search_seed_try_by_size, search_seed_push_by_size;
    std::vector<long long> search_pq_pop_by_size, search_relax_try_by_size, search_relax_ok_by_size;
    std::vector<long long> complement_calls_by_size, complement_scan_by_size, complement_hits_by_size;
    std::vector<long long> complement_cache_builds_by_size, complement_cache_queries_by_size;
    std::vector<long long> complement_cache_scan_by_size, complement_cache_hits_by_size;
    std::vector<long long> early_cover_by_rem_size;
    std::vector<long long> early_cover_ready_by_rem_size;
    std::vector<long long> early_cover_better_by_rem_size;
    std::vector<long long> pair_saved_by_cover_size;
    std::vector<long long> pair_saved_slack_rel_bucket;
    std::vector<long long> dense_rows_by_size, dense_states_by_size;
    std::vector<double> pull_ms_by_size, search_ms_by_size, complement_ms_by_size;
    std::vector<double> complement_cache_ms_by_size;
    std::vector<double> best_after_size;
};

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    Test18Stats stats;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::test18

#endif
