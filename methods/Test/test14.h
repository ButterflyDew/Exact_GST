#ifndef GST_METHODS_TEST_TEST14_H
#define GST_METHODS_TEST_TEST14_H

#include <vector>

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::test14
{
struct Test14Stats
{
    int n = 0, m = 0, g = 0;
    int total_group_vertices = 0, max_group_size = 0;
    long long total_valid = 0, total_inqueue = 0;
    long long pull_pairs = 0, pull_scan = 0, pull_hits = 0;
    long long h_pairs = 0, h_scan = 0, h_hits = 0;
    long long complement_pairs = 0, complement_scan = 0, complement_hits = 0;
    long long prune_ge_best = 0, prune_far = 0;
    long long active_seed = 0, lb_calls = 0;
    long long pq_push = 0, pq_pop = 0, relax_try = 0, relax_ok = 0;
    long long masks_processed = 0, greedy_pops = 0;
    long long finite_states = 0, confirmed_states = 0;
    int max_active = 0;
    double root_star_upper = -1.0, greedy_upper = -1.0;
    double preprocess_ms = 0.0, group_dist_ms = 0.0;
    double greedy_ms = 0.0, dp_ms = 0.0;
    double pull_ms = 0.0, h_ms = 0.0, complement_ms = 0.0, search_ms = 0.0;
    std::vector<long long> valid_by_size, total_by_size;
    std::vector<long long> active_by_size, inqueue_by_size, merge_by_size;
};

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    Test14Stats stats;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::test14

#endif
