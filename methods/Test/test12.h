#ifndef GST_METHODS_TEST_TEST12_H
#define GST_METHODS_TEST_TEST12_H

#include <vector>

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::test12
{
    struct Test12Stats
    {
        long long total_valid = 0;
        long long total_up_subset = 0;
        long long total_inqueue = 0;
        long long merge_bucket_scans = 0;
        long long merge_best_checks = 0;
        long long merge_live_dp_checks = 0;
        long long merge_future_h_checks = 0;
        long long live_dp_pruned_ge_best = 0;
        long long live_dp_pruned_far = 0;
        long long live_dp_closed_layers = 0;
        long long active_seed_vertices = 0;
        long long full_seed_vertices_would = 0;
        long long target_seed = 0;
        long long prefix_considered = 0;
        long long prefix_pushed = 0;
        long long prefix_pruned_lb = 0;
        long long target_promoted = 0;
        long long target_demoted = 0;
        long long modify_original_target = 0;
        long long modify_promoted_target = 0;
        long long modify_init = 0;
        long long modify_calls = 0;
        long long modify_no_effect = 0;
        long long modify_best_effect = 0;
        long long modify_live_effect = 0;
        long long modify_h_effect = 0;
        long long modify_only_h_effect = 0;
        long long modify_only_live_effect = 0;
        long long modify_only_best_effect = 0;
        long long modify_multi_effect = 0;
        long long modify_no_live_candidate = 0;
        long long modify_no_h_candidate = 0;
        long long modify_no_best_candidate = 0;
        long long modify_no_candidate_all = 0;
        long long init_no_effect = 0;
        long long init_only_h = 0;
        long long original_no_effect = 0;
        long long original_only_h = 0;
        long long promoted_no_effect = 0;
        long long promoted_only_h = 0;
        long long fast_no_candidate_return = 0;
        long long precheck_calls = 0;
        long long precheck_scans = 0;
        long long precheck_no_candidate = 0;
        long long pending_promoted = 0;
        long long pending_promoted_unique = 0;
        long long pending_layer_roots = 0;
        long long pending_layer_repeat_roots = 0;
        int max_pending_roots_in_layer = 0;
        long long pop_target = 0;
        long long pop_prefix = 0;
        long long pop_skip_stale = 0;
        long long pop_skip_lb = 0;
        long long lb_calls = 0;
        long long pq_pushes = 0;
        long long pq_pops = 0;
        long long relax_attempts = 0;
        long long relax_success = 0;
        long long masks_processed = 0;
        int max_active_vertices = 0;
        double root_star_upper = -1.0;
        double greedy_upper = -1.0;
        long long greedy_pops = 0;
        double preprocess_ms = 0.0;
        double dp_ms = 0.0;
        std::vector<long long> valid_by_size;
        std::vector<long long> total_by_size;
        std::vector<long long> active_by_size;
        std::vector<long long> inqueue_by_size;
        std::vector<long long> merge_by_size;
    };

    struct SolveResult
    {
        double best_weight = -1.0;
        bool feasible = false;
        Test12Stats stats;
    };

SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::test12

#endif  // GST_METHODS_TEST_TEST12_H
