#ifndef GST_METHODS_TEST_TEST21_ANCHOR_HALF_H
#define GST_METHODS_TEST_TEST21_ANCHOR_HALF_H

#include <array>

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::test21_anchor_half
{
struct Test21Stats
{
    int n = 0;
    int m = 0;
    int g = 0;
    int anchor_group = 0;
    int half = 0;
    long long original_half_masks = 0;
    long long ordinary_masks = 0;
    long long anchored_masks = 0;
    long long ordinary_values = 0;
    long long ordinary_branch_values = 0;
    long long anchored_values = 0;
    long long ordinary_queue_pops = 0;
    std::array<long long, 17> ordinary_values_by_size{};
    std::array<long long, 17> ordinary_branch_values_by_size{};
    std::array<long long, 17> ordinary_seeds_by_size{};
    std::array<long long, 17> ordinary_pops_by_size{};
    std::array<double, 17> ordinary_ms_by_size{};
    long long anchored_queue_pops = 0;
    long long anchored_touched_values = 0;
    long long anchored_settled_values = 0;
    long long anchored_peak_queue = 0;
    long long anchored_merge_probes = 0;
    long long completion_rows = 0;
    long long completion_vertices = 0;
    long long completion_checks = 0;
    long long completion_scan_vertices = 0;
    long long early_upper_partitions = 0;
    long long early_upper_probes = 0;
    long long early_witness_values = 0;
    long long early_witness_pops = 0;
    long long quarter_upper_halves = 0;
    long long quarter_upper_pair_probes = 0;
    long long quarter_upper_join_probes = 0;
    long long quarter_witness_values = 0;
    long long quarter_witness_pops = 0;
    double early_witness_before = 0.0;
    double early_witness_after = 0.0;
    double group_distance_ms = 0.0;
    double ordinary_ms = 0.0;
    double dual_ms = 0.0;
    double anchored_ms = 0.0;
    double completion_ms = 0.0;
    double early_upper_before = 0.0;
    double early_upper_after = 0.0;
    double early_upper_ms = 0.0;
    double quarter_upper_before = 0.0;
    double quarter_upper_after = 0.0;
    double quarter_upper_ms = 0.0;
    double quarter_witness_before = 0.0;
    double quarter_witness_after = 0.0;
    double total_ms = 0.0;
};

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    Test21Stats stats;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::test21_anchor_half

#endif
