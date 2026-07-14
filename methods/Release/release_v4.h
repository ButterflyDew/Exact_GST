#ifndef GST_METHODS_RELEASE_RELEASE_V4_H
#define GST_METHODS_RELEASE_RELEASE_V4_H

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::release_v4
{
struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::release_v4

#endif
