#ifndef GST_TOOLS_TSP_WITNESS_SOLVER_PROBE_WITNESS_SOLVER_H
#define GST_TOOLS_TSP_WITNESS_SOLVER_PROBE_WITNESS_SOLVER_H

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::tools::tsp_witness
{
struct ReleaseStats
{
    int n = 0;
    int m = 0;
    int g = 0;
    long long saved_states = 0;
    long long peak_live_states = 0;
    long long queue_pushes = 0;
    long long queue_pops = 0;
    long long complement_cache_builds = 0;
    long long witness_states = 0;
    long long witness_bytes = 0;
    long long baseline_row_bytes = 0;
    double group_distance_ms = 0.0;
    double tsp_ms = 0.0;
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

}  // namespace gst::tools::tsp_witness

#endif
