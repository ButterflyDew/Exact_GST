#ifndef GST_METHODS_TEST_TEST7_H
#define GST_METHODS_TEST_TEST7_H

#include <memory>
#include <vector>

#include "../../answer_tree.h"
#include "../../graph_io.h"
#include "../../query_io.h"
#include "../DPBF/dpbf_solver.h"

namespace gst::methods::test7
{
    struct Test7Stats
    {
        long long total_valid = 0;
        long long total_up_subset = 0;
        long long total_inqueue = 0;
        long long dense_up_subset_would = 0;
        long long lb_filtered_seed = 0;
        long long lb_filtered_relax = 0;
        double initial_upper = -1.0;
        std::vector<long long> valid_by_size;
        std::vector<long long> total_by_size;
    };

    struct SolveResult
    {
        double best_weight = -1.0;
        bool feasible = false;
        std::unique_ptr<AnswerTreeBase> answer;
        Test7Stats stats;
    };

    SolveResult SolveOneQuery(const Graph& graph, const Query& query, dpbf::OutputMode output_mode);

}  // namespace gst::methods::test7

#endif  // GST_METHODS_TEST_TEST7_H
