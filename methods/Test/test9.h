#ifndef GST_METHODS_TEST_TEST9_H
#define GST_METHODS_TEST_TEST9_H

#include <memory>
#include <vector>

#include "../../answer_tree.h"
#include "../../graph_io.h"
#include "../../query_io.h"
#include "../DPBF/dpbf_solver.h"

namespace gst::methods::test9
{
    struct Test9Stats
    {
        long long total_valid = 0;
        long long total_up_subset = 0;
        long long total_inqueue = 0;
        long long merge_scan_checks = 0;
        long long merge_dense_checks = 0;
        long long active_seed_vertices = 0;
        long long full_seed_vertices_would = 0;
        long long lb_calls = 0;
        long long pq_pushes = 0;
        long long pq_pops = 0;
        long long relax_attempts = 0;
        long long relax_success = 0;
        long long masks_processed = 0;
        int max_active_vertices = 0;
        double root_star_upper = -1.0;
        double greedy_upper = -1.0;
        long long greedy_pops = 0;
        double preprocess_ms = 0.0;
        double dp_ms = 0.0;
        std::vector<long long> valid_by_size;
        std::vector<long long> total_by_size;
        std::vector<long long> active_by_size;
        std::vector<long long> inqueue_by_size;
        std::vector<long long> merge_by_size;

        long long dense_one_table_cells = 0;
        long long dense_dp_h_cells = 0;
        long long implicit_zero_cells = 0;
        long long dp_first_seen_total = 0;
        long long h_first_seen_total = 0;
        long long dp_sparse_cells_with_zero = 0;
        long long sparse_dp_h_cells_with_zero = 0;
        long long dp_large_side_cells = 0;
        long long h_large_side_cells = 0;
        long long dead_nxt_skip_would = 0;
        long long dead_h_skip_would = 0;
        long long t_size_filtered_would = 0;
        long long full_best_only_hits = 0;
        long long full_best_only_improvements = 0;
        long long dp_store_skipped_high = 0;
        long long h_store_skipped_low = 0;
        long long h_store_skipped_high = 0;
        long long h_store_skipped_full = 0;
        long long dp_live_attempts = 0;
        long long dp_update_success = 0;
        long long dp_update_first = 0;
        long long dp_update_improve = 0;
        long long dp_attempt_old_finite = 0;
        long long dp_attempt_cand_ge_best = 0;
        long long dp_success_cand_ge_best = 0;
        long long dp_attempt_cand_far_ge_best = 0;
        long long dp_success_cand_far_ge_best = 0;
        long long dp_attempt_cand_lb_ge_best = 0;
        long long dp_success_cand_lb_ge_best = 0;
        long long h_live_attempts = 0;
        long long h_update_success = 0;
        long long h_update_first = 0;
        long long h_update_improve = 0;
        long long h_attempt_old_set = 0;
        long long h_attempt_value_ge_best = 0;
        long long h_success_value_ge_best = 0;
        long long h_attempt_value_far_ge_best = 0;
        long long h_success_value_far_ge_best = 0;
        long long h_attempt_value_lb_ge_best = 0;
        long long h_success_value_lb_ge_best = 0;
        long long full_best_dominated = 0;
        long long full_best_dominated_improve = 0;
        long long dp_try_cur_le_t = 0;
        long long dp_succ_cur_le_t = 0;
        long long dp_try_cur_gt_t = 0;
        long long dp_succ_cur_gt_t = 0;
        long long h_try_cur_le_t = 0;
        long long h_succ_cur_le_t = 0;
        long long h_try_cur_gt_t = 0;
        long long h_succ_cur_gt_t = 0;
        long long dp_mask_count = 0;
        long long h_mask_count = 0;
        long long root_mask_entries = 0;
        int max_root_masks = 0;
        long long peak_window_dp_cells = 0;
        long long peak_window_h_cells = 0;
        long long peak_window_dp_h_cells = 0;
        int peak_window_layer = 0;
        std::vector<long long> dp_finite_by_size;
        std::vector<long long> h_finite_by_size;
        std::vector<long long> dp_mask_count_by_size;
        std::vector<long long> h_mask_count_by_size;
        std::vector<long long> window_keep_dp_by_layer;
        std::vector<long long> window_keep_h_by_layer;
        std::vector<long long> window_drop_low_dp_by_layer;
        std::vector<long long> window_drop_high_dp_by_layer;
        long long peak_lifecycle_dp_cells = 0;
        long long peak_lifecycle_h_cells = 0;
        long long peak_lifecycle_dp_h_cells = 0;
        int peak_lifecycle_layer = 0;
        std::vector<long long> lifecycle_keep_dp_by_layer;
        std::vector<long long> lifecycle_keep_h_by_layer;
        std::vector<long long> lifecycle_drop_dp_by_layer;
        std::vector<long long> lifecycle_drop_h_by_layer;
        std::vector<long long> dp_try_by_src_size;
        std::vector<long long> dp_succ_by_src_size;
        std::vector<long long> h_try_by_src_size;
        std::vector<long long> h_succ_by_src_size;
        std::vector<long long> dp_try_by_t_size;
        std::vector<long long> dp_succ_by_t_size;
        std::vector<long long> h_try_by_t_size;
        std::vector<long long> h_succ_by_t_size;
        std::vector<long long> dp_try_by_nxt_size;
        std::vector<long long> dp_succ_by_nxt_size;
        std::vector<long long> h_try_by_nxt_size;
        std::vector<long long> h_succ_by_nxt_size;
        std::vector<long long> dp_try_by_src_t;
        std::vector<long long> dp_succ_by_src_t;
        std::vector<long long> h_try_by_src_t;
        std::vector<long long> h_succ_by_src_t;
        std::vector<long long> full_try_by_src_size;
        std::vector<long long> full_improve_by_src_size;
    };

    struct SolveResult
    {
        double best_weight = -1.0;
        bool feasible = false;
        std::unique_ptr<AnswerTreeBase> answer;
        Test9Stats stats;
    };

    SolveResult SolveOneQuery(const Graph& graph, const Query& query, dpbf::OutputMode output_mode);

}  // namespace gst::methods::test9

#endif  // GST_METHODS_TEST_TEST9_H
