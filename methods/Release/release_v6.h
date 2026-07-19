#ifndef GST_METHODS_RELEASE_RELEASE_V6_H
#define GST_METHODS_RELEASE_RELEASE_V6_H

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::release_v6
{
struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::release_v6

#endif
