#ifndef GST_METHODS_PRUNED_DP_SOLVER_H
#define GST_METHODS_PRUNED_DP_SOLVER_H

#include <memory>

#include "../../answer_tree.h"
#include "../../graph_io.h"
#include "../../query_io.h"
#include "../DPBF/dpbf_solver.h"

namespace gst::methods::pruned_dp
{

struct PrunedDpStats
{
};

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    std::unique_ptr<AnswerTreeBase> answer;
    PrunedDpStats stats;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query, dpbf::OutputMode output_mode);

}  // namespace gst::methods::pruned_dp

#endif  // GST_METHODS_PRUNED_DP_SOLVER_H
