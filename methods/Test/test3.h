#ifndef GST_METHODS_TEST_TEST3_H
#define GST_METHODS_TEST_TEST3_H

#include <memory>
#include <vector>

#include "../../answer_tree.h"
#include "../../graph_io.h"
#include "../../query_io.h"
#include "../DPBF/dpbf_solver.h"

namespace gst::methods::test3
{
    struct Test3Stats
    {
        long long total_valid = 0;
        std::vector<long long> valid_by_size;
        std::vector<long long> total_by_size;
    };

    struct SolveResult
    {
        double best_weight = -1.0;
        bool feasible = false;
        std::unique_ptr<AnswerTreeBase> answer;
        Test3Stats stats;
    };

    SolveResult SolveOneQuery(const Graph& graph, const Query& query, dpbf::OutputMode output_mode);

}  // namespace gst::methods::test3

#endif  // GST_METHODS_TEST_TEST3_H
