#ifndef GST_METHODS_TEST_TEST10_H
#define GST_METHODS_TEST_TEST10_H

#include <memory>
#include <vector>

#include "../../answer_tree.h"
#include "../../graph_io.h"
#include "../../query_io.h"
#include "../DPBF/dpbf_solver.h"

namespace gst::methods::test10
{
    struct Test10Stats
    {
        long long total_valid = 0;
        long long total_up_subset = 0;
        long long total_inqueue = 0;
        long long merge_bucket_scans = 0;
        long long merge_best_checks = 0;
        long long merge_live_dp_checks = 0;
        long long merge_future_h_checks = 0;
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
        std::unique_ptr<AnswerTreeBase> answer;
        Test10Stats stats;
    };

    SolveResult SolveOneQuery(const Graph& graph, const Query& query, dpbf::OutputMode output_mode);

}  // namespace gst::methods::test10

#endif  // GST_METHODS_TEST_TEST10_H
