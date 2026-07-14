#ifndef GST_METHODS_TEST_TEST80_ANCHOR_PROGRESSIVE_H
#define GST_METHODS_TEST_TEST80_ANCHOR_PROGRESSIVE_H

#include <array>

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::test80_anchor_progressive
{
struct Test80Stats
{
    int n = 0;
    int m = 0;
    int g = 0;
    int anchor_group = 0;
    int half = 0;
    int root_star_root = 0;
    int min_group_size = 0;
    int max_group_size = 0;
    int anchor_group_size = 0;
    long long total_group_vertices = 0;
    long long group_distance_bytes = 0;
    double root_star_upper = 0.0;
    double dual_primal_upper = 0.0;
    double root_tour_lower = 0.0;
    double root_dual_lower = 0.0;
    double root_group_distance_min = 0.0;
    double root_group_distance_average = 0.0;
    double root_group_distance_second_max = 0.0;
    double root_group_distance_max = 0.0;
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
    std::array<long long, 17> ordinary_masks_by_size{};
    std::array<long long, 17> ordinary_dense_rows_by_size{};
    std::array<long long, 17> ordinary_sparse_rows_by_size{};
    std::array<long long, 17> ordinary_row_bytes_by_size{};
    std::array<long long, 17> ordinary_seed_candidates_by_size{};
    std::array<long long, 17> ordinary_seed_reject_old_by_size{};
    std::array<long long, 17> ordinary_seed_reject_bound_by_size{};
    std::array<long long, 17> ordinary_seed_accept_by_size{};
    std::array<long long, 17> ordinary_relax_attempts_by_size{};
    std::array<long long, 17> ordinary_relax_reject_old_by_size{};
    std::array<long long, 17> ordinary_relax_reject_bound_by_size{};
    std::array<long long, 17> ordinary_relax_accept_by_size{};
    std::array<long long, 17> ordinary_pop_stale_by_size{};
    std::array<long long, 17> ordinary_pop_bound_by_size{};
    std::array<long long, 17> ordinary_h_evals_by_size{};
    std::array<long long, 17> ordinary_h_farthest_by_size{};
    std::array<long long, 17> ordinary_h_tour_by_size{};
    std::array<long long, 17> ordinary_h_dual_by_size{};
    std::array<double, 17> ordinary_ms_by_size{};
    std::array<double, 17> best_after_ordinary_size{};
    long long ordinary_join_direct_calls = 0;
    long long ordinary_join_direct_work = 0;
    long long ordinary_join_binary_calls = 0;
    long long ordinary_join_binary_work = 0;
    long long ordinary_join_linear_calls = 0;
    long long ordinary_join_linear_work = 0;
    long long anchored_queue_pops = 0;
    long long anchored_touched_values = 0;
    long long anchored_settled_values = 0;
    long long anchored_peak_queue = 0;
    long long anchored_merge_probes = 0;
    std::array<long long, 17> anchored_masks_by_size{};
    std::array<long long, 17> anchored_values_by_size{};
    std::array<long long, 17> anchored_touched_by_size{};
    std::array<long long, 17> anchored_settled_by_size{};
    std::array<long long, 17> anchored_pops_by_size{};
    std::array<long long, 17> anchored_merge_probes_by_size{};
    std::array<long long, 17> anchored_dense_rows_by_size{};
    std::array<long long, 17> anchored_sparse_rows_by_size{};
    std::array<long long, 17> anchored_row_bytes_by_size{};
    std::array<long long, 17> anchored_seed_candidates_by_size{};
    std::array<long long, 17> anchored_seed_reject_old_by_size{};
    std::array<long long, 17> anchored_seed_reject_bound_by_size{};
    std::array<long long, 17> anchored_seed_accept_by_size{};
    std::array<long long, 17> anchored_relax_attempts_by_size{};
    std::array<long long, 17> anchored_relax_reject_old_by_size{};
    std::array<long long, 17> anchored_relax_reject_bound_by_size{};
    std::array<long long, 17> anchored_relax_accept_by_size{};
    std::array<long long, 17> anchored_pop_stale_by_size{};
    std::array<long long, 17> anchored_pop_bound_by_size{};
    std::array<long long, 17> anchored_h_evals_by_size{};
    std::array<long long, 17> anchored_h_farthest_by_size{};
    std::array<long long, 17> anchored_h_tour_by_size{};
    std::array<long long, 17> anchored_h_dual_by_size{};
    std::array<double, 17> anchored_ms_by_size{};
    std::array<double, 17> best_after_anchored_size{};
    long long anchored_join_direct_calls = 0;
    long long anchored_join_direct_work = 0;
    long long anchored_join_binary_calls = 0;
    long long anchored_join_binary_work = 0;
    long long anchored_join_linear_calls = 0;
    long long anchored_join_linear_work = 0;
    long long completion_rows = 0;
    long long completion_vertices = 0;
    long long completion_checks = 0;
    long long completion_scan_vertices = 0;
    std::array<long long, 17> completion_rows_by_size{};
    std::array<long long, 17> completion_scan_vertices_by_size{};
    std::array<long long, 17> completion_checks_by_size{};
    std::array<double, 17> completion_ms_by_size{};
    long long early_upper_partitions = 0;
    long long early_upper_probes = 0;
    long long early_witness_values = 0;
    long long early_witness_pops = 0;
    long long quarter_upper_halves = 0;
    long long quarter_upper_pair_probes = 0;
    long long quarter_upper_join_probes = 0;
    long long quarter_witness_values = 0;
    long long quarter_witness_pops = 0;
    int junction_path_vertices = 0;
    int junction_candidate_roots = 0;
    int junction_tree_vertices = 0;
    long long junction_triple_scans = 0;
    long long junction_convolutions = 0;
    long long junction_work = 0;
    long long pair_work = 0;
    long long packing_budget = 0;
    int packing_trigger_pair_rows = 0;
    long long packing_trigger_pair_work = 0;
    long long packing_trigger_pair_values = 0;
    int packing_rounds = 0;
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
    double junction_before = 0.0;
    double junction_after = 0.0;
    double junction_upper = 0.0;
    double junction_ms = 0.0;
    double packing_min_scale = 0.0;
    double packing_average_scale = 0.0;
    double packing_max_scale = 0.0;
    double packing_ms = 0.0;
    double total_ms = 0.0;
};

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    Test80Stats stats;
};

SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::test80_anchor_progressive

#endif
