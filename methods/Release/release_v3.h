#ifndef GST_METHODS_RELEASE_RELEASE_V3_H
#define GST_METHODS_RELEASE_RELEASE_V3_H

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::release_v3
{
struct ReleaseStats
{
    int n = 0;
    int m = 0;
    int g = 0;
    long long distance_bytes = 0;
    long long dual_cut_build_work = 0;
    long long row_work = 0;
    int global_switch_size = 0;
    int global_switch_masks_in_size = 0;
    int global_anchor_group = 0;
    double global_anchor_distance = 0.0;
    long long global_created_labels = 0;
    long long global_settled_labels = 0;
    long long global_peak_open_labels = 0;
    bool global_used = false;
    double group_distance_ms = 0.0;
    double tsp_ms = 0.0;
    double dual_cut_ms = 0.0;
    double global_ms = 0.0;
    double upper_bound_ms = 0.0;
    double dp_ms = 0.0;
    double total_ms = 0.0;
};

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    ReleaseStats stats;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::release_v3

#endif
