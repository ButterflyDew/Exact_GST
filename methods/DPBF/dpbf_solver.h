#ifndef GST_METHODS_DPBF_SOLVER_H
#define GST_METHODS_DPBF_SOLVER_H

#include <memory>
#include <vector>

#include "../../answer_tree.h"
#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::dpbf
{

enum class OutputMode
{
    kWeightOnly = 0,
    kConcreteTree = 1,
    kVirtualTree = 2,
};

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    std::unique_ptr<AnswerTreeBase> answer;
};

SolveResult SolveOneQuery(const Graph& graph,
                         const Query& query,
                         OutputMode output_mode,
                         VirtualRootPolicy root_policy = VirtualRootPolicy::kMinMaxChildThenHeight);

}  // namespace gst::methods::dpbf

#endif  // GST_METHODS_DPBF_SOLVER_H
