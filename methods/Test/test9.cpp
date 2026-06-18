#include "test9.h"

#include <algorithm>
#include <chrono>
#include <queue>
#include <stdexcept>
#include <utility>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

namespace gst::methods::test9
{
    SolveResult SolveOneQuery(const Graph& graph, const Query& query, dpbf::OutputMode /*output_mode*/)
    {
        const int g = static_cast<int>(query.groups.size());
        const int n = graph.n;
        SolveResult result;
        if (g == 0)
        {
            result.best_weight = 0.0;
            result.feasible = true;
            return result;
        }
        if (g >= 23)
        {
            throw std::runtime_error("Test9 currently supports group count <= 22.");
        }
        if (!IsQueryFeasible(graph, query))
        {
            result.best_weight = -1.0;
            result.feasible = false;
            result.stats.valid_by_size.assign(g + 1, 0);
            result.stats.total_by_size.assign(g + 1, 0);
            result.stats.active_by_size.assign(g + 1, 0);
            result.stats.inqueue_by_size.assign(g + 1, 0);
            result.stats.merge_by_size.assign(g + 1, 0);
            result.stats.dp_finite_by_size.assign(g + 1, 0);
            result.stats.h_finite_by_size.assign(g + 1, 0);
            result.stats.dp_mask_count_by_size.assign(g + 1, 0);
            result.stats.h_mask_count_by_size.assign(g + 1, 0);
            return result;
        }

        result.stats.total_by_size.assign(g + 1, 0);
        result.stats.valid_by_size.assign(g + 1, 0);
        result.stats.active_by_size.assign(g + 1, 0);
        result.stats.inqueue_by_size.assign(g + 1, 0);
        result.stats.merge_by_size.assign(g + 1, 0);
        result.stats.dp_finite_by_size.assign(g + 1, 0);
        result.stats.h_finite_by_size.assign(g + 1, 0);
        result.stats.dp_mask_count_by_size.assign(g + 1, 0);
        result.stats.h_mask_count_by_size.assign(g + 1, 0);
        result.stats.window_keep_dp_by_layer.assign(g + 1, 0);
        result.stats.window_keep_h_by_layer.assign(g + 1, 0);
        result.stats.window_drop_low_dp_by_layer.assign(g + 1, 0);
        result.stats.window_drop_high_dp_by_layer.assign(g + 1, 0);
        result.stats.lifecycle_keep_dp_by_layer.assign(g + 1, 0);
        result.stats.lifecycle_keep_h_by_layer.assign(g + 1, 0);
        result.stats.lifecycle_drop_dp_by_layer.assign(g + 1, 0);
        result.stats.lifecycle_drop_h_by_layer.assign(g + 1, 0);
        result.stats.dp_try_by_src_size.assign(g + 1, 0);
        result.stats.dp_succ_by_src_size.assign(g + 1, 0);
        result.stats.h_try_by_src_size.assign(g + 1, 0);
        result.stats.h_succ_by_src_size.assign(g + 1, 0);
        result.stats.dp_try_by_t_size.assign(g + 1, 0);
        result.stats.dp_succ_by_t_size.assign(g + 1, 0);
        result.stats.h_try_by_t_size.assign(g + 1, 0);
        result.stats.h_succ_by_t_size.assign(g + 1, 0);
        result.stats.dp_try_by_nxt_size.assign(g + 1, 0);
        result.stats.dp_succ_by_nxt_size.assign(g + 1, 0);
        result.stats.h_try_by_nxt_size.assign(g + 1, 0);
        result.stats.h_succ_by_nxt_size.assign(g + 1, 0);
        result.stats.dp_try_by_src_t.assign((g + 1) * (g + 1), 0);
        result.stats.dp_succ_by_src_t.assign((g + 1) * (g + 1), 0);
        result.stats.h_try_by_src_t.assign((g + 1) * (g + 1), 0);
        result.stats.h_succ_by_src_t.assign((g + 1) * (g + 1), 0);
        result.stats.full_try_by_src_size.assign(g + 1, 0);
        result.stats.full_improve_by_src_size.assign(g + 1, 0);

        auto solve_start = std::chrono::steady_clock::now();

        const int H = g / 2;
        const int U = (1 << g) - 1;
        std::vector<int> popcnt(1 << g);
        for (int s = 0; s <= U; ++s)
        {
            popcnt[s] = popcnt[s >> 1] + (s & 1);
            result.stats.total_by_size[popcnt[s]] += n;
        }

        result.stats.dense_one_table_cells = static_cast<long long>(1 << g) * static_cast<long long>(n + 1);
        result.stats.dense_dp_h_cells = result.stats.dense_one_table_cells * 2;
        result.stats.implicit_zero_cells = n;

        std::vector<int> vertex_group_mask(n + 1, 0);
        for (int gi = 0; gi < g; ++gi)
        {
            for (int v : query.groups[gi])
            {
                if (v < 1 || v > n)
                {
                    throw std::runtime_error("Query vertex id out of range.");
                }
                vertex_group_mask[v] |= (1 << gi);
            }
        }

        std::vector<std::vector<double>> group_dist(g, std::vector<double>(n + 1, fp::kInf));
        for (int gi = 0; gi < g; ++gi)
        {
            auto& dist = group_dist[gi];
            std::priority_queue<std::pair<double, int>,
                                std::vector<std::pair<double, int>>,
                                std::greater<std::pair<double, int>>> pq;
            for (int v : query.groups[gi])
            {
                if (v < 1 || v > n)
                {
                    throw std::runtime_error("Query vertex id out of range.");
                }
                if (dist[v] > 0.0)
                {
                    dist[v] = 0.0;
                    pq.push({0.0, v});
                }
            }
            while (!pq.empty())
            {
                auto [d, u] = pq.top();
                pq.pop();
                if (d > dist[u])
                {
                    continue;
                }
                for (const auto& e : graph.adj[u])
                {
                    if (dist[e.to] > d + e.w)
                    {
                        pq.push({dist[e.to] = d + e.w, e.to});
                    }
                }
            }
        }

        std::vector<std::vector<double>> group_pair(g, std::vector<double>(g, fp::kInf));
        for (int a = 0; a < g; ++a)
        {
            for (int b = 0; b < g; ++b)
            {
                for (int v : query.groups[b])
                {
                    group_pair[a][b] = std::min(group_pair[a][b], group_dist[a][v]);
                }
            }
        }

        std::vector<double> mst_cache(1 << g, -1.0);
        auto MstHalf = [&](int mask)
        {
            if (mask == 0 || (mask & (mask - 1)) == 0)
            {
                return 0.0;
            }
            double& cached = mst_cache[mask];
            if (cached >= 0.0)
            {
                return cached;
            }
            std::vector<double> best_edge(g, fp::kInf);
            std::vector<char> used(g, 0);
            int start = 0;
            while (((mask >> start) & 1) == 0)
            {
                ++start;
            }
            best_edge[start] = 0.0;
            double sum = 0.0;
            for (int iter = 0; iter < popcnt[mask]; ++iter)
            {
                int u = -1;
                for (int a = 0; a < g; ++a)
                {
                    if (((mask >> a) & 1) && !used[a] && (u < 0 || best_edge[a] < best_edge[u]))
                    {
                        u = a;
                    }
                }
                used[u] = 1;
                sum += best_edge[u];
                for (int a = 0; a < g; ++a)
                {
                    if (((mask >> a) & 1) && !used[a])
                    {
                        best_edge[a] = std::min(best_edge[a], group_pair[u][a]);
                    }
                }
            }
            return cached = sum * 0.5;
        };

        auto LowerBound = [&](int v, int rem)
        {
            if (rem == 0)
            {
                return 0.0;
            }
            double farthest = 0.0;
            double first = fp::kInf;
            double second = fp::kInf;
            for (int gi = 0; gi < g; ++gi)
            {
                if (((rem >> gi) & 1) == 0)
                {
                    continue;
                }
                const double d = group_dist[gi][v];
                farthest = std::max(farthest, d);
                if (d < first)
                {
                    second = first;
                    first = d;
                }
                else if (d < second)
                {
                    second = d;
                }
            }
            if ((rem & (rem - 1)) == 0)
            {
                return farthest;
            }
            return std::max(farthest, MstHalf(rem) + (first + second) * 0.5);
        };

        auto FarBound = [&](int v, int rem)
        {
            double farthest = 0.0;
            for (int gi = 0; gi < g; ++gi)
            {
                if (((rem >> gi) & 1) != 0)
                {
                    farthest = std::max(farthest, group_dist[gi][v]);
                }
            }
            return farthest;
        };

        double best = fp::kInf;
        int best_root = 1;
        for (int v = 1; v <= n; ++v)
        {
            double star = 0.0;
            for (int gi = 0; gi < g; ++gi)
            {
                star += group_dist[gi][v];
            }
            if (star < best)
            {
                best = star;
                best_root = v;
            }
        }
        result.stats.root_star_upper = best;

        auto GreedyUpperBound = [&](int start)
        {
            int covered = vertex_group_mask[start];
            double total = 0.0;
            std::vector<int> selected{start};
            std::vector<double> dist(n + 1, fp::kInf);
            std::vector<int> seen(n + 1, 0);
            int stamp = 0;

            while (covered != U && total < best)
            {
                ++stamp;
                std::priority_queue<std::pair<double, int>,
                                    std::vector<std::pair<double, int>>,
                                    std::greater<std::pair<double, int>>> pq;
                for (int s : selected)
                {
                    seen[s] = stamp;
                    dist[s] = 0.0;
                    pq.push({0.0, s});
                }

                bool found = false;
                while (!pq.empty())
                {
                    auto [d, u] = pq.top();
                    pq.pop();
                    result.stats.greedy_pops++;
                    if (seen[u] != stamp || d > dist[u])
                    {
                        continue;
                    }
                    const int newly_covered = vertex_group_mask[u] & (U ^ covered);
                    if (newly_covered != 0)
                    {
                        total += d;
                        covered |= vertex_group_mask[u];
                        selected.push_back(u);
                        found = true;
                        break;
                    }
                    for (const auto& e : graph.adj[u])
                    {
                        const double nd = d + e.w;
                        if (seen[e.to] != stamp || nd < dist[e.to])
                        {
                            seen[e.to] = stamp;
                            dist[e.to] = nd;
                            pq.push({nd, e.to});
                        }
                    }
                }
                if (!found)
                {
                    return fp::kInf;
                }
            }
            return total;
        };

        const double greedy_best = GreedyUpperBound(best_root);
        if (greedy_best < best)
        {
            best = greedy_best;
        }
        result.stats.greedy_upper = greedy_best;
        result.stats.preprocess_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - solve_start).count();
        auto dp_start = std::chrono::steady_clock::now();

        std::vector<std::vector<double>> dp(1 << g, std::vector<double>(n + 1, fp::kInf));
        std::vector<std::vector<double>> h(1 << g, std::vector<double>(n + 1, -1.0));
        std::vector<std::vector<int>> root_masks(n + 1);
        std::vector<std::vector<int>> active_vertices(1 << g);
        std::vector<int> dp_mask_seen(1 << g, 0);
        std::vector<int> h_mask_seen(1 << g, 0);
        for (int i = 1; i <= n; ++i)
        {
            dp[0][i] = 0.0;
        }

        auto TouchDpMask = [&](int mask)
        {
            if (!dp_mask_seen[mask])
            {
                dp_mask_seen[mask] = 1;
                result.stats.dp_mask_count++;
                result.stats.dp_mask_count_by_size[popcnt[mask]]++;
            }
        };

        auto TouchHMask = [&](int mask)
        {
            if (!h_mask_seen[mask])
            {
                h_mask_seen[mask] = 1;
                result.stats.h_mask_count++;
                result.stats.h_mask_count_by_size[popcnt[mask]]++;
            }
        };

        auto SetDp = [&](int mask, int v, double value)
        {
            const bool first_seen = dp[mask][v] >= fp::kInf / 4;
            if (!first_seen && !(value < dp[mask][v]))
            {
                return 0;
            }
            dp[mask][v] = value;
            if (first_seen)
            {
                root_masks[v].push_back(mask);
                active_vertices[mask].push_back(v);
                TouchDpMask(mask);
                result.stats.dp_first_seen_total++;
                result.stats.dp_finite_by_size[popcnt[mask]]++;
                if (popcnt[mask] > H)
                {
                    result.stats.dp_large_side_cells++;
                }
                return 1;
            }
            return 2;
        };

        auto UpdateH = [&](int mask, int v, double value)
        {
            const double old_value = h[mask][v];
            if (old_value >= 0.0 && !(value > old_value))
            {
                return 0;
            }
            if (h[mask][v] < 0.0)
            {
                TouchHMask(mask);
                result.stats.h_first_seen_total++;
                result.stats.h_finite_by_size[popcnt[mask]]++;
                if (popcnt[mask] > H)
                {
                    result.stats.h_large_side_cells++;
                }
                h[mask][v] = value;
                return 1;
            }
            h[mask][v] = value;
            return 2;
        };

        auto RecordWindow = [&](int layer)
        {
            long long keep_dp = 0;
            long long keep_h = 0;
            long long drop_low_dp = 0;
            long long drop_high_dp = 0;
            long long lifecycle_keep_dp = 0;
            long long lifecycle_keep_h = 0;
            long long lifecycle_drop_dp = 0;
            long long lifecycle_drop_h = 0;
            for (int size = 0; size <= g; ++size)
            {
                if (size < layer)
                {
                    drop_low_dp += result.stats.dp_finite_by_size[size];
                }
                else if (size > g - layer)
                {
                    drop_high_dp += result.stats.dp_finite_by_size[size];
                }
                else
                {
                    keep_dp += result.stats.dp_finite_by_size[size];
                    keep_h += result.stats.h_finite_by_size[size];
                }

                const bool dp_needed_for_future_expand = (size >= layer && size <= H);
                const bool dp_needed_as_small_counterpart = (size <= g - 2 * layer);
                const bool dp_needed_as_complement = (size >= g - H && size <= g - layer);
                const bool h_needed_as_future_complement = (size >= g - H && size <= g - layer);
                if (dp_needed_for_future_expand || dp_needed_as_small_counterpart || dp_needed_as_complement)
                {
                    lifecycle_keep_dp += result.stats.dp_finite_by_size[size];
                }
                else
                {
                    lifecycle_drop_dp += result.stats.dp_finite_by_size[size];
                }
                if (h_needed_as_future_complement)
                {
                    lifecycle_keep_h += result.stats.h_finite_by_size[size];
                }
                else
                {
                    lifecycle_drop_h += result.stats.h_finite_by_size[size];
                }
            }
            result.stats.window_keep_dp_by_layer[layer] = keep_dp;
            result.stats.window_keep_h_by_layer[layer] = keep_h;
            result.stats.window_drop_low_dp_by_layer[layer] = drop_low_dp;
            result.stats.window_drop_high_dp_by_layer[layer] = drop_high_dp;
            const long long total_keep = keep_dp + keep_h;
            if (total_keep > result.stats.peak_window_dp_h_cells)
            {
                result.stats.peak_window_dp_h_cells = total_keep;
                result.stats.peak_window_dp_cells = keep_dp;
                result.stats.peak_window_h_cells = keep_h;
                result.stats.peak_window_layer = layer;
            }
            result.stats.lifecycle_keep_dp_by_layer[layer] = lifecycle_keep_dp;
            result.stats.lifecycle_keep_h_by_layer[layer] = lifecycle_keep_h;
            result.stats.lifecycle_drop_dp_by_layer[layer] = lifecycle_drop_dp;
            result.stats.lifecycle_drop_h_by_layer[layer] = lifecycle_drop_h;
            const long long lifecycle_total_keep = lifecycle_keep_dp + lifecycle_keep_h;
            if (lifecycle_total_keep > result.stats.peak_lifecycle_dp_h_cells)
            {
                result.stats.peak_lifecycle_dp_h_cells = lifecycle_total_keep;
                result.stats.peak_lifecycle_dp_cells = lifecycle_keep_dp;
                result.stats.peak_lifecycle_h_cells = lifecycle_keep_h;
                result.stats.peak_lifecycle_layer = layer;
            }
        };

        auto Modify = [&](int mask, int v, double w)
        {
            SetDp(mask, v, w);
            result.stats.total_valid++;
            result.stats.valid_by_size[popcnt[mask]]++;

            auto HasDominatingSuperset = [&](int base_mask, int root, double value)
            {
                for (int candidate_mask : root_masks[root])
                {
                    if (candidate_mask == base_mask)
                    {
                        continue;
                    }
                    if ((candidate_mask | base_mask) == candidate_mask && dp[candidate_mask][root] <= value)
                    {
                        return true;
                    }
                }
                return false;
            };

            auto RelaxSameRoot = [&](int t)
            {
                result.stats.total_up_subset++;
                result.stats.merge_by_size[popcnt[mask]]++;
                const int nxt = mask | t;
                const double cand = dp[mask][v] + dp[t][v];
                const int current_size = popcnt[mask];
                const int nxt_size = popcnt[nxt];
                const int t_size = popcnt[t];
                const int src_t_index = current_size * (g + 1) + t_size;
                const bool direct_full = (nxt == U);
                const bool dp_live = !direct_full && (nxt_size <= g - current_size);
                const bool h_live = !direct_full && (nxt_size >= g - H) && (nxt_size <= g - current_size);

                if (direct_full)
                {
                    result.stats.full_best_only_hits++;
                    result.stats.full_try_by_src_size[current_size]++;
                    const bool dominated = HasDominatingSuperset(mask, v, dp[mask][v]);
                    if (dominated)
                    {
                        result.stats.full_best_dominated++;
                    }
                    result.stats.dead_h_skip_would++;
                    result.stats.h_store_skipped_full++;
                    if (cand < best)
                    {
                        best = cand;
                        result.stats.full_best_only_improvements++;
                        result.stats.full_improve_by_src_size[current_size]++;
                        if (dominated)
                        {
                            result.stats.full_best_dominated_improve++;
                        }
                    }
                    return;
                }

                if (!dp_live)
                {
                    result.stats.dead_nxt_skip_would++;
                    result.stats.dp_store_skipped_high++;
                    if (t != 0 && t != (U ^ mask) && t_size > g - 2 * current_size)
                    {
                        result.stats.t_size_filtered_would++;
                    }
                }
                else
                {
                    result.stats.dp_live_attempts++;
                    result.stats.dp_try_by_src_size[current_size]++;
                    result.stats.dp_try_by_t_size[t_size]++;
                    result.stats.dp_try_by_nxt_size[nxt_size]++;
                    result.stats.dp_try_by_src_t[src_t_index]++;
                    if (dp[mask][v] <= dp[t][v])
                    {
                        result.stats.dp_try_cur_le_t++;
                    }
                    else
                    {
                        result.stats.dp_try_cur_gt_t++;
                    }
                    if (dp[nxt][v] < fp::kInf / 4)
                    {
                        result.stats.dp_attempt_old_finite++;
                    }
                    if (cand >= best)
                    {
                        result.stats.dp_attempt_cand_ge_best++;
                    }
                    const int rem_after = U ^ nxt;
                    const bool cand_far_dead = (cand + FarBound(v, rem_after) >= best);
                    const bool cand_lb_dead = (cand + LowerBound(v, rem_after) >= best);
                    if (cand_far_dead)
                    {
                        result.stats.dp_attempt_cand_far_ge_best++;
                    }
                    if (cand_lb_dead)
                    {
                        result.stats.dp_attempt_cand_lb_ge_best++;
                    }
                    const int update_status = SetDp(nxt, v, cand);
                    if (update_status != 0)
                    {
                        result.stats.dp_update_success++;
                        result.stats.dp_succ_by_src_size[current_size]++;
                        result.stats.dp_succ_by_t_size[t_size]++;
                        result.stats.dp_succ_by_nxt_size[nxt_size]++;
                        result.stats.dp_succ_by_src_t[src_t_index]++;
                        if (dp[mask][v] <= dp[t][v])
                        {
                            result.stats.dp_succ_cur_le_t++;
                        }
                        else
                        {
                            result.stats.dp_succ_cur_gt_t++;
                        }
                        if (update_status == 1)
                        {
                            result.stats.dp_update_first++;
                        }
                        else
                        {
                            result.stats.dp_update_improve++;
                        }
                        if (cand >= best)
                        {
                            result.stats.dp_success_cand_ge_best++;
                        }
                        if (cand_far_dead)
                        {
                            result.stats.dp_success_cand_far_ge_best++;
                        }
                        if (cand_lb_dead)
                        {
                            result.stats.dp_success_cand_lb_ge_best++;
                        }
                    }
                }

                if (!h_live)
                {
                    result.stats.dead_h_skip_would++;
                    if (nxt_size < g - H)
                    {
                        result.stats.h_store_skipped_low++;
                    }
                    else if (nxt_size > g - current_size)
                    {
                        result.stats.h_store_skipped_high++;
                    }
                }
                else
                {
                    result.stats.h_live_attempts++;
                    result.stats.h_try_by_src_size[current_size]++;
                    result.stats.h_try_by_t_size[t_size]++;
                    result.stats.h_try_by_nxt_size[nxt_size]++;
                    result.stats.h_try_by_src_t[src_t_index]++;
                    if (dp[mask][v] <= dp[t][v])
                    {
                        result.stats.h_try_cur_le_t++;
                    }
                    else
                    {
                        result.stats.h_try_cur_gt_t++;
                    }
                    if (h[nxt][v] >= 0.0)
                    {
                        result.stats.h_attempt_old_set++;
                    }
                    if (dp[mask][v] >= best)
                    {
                        result.stats.h_attempt_value_ge_best++;
                    }
                    const int h_rem_after = U ^ nxt;
                    const bool h_far_dead = (dp[mask][v] + FarBound(v, h_rem_after) >= best);
                    const bool h_lb_dead = (dp[mask][v] + LowerBound(v, h_rem_after) >= best);
                    if (h_far_dead)
                    {
                        result.stats.h_attempt_value_far_ge_best++;
                    }
                    if (h_lb_dead)
                    {
                        result.stats.h_attempt_value_lb_ge_best++;
                    }
                    const int update_status = UpdateH(nxt, v, dp[mask][v]);
                    if (update_status != 0)
                    {
                        result.stats.h_update_success++;
                        result.stats.h_succ_by_src_size[current_size]++;
                        result.stats.h_succ_by_t_size[t_size]++;
                        result.stats.h_succ_by_nxt_size[nxt_size]++;
                        result.stats.h_succ_by_src_t[src_t_index]++;
                        if (dp[mask][v] <= dp[t][v])
                        {
                            result.stats.h_succ_cur_le_t++;
                        }
                        else
                        {
                            result.stats.h_succ_cur_gt_t++;
                        }
                        if (update_status == 1)
                        {
                            result.stats.h_update_first++;
                        }
                        else
                        {
                            result.stats.h_update_improve++;
                        }
                        if (dp[mask][v] >= best)
                        {
                            result.stats.h_success_value_ge_best++;
                        }
                        if (h_far_dead)
                        {
                            result.stats.h_success_value_far_ge_best++;
                        }
                        if (h_lb_dead)
                        {
                            result.stats.h_success_value_lb_ge_best++;
                        }
                    }
                }
            };

            RelaxSameRoot(0);
            const int rem = U ^ mask;
            const long long dense_count = (1LL << popcnt[rem]) - 1;
            const int old_size = static_cast<int>(root_masks[v].size());
            if (old_size <= dense_count)
            {
                result.stats.merge_scan_checks += old_size;
                for (int idx = 0; idx < old_size; ++idx)
                {
                    const int t = root_masks[v][idx];
                    if (t != mask && (t & mask) == 0)
                    {
                        RelaxSameRoot(t);
                    }
                }
            }
            else
            {
                result.stats.merge_dense_checks += dense_count;
                for (int t = rem; t > 0; t = (t - 1) & rem)
                {
                    if (dp[t][v] < fp::kInf / 4)
                    {
                        RelaxSameRoot(t);
                    }
                }
            }
        };

        for (int gi = 0; gi < g; ++gi)
        {
            const int m = (1 << gi);
            for (int v : query.groups[gi])
            {
                if (v < 1 || v > n)
                {
                    throw std::runtime_error("Query vertex id out of range.");
                }
                Modify(m, v, 0.0);
            }
        }

        std::vector<int> order;
        for (int mask = 1; mask <= U; ++mask)
        {
            order.push_back(mask);
        }
        std::sort(order.begin(), order.end(), [&](int a, int b)
        {
            return popcnt[a] < popcnt[b];
        });

        std::vector<int> status(n + 1, 0);
        int stamp = 0;
        int current_layer = 1;
        for (int mask : order)
        {
            if (popcnt[mask] > H)
            {
                break;
            }
            while (current_layer < popcnt[mask])
            {
                RecordWindow(current_layer);
                ++current_layer;
            }

            const int rem = U ^ mask;
            std::vector<double> lb(n + 1, -1.0);
            auto GetLb = [&](int v)
            {
                if (lb[v] < 0.0)
                {
                    result.stats.lb_calls++;
                    lb[v] = LowerBound(v, rem);
                }
                return lb[v];
            };

            ++stamp;
            result.stats.masks_processed++;
            std::priority_queue<std::pair<double, int>,
                                std::vector<std::pair<double, int>>,
                                std::greater<std::pair<double, int>>> pq;

            double mx_used = 0.0;
            int used_cnt = 0;
            result.stats.full_seed_vertices_would += n;
            result.stats.active_seed_vertices += static_cast<long long>(active_vertices[mask].size());
            result.stats.active_by_size[popcnt[mask]] += static_cast<long long>(active_vertices[mask].size());
            result.stats.max_active_vertices =
                std::max(result.stats.max_active_vertices, static_cast<int>(active_vertices[mask].size()));

            for (int i : active_vertices[mask])
            {
                const double lbi = GetLb(i);
                if (dp[mask][i] + h[rem][i] <= best && dp[mask][i] + lbi <= best)
                {
                    pq.push({dp[mask][i] + lbi, i});
                    result.stats.pq_pushes++;
                    used_cnt++;
                    status[i] = stamp;
                    mx_used = std::max(mx_used, dp[mask][i]);
                }
            }
            for (int i : active_vertices[mask])
            {
                if ((dp[mask][i] + h[rem][i] > best || dp[mask][i] + GetLb(i) > best) &&
                    dp[mask][i] < mx_used)
                {
                    pq.push({dp[mask][i] + GetLb(i), i});
                    result.stats.pq_pushes++;
                }
            }

            while (!pq.empty() && used_cnt > 0)
            {
                auto [key, u] = pq.top();
                pq.pop();
                result.stats.pq_pops++;
                const double d = dp[mask][u];
                if (key > d + GetLb(u) || d + GetLb(u) > best)
                {
                    continue;
                }
                result.stats.total_inqueue++;
                result.stats.inqueue_by_size[popcnt[mask]]++;
                if (status[u] == stamp)
                {
                    --used_cnt;
                    Modify(mask, u, d);
                }
                for (const auto& e : graph.adj[u])
                {
                    result.stats.relax_attempts++;
                    if (dp[mask][e.to] > d + e.w)
                    {
                        const double nd = d + e.w;
                        if (nd + GetLb(e.to) > best)
                        {
                            continue;
                        }
                        if (status[u] != status[e.to])
                        {
                            status[e.to] = status[u];
                            if (status[e.to] == stamp)
                            {
                                ++used_cnt;
                            }
                        }
                        SetDp(mask, e.to, nd);
                        result.stats.relax_success++;
                        pq.push({nd + GetLb(e.to), e.to});
                        result.stats.pq_pushes++;
                    }
                }
            }
        }
        while (current_layer <= H)
        {
            RecordWindow(current_layer);
            ++current_layer;
        }

        for (int v = 1; v <= n; ++v)
        {
            const int sz = static_cast<int>(root_masks[v].size());
            result.stats.root_mask_entries += sz;
            result.stats.max_root_masks = std::max(result.stats.max_root_masks, sz);
        }

        result.stats.dp_sparse_cells_with_zero = result.stats.dp_first_seen_total + result.stats.implicit_zero_cells;
        result.stats.sparse_dp_h_cells_with_zero =
            result.stats.dp_sparse_cells_with_zero + result.stats.h_first_seen_total;

        result.stats.dp_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - dp_start).count();
        result.stats.valid_by_size.resize(H + 1);
        result.stats.active_by_size.resize(H + 1);
        result.stats.inqueue_by_size.resize(H + 1);
        result.stats.merge_by_size.resize(H + 1);
        result.best_weight = best;
        result.feasible = true;
        return result;
    }
}  // namespace gst::methods::test9
