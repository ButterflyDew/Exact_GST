#ifndef GST_METHODS_TEST_TEST11_H
#define GST_METHODS_TEST_TEST11_H

#include <vector>

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::test11
{
    struct Test11Stats
    {
        long long total_valid = 0;
        long long total_up_subset = 0;
        long long total_inqueue = 0;
        int n = 0;
        int m = 0;
        int g = 0;
        int total_group_vertices = 0;
        int max_group_size = 0;
        long long merge_bucket_scans = 0;
        long long merge_best_checks = 0;
        long long merge_live_dp_checks = 0;
        long long merge_future_h_checks = 0;
        long long live_dp_pruned_ge_best = 0;
        long long live_dp_pruned_far = 0;
        long long live_dp_closed_layers = 0;
        long long active_seed_vertices = 0;
        long long full_seed_vertices_would = 0;
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
        double group_dist_ms = 0.0;
        double group_pair_ms = 0.0;
        double greedy_ms = 0.0;
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
        Test11Stats stats;
    };

SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::test11

#endif  // GST_METHODS_TEST_TEST11_H
