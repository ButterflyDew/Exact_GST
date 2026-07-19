#ifndef GST_METHODS_PRUNED_DP_SOLVER_H
#define GST_METHODS_PRUNED_DP_SOLVER_H

#include <string>
#include <vector>

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::pruned_dp
{

enum class StateStorage
{
    Hash,
    Dense,
};

struct PrunedDpOptions
{
    StateStorage state_storage = StateStorage::Hash;
    bool use_mst_upper_bound = true;
    bool enforce_lb2_pathmax = true;
};

struct PrunedDpStats
{
    int n = 0;
    int m = 0;
    int g = 0;
    int total_group_vertices = 0;
    int max_group_size = 0;
    double build_ms = 0.0;
    double dist_ms = 0.0;
    double calw_ms = 0.0;
    double search_ms = 0.0;
    double total_ms = 0.0;
    long long initial_pushes = 0;
    long long pq_pushes = 0;
    long long pq_pops = 0;
    long long stale_pops = 0;
    long long finalized_labels = 0;
    long long cost_ge_best_skips = 0;
    long long update_calls = 0;
    long long update_finalized_skip = 0;
    long long update_bound_pruned = 0;
    long long update_pushes = 0;
    long long best_full_updates = 0;
    long long best_expect_updates = 0;
    long long edge_relax_attempts = 0;
    long long merge_submask_attempts = 0;
    long long merge_state_hits = 0;
    long long merge_cost_gate_pass = 0;
    long long discovered_states = 0;
    long long reopened_states = 0;
    long long mst_calls = 0;
    long long mst_improvements = 0;
    long long mst_input_edges = 0;
    std::string state_storage;
    bool use_mst_upper_bound = true;
    bool enforce_lb2_pathmax = true;
    std::vector<long long> finalized_by_size;
    std::vector<long long> update_push_by_size;
};

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    PrunedDpStats stats;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query);
SolveResult SolveOneQuery(const Graph& graph,
                          const Query& query,
                          const PrunedDpOptions& options);

const char* StateStorageName(StateStorage storage);

}  // namespace gst::methods::pruned_dp

#endif  // GST_METHODS_PRUNED_DP_SOLVER_H
