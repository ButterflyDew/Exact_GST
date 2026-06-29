#ifndef GST_METHODS_HALF_DPBF_SOLVER_H
#define GST_METHODS_HALF_DPBF_SOLVER_H

#include <vector>

#include "../../graph_io.h"
#include "../../query_io.h"

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
    HalfDpbfStats stats;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::half_dpbf

#endif  // GST_METHODS_HALF_DPBF_SOLVER_H
