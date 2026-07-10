#ifndef GST_METHODS_RELEASE_RELEASE_V2_H
#define GST_METHODS_RELEASE_RELEASE_V2_H

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::release_v2
{
struct ReleaseStats
{
    int n = 0;
    int m = 0;
    int g = 0;
    long long settled_labels = 0;
    long long created_labels = 0;
    long long peak_open_labels = 0;
    double group_distance_ms = 0.0;
    double lower_bound_ms = 0.0;
    double upper_bound_ms = 0.0;
    double dual_ms = 0.0;
    double search_ms = 0.0;
    double total_ms = 0.0;
};

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    ReleaseStats stats;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::release_v2

#endif
