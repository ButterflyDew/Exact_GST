#ifndef GST_METHODS_TEST_TEST2_H
#define GST_METHODS_TEST_TEST2_H

#include <memory>
#include <vector>

#include "../../answer_tree.h"
#include "../../graph_io.h"
#include "../../query_io.h"
#include "../DPBF/dpbf_solver.h"

namespace gst::methods::test2
{

struct Test2Stats
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
    Test2Stats stats;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query, dpbf::OutputMode output_mode);

}  // namespace gst::methods::test2

#endif  // GST_METHODS_TEST_TEST2_H
