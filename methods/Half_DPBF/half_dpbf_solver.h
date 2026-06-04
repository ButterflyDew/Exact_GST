#ifndef GST_METHODS_HALF_DPBF_SOLVER_H
#define GST_METHODS_HALF_DPBF_SOLVER_H

#include <memory>
#include <vector>

#include "../../answer_tree.h"
#include "../../graph_io.h"
#include "../../query_io.h"
#include "../DPBF/dpbf_solver.h"

namespace gst::methods::half_dpbf
{

struct HalfDpbfStats
{
    long long stage1_total = 0;
    long long lt_v = 0;
    long long lt_v2 = 0;
    long long lt_v4 = 0;
    long long lt_v8 = 0;
};

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    std::unique_ptr<AnswerTreeBase> answer;
    HalfDpbfStats stats;
};

SolveResult SolveOneQuery(const Graph& graph,
                         const Query& query,
                         dpbf::OutputMode output_mode,
                         VirtualRootPolicy root_policy = VirtualRootPolicy::kMinMaxChildThenHeight);

}  // namespace gst::methods::half_dpbf

#endif  // GST_METHODS_HALF_DPBF_SOLVER_H
