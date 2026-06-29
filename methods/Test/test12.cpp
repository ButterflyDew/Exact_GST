#include "test12.h"

#include <algorithm>
#include <chrono>
#include <queue>
#include <stdexcept>
#include <utility>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

namespace gst::methods::test12
{
    SolveResult SolveOneQuery(const Graph& graph, const Query& query)
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
            throw std::runtime_error("Test12 currently supports group count <= 22.");
        }
        if (!IsQueryFeasible(graph, query))
        {
            result.best_weight = -1.0;
            result.feasible = false;
            result.stats.valid_by_size.assign(g + 1, 0);
            result.stats.total_by_size.assign(g + 1, 0);
            return result;
        }

        result.stats.total_by_size.assign(g + 1, 0);
        result.stats.valid_by_size.assign(g + 1, 0);
        result.stats.active_by_size.assign(g + 1, 0);
        result.stats.inqueue_by_size.assign(g + 1, 0);
        result.stats.merge_by_size.assign(g + 1, 0);
        auto solve_start = std::chrono::steady_clock::now();

        const int H = g / 2;
        const int U = (1 << g) - 1;
        std::vector<int> popcnt(1 << g);
        for (int s = 0; s <= U; ++s)
        {
            popcnt[s] = popcnt[s >> 1] + (s & 1);
            result.stats.total_by_size[popcnt[s]] += n;
        }

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
                    const double nd = d + e.w;
                    if (nd < dist[e.to])
                    {
                        dist[e.to] = nd;
                        pq.push({nd, e.to});
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
                    ++result.stats.greedy_pops;
                    if (seen[u] != stamp || d > dist[u])
                    {
                        continue;
                    }
                    if ((vertex_group_mask[u] & (U ^ covered)) != 0)
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
        std::vector<std::vector<int>> active_vertices(1 << g);
        std::vector<std::vector<std::vector<int>>> root_masks_by_size(n + 1, std::vector<std::vector<int>>(g + 1));

        for (int i = 1; i <= n; ++i)
        {
            dp[0][i] = 0.0;
        }

        auto SetDp = [&](int mask, int v, double value)
        {
            const bool first_seen = dp[mask][v] >= fp::kInf / 4;
            if (!first_seen && !(value < dp[mask][v]))
            {
                return false;
            }
            dp[mask][v] = value;
            if (first_seen)
            {
                root_masks_by_size[v][popcnt[mask]].push_back(mask);
                active_vertices[mask].push_back(v);
            }
            return true;
        };

        auto TryLiveDp = [&](int mask, int v, double w, int t)
        {
            if ((t & mask) != 0)
            {
                return false;
            }
            const int nxt = mask | t;
            if (nxt == U)
            {
                return false;
            }
            const double cand = w + dp[t][v];
            if (cand >= best)
            {
                ++result.stats.live_dp_pruned_ge_best;
                return false;
            }
            if (cand + FarBound(v, U ^ nxt) >= best)
            {
                ++result.stats.live_dp_pruned_far;
                return false;
            }

            ++result.stats.merge_live_dp_checks;
            ++result.stats.total_up_subset;
            ++result.stats.merge_by_size[popcnt[mask]];
            return SetDp(nxt, v, cand);
        };

        auto TryFutureH = [&](int mask, int v, double w, int t)
        {
            if ((t & mask) != 0)
            {
                return false;
            }
            const int nxt = mask | t;
            if (nxt == U)
            {
                return false;
            }
            ++result.stats.merge_future_h_checks;
            ++result.stats.total_up_subset;
            ++result.stats.merge_by_size[popcnt[mask]];
            if (w <= h[nxt][v])
            {
                return false;
            }
            h[nxt][v] = w;
            return true;
        };

        auto Modify = [&](int mask, int v, double w, int modify_kind)
        {
            SetDp(mask, v, w);
            ++result.stats.modify_calls;
            if (modify_kind == 0)
            {
                ++result.stats.modify_init;
            }
            ++result.stats.total_valid;
            ++result.stats.valid_by_size[popcnt[mask]];

            const int k = popcnt[mask];
            const int rem = U ^ mask;
            bool best_effect = false;
            bool live_effect = false;
            bool h_effect = false;
            bool best_candidate = false;
            bool live_candidate = false;
            bool h_candidate = false;

            if (dp[rem][v] < fp::kInf / 4)
            {
                best_candidate = true;
                ++result.stats.merge_best_checks;
                const double cand_best = w + dp[rem][v];
                if (cand_best < best)
                {
                    best = cand_best;
                    best_effect = true;
                }
            }

            const int live_limit = g - 2 * k;
            bool live_bucket_possible = false;
            bool exact_precheck = (modify_kind != 1);
            if (exact_precheck)
            {
                ++result.stats.precheck_calls;
            }
            for (int sz = 1; sz <= live_limit; ++sz)
            {
                const auto& bucket = root_masks_by_size[v][sz];
                if (!exact_precheck)
                {
                    if (!bucket.empty())
                    {
                        live_bucket_possible = true;
                        break;
                    }
                    continue;
                }
                for (int t : bucket)
                {
                    ++result.stats.precheck_scans;
                    if ((t & mask) == 0)
                    {
                        live_bucket_possible = true;
                        break;
                    }
                }
                if (live_bucket_possible)
                {
                    break;
                }
            }
            const int h_low = std::max(0, g - H - k);
            const int h_high = g - k;
            bool h_bucket_possible = false;
            for (int sz = h_low; sz <= h_high; ++sz)
            {
                if (sz == 0 || (sz >= 0 && sz <= g && !root_masks_by_size[v][sz].empty()))
                {
                    if (sz == 0 || !exact_precheck)
                    {
                        h_bucket_possible = true;
                        break;
                    }
                    const auto& bucket = root_masks_by_size[v][sz];
                    for (int t : bucket)
                    {
                        ++result.stats.precheck_scans;
                        if ((t & mask) == 0)
                        {
                            h_bucket_possible = true;
                            break;
                        }
                    }
                    if (h_bucket_possible)
                    {
                        break;
                    }
                }
            }

            if (!best_candidate && !live_bucket_possible && !h_bucket_possible)
            {
                ++result.stats.modify_no_best_candidate;
                ++result.stats.modify_no_live_candidate;
                ++result.stats.modify_no_h_candidate;
                ++result.stats.modify_no_candidate_all;
                ++result.stats.precheck_no_candidate;
                ++result.stats.fast_no_candidate_return;
                ++result.stats.modify_no_effect;
                if (modify_kind == 0)
                {
                    ++result.stats.init_no_effect;
                }
                else if (modify_kind == 1)
                {
                    ++result.stats.original_no_effect;
                }
                else if (modify_kind == 2)
                {
                    ++result.stats.promoted_no_effect;
                }
                return;
            }

            if (live_limit <= 0)
            {
                ++result.stats.live_dp_closed_layers;
            }
            for (int sz = 1; sz <= live_limit; ++sz)
            {
                const auto& bucket = root_masks_by_size[v][sz];
                result.stats.merge_bucket_scans += static_cast<long long>(bucket.size());
                for (int t : bucket)
                {
                    if ((t & mask) == 0)
                    {
                        live_candidate = true;
                    }
                    live_effect = TryLiveDp(mask, v, w, t) || live_effect;
                }
            }

            for (int sz = h_low; sz <= h_high; ++sz)
            {
                if (sz < 0 || sz > g)
                {
                    continue;
                }
                if (sz == 0)
                {
                    h_candidate = true;
                    h_effect = TryFutureH(mask, v, w, 0) || h_effect;
                    continue;
                }
                const auto& bucket = root_masks_by_size[v][sz];
                result.stats.merge_bucket_scans += static_cast<long long>(bucket.size());
                for (int t : bucket)
                {
                    if ((t & mask) == 0)
                    {
                        h_candidate = true;
                    }
                    h_effect = TryFutureH(mask, v, w, t) || h_effect;
                }
            }

            if (!best_candidate)
            {
                ++result.stats.modify_no_best_candidate;
            }
            if (!live_candidate)
            {
                ++result.stats.modify_no_live_candidate;
            }
            if (!h_candidate)
            {
                ++result.stats.modify_no_h_candidate;
            }
            if (!best_candidate && !live_candidate && !h_candidate)
            {
                ++result.stats.modify_no_candidate_all;
            }

            const int effect_count = (best_effect ? 1 : 0) + (live_effect ? 1 : 0) + (h_effect ? 1 : 0);
            if (best_effect)
            {
                ++result.stats.modify_best_effect;
            }
            if (live_effect)
            {
                ++result.stats.modify_live_effect;
            }
            if (h_effect)
            {
                ++result.stats.modify_h_effect;
            }
            if (effect_count == 0)
            {
                ++result.stats.modify_no_effect;
                if (modify_kind == 0)
                {
                    ++result.stats.init_no_effect;
                }
                else if (modify_kind == 1)
                {
                    ++result.stats.original_no_effect;
                }
                else if (modify_kind == 2)
                {
                    ++result.stats.promoted_no_effect;
                }
            }
            else if (effect_count > 1)
            {
                ++result.stats.modify_multi_effect;
            }
            else if (best_effect)
            {
                ++result.stats.modify_only_best_effect;
            }
            else if (live_effect)
            {
                ++result.stats.modify_only_live_effect;
            }
            else
            {
                ++result.stats.modify_only_h_effect;
                if (modify_kind == 0)
                {
                    ++result.stats.init_only_h;
                }
                else if (modify_kind == 1)
                {
                    ++result.stats.original_only_h;
                }
                else if (modify_kind == 2)
                {
                    ++result.stats.promoted_only_h;
                }
            }
        };

        for (int gi = 0; gi < g; ++gi)
        {
            const int m = (1 << gi);
            for (int v : query.groups[gi])
            {
                Modify(m, v, 0.0, 0);
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
        std::vector<int> original_target(n + 1, 0);
        std::vector<int> pending_root_seen(n + 1, 0);
        int pending_layer_stamp = 0;
        int current_layer = -1;
        int current_layer_pending_roots = 0;
        int stamp = 0;
        for (int mask : order)
        {
            if (popcnt[mask] > H)
            {
                break;
            }
            if (popcnt[mask] != current_layer)
            {
                if (current_layer >= 0)
                {
                    result.stats.max_pending_roots_in_layer =
                        std::max(result.stats.max_pending_roots_in_layer, current_layer_pending_roots);
                }
                current_layer = popcnt[mask];
                ++pending_layer_stamp;
                current_layer_pending_roots = 0;
            }
            const int rem = U ^ mask;
            std::vector<double> lb(n + 1, -1.0);
            auto GetLb = [&](int v)
            {
                if (lb[v] < 0.0)
                {
                    ++result.stats.lb_calls;
                    lb[v] = LowerBound(v, rem);
                }
                return lb[v];
            };

            ++stamp;
            ++result.stats.masks_processed;
            std::priority_queue<std::pair<double, int>,
                                std::vector<std::pair<double, int>>,
                                std::greater<std::pair<double, int>>> pq;
            std::vector<std::pair<int, double>> pending_promoted;

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
                    ++result.stats.pq_pushes;
                    ++used_cnt;
                    ++result.stats.target_seed;
                    status[i] = stamp;
                    original_target[i] = stamp;
                    mx_used = std::max(mx_used, dp[mask][i]);
                }
            }
            for (int i : active_vertices[mask])
            {
                if ((dp[mask][i] + h[rem][i] > best || dp[mask][i] + GetLb(i) > best) &&
                    dp[mask][i] < mx_used)
                {
                    ++result.stats.prefix_considered;
                    if (dp[mask][i] + GetLb(i) > best)
                    {
                        ++result.stats.prefix_pruned_lb;
                        continue;
                    }
                    pq.push({dp[mask][i] + GetLb(i), i});
                    ++result.stats.prefix_pushed;
                    ++result.stats.pq_pushes;
                }
            }

            while (!pq.empty() && used_cnt > 0)
            {
                auto [key, u] = pq.top();
                pq.pop();
                ++result.stats.pq_pops;
                const double d = dp[mask][u];
                if (key > d + GetLb(u))
                {
                    ++result.stats.pop_skip_stale;
                    continue;
                }
                if (d + GetLb(u) > best)
                {
                    ++result.stats.pop_skip_lb;
                    continue;
                }
                ++result.stats.total_inqueue;
                ++result.stats.inqueue_by_size[popcnt[mask]];
                if (status[u] == stamp)
                {
                    ++result.stats.pop_target;
                    --used_cnt;
                    if (original_target[u] == stamp)
                    {
                        ++result.stats.modify_original_target;
                        Modify(mask, u, d, 1);
                    }
                    else
                    {
                        ++result.stats.modify_promoted_target;
                        pending_promoted.push_back({u, d});
                        if (pending_root_seen[u] == pending_layer_stamp)
                        {
                            ++result.stats.pending_layer_repeat_roots;
                        }
                        else
                        {
                            pending_root_seen[u] = pending_layer_stamp;
                            ++result.stats.pending_layer_roots;
                            ++current_layer_pending_roots;
                        }
                    }
                }
                else
                {
                    ++result.stats.pop_prefix;
                }
                for (const auto& e : graph.adj[u])
                {
                    ++result.stats.relax_attempts;
                    if (dp[mask][e.to] > d + e.w)
                    {
                        const double nd = d + e.w;
                        if (nd + GetLb(e.to) > best)
                        {
                            continue;
                        }
                        if (status[u] != status[e.to])
                        {
                            if (status[e.to] == stamp && status[u] != stamp)
                            {
                                ++result.stats.target_demoted;
                            }
                            if (status[e.to] != stamp && status[u] == stamp)
                            {
                                ++result.stats.target_promoted;
                            }
                            status[e.to] = status[u];
                            if (status[e.to] == stamp)
                            {
                                ++used_cnt;
                            }
                        }
                        SetDp(mask, e.to, nd);
                        ++result.stats.relax_success;
                        pq.push({nd + GetLb(e.to), e.to});
                        ++result.stats.pq_pushes;
                    }
                }
            }

            if (!pending_promoted.empty())
            {
                result.stats.pending_promoted += static_cast<long long>(pending_promoted.size());
                std::sort(pending_promoted.begin(), pending_promoted.end());
                int last_vertex = -1;
                double best_value = fp::kInf;
                auto flush_pending = [&]()
                {
                    if (last_vertex >= 0)
                    {
                        ++result.stats.pending_promoted_unique;
                        Modify(mask, last_vertex, best_value, 2);
                    }
                };
                for (const auto& item : pending_promoted)
                {
                    if (item.first != last_vertex)
                    {
                        flush_pending();
                        last_vertex = item.first;
                        best_value = item.second;
                    }
                    else
                    {
                        best_value = std::min(best_value, item.second);
                    }
                }
                flush_pending();
            }
        }
        if (current_layer >= 0)
        {
            result.stats.max_pending_roots_in_layer =
                std::max(result.stats.max_pending_roots_in_layer, current_layer_pending_roots);
        }

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
}  // namespace gst::methods::test12
