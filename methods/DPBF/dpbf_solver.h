#ifndef GST_METHODS_DPBF_SOLVER_H
#define GST_METHODS_DPBF_SOLVER_H

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::dpbf
{

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::dpbf

#endif  // GST_METHODS_DPBF_SOLVER_H
