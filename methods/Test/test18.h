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
    int total_group_vertices = 0, max_group_size = 0;
    long long total_inqueue = 0;
    long long pull_pairs = 0, pull_scan = 0, pull_hits = 0;
    long long complement_pairs = 0, complement_scan = 0, complement_hits = 0;
    long long complement_calls = 0, complement_skip_early = 0;
    long long complement_mask_pairs = 0, complement_mask_empty = 0;
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
    long long final_pruned_full = 0, final_pruned_need = 0;
    long long stale_need_skips = 0, lookup_need_skips = 0;
    long long compact_calls = 0, compact_removed = 0;
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
    std::vector<long long> total_by_size;
    std::vector<long long> active_by_size, inqueue_by_size, merge_by_size;
    std::vector<long long> early_cover_by_rem_size;
    std::vector<long long> early_cover_ready_by_rem_size;
    std::vector<long long> early_cover_better_by_rem_size;
    std::vector<long long> pair_saved_by_cover_size;
    std::vector<long long> pair_saved_slack_rel_bucket;
    std::vector<long long> dense_rows_by_size, dense_states_by_size;
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
