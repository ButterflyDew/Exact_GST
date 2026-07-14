#ifndef GST_TOOLS_DISTANCE_EPOCH_SOLVER_PROBE_DISTANCE_SOLVER_H
#define GST_TOOLS_DISTANCE_EPOCH_SOLVER_PROBE_DISTANCE_SOLVER_H

#include "../../graph_io.h"
#include "../../query_io.h"

#include <vector>

namespace gst::tools::distance_epoch
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
    long long distance_states = 0;
    long long distance_bytes = 0;
    long long baseline_row_bytes = 0;
    long long compact_recomputed_states = 0;
    long long pair_partition_roots = 0;
    long long pair_partition_updates = 0;
    long long dense_join_work = 0;
    long long dual_cut_build_work = 0;
    long long row_work = 0;
    bool budget_exhausted = false;
    int budget_stop_size = 0;
    int budget_stop_masks_in_size = 0;
    int dual_activation_size = 0;
    int dual_activation_masks_in_size = 0;
    long long dual_activation_work = 0;
    long long global_created_labels = 0;
    long long global_settled_labels = 0;
    long long global_peak_open_labels = 0;
    int global_anchor_group = 0;
    double global_anchor_distance = 0.0;
    long long released_row_bytes = 0;
    long long goal_root_settled = 0;
    long long goal_root_edge_relaxations = 0;
    long long greedy_upper_build_work = 0;
    long long greedy_upper_activation_work = 0;
    int greedy_upper_activation_size = 0;
    bool dual_cut_enabled = false;
    bool global_used = false;
    bool goal_root_used = false;
    bool greedy_upper_delayed = false;
    bool greedy_upper_activated = false;
    std::vector<long long> kept_by_size;
    std::vector<long long> active_roots_by_size;
    std::vector<long long> depth_sum_by_size;
    std::vector<int> max_depth_by_size;
    double pair_partition_upper = -1.0;
    double pair_partition_ms = 0.0;
    double compact_recompute_ms = 0.0;
    double group_distance_ms = 0.0;
    double tsp_ms = 0.0;
    double dual_cut_ms = 0.0;
    double dual_cut_objective = 0.0;
    double dual_cut_primal_upper = -1.0;
    double global_ms = 0.0;
    double global_start_best = -1.0;
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

enum class ProbeMode
{
    Default,
    StopAtDual,
    DynamicDual,
    StopAtGlobal,
    Hybrid,
    DelayedUpperHybrid,
    EarlyDelayedHybrid
};

SolveResult SolveOneQuery(const Graph& graph,
                          const Query& query,
                          bool verbose = false,
                          ProbeMode mode = ProbeMode::Default);

}  // namespace gst::tools::distance_epoch

#endif
