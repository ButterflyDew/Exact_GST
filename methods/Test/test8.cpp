#include "test8.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>
#include <queue>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

namespace gst::methods::test8
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
            throw std::runtime_error("Test8 currently supports group count <= 22.");
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

        int H = g / 2, U = (1 << g) - 1;
        std::vector<int> popcnt(1 << g);
        for(int s = 0; s <= U; s++)
            popcnt[s] = popcnt[s >> 1] + (s & 1), result.stats.total_by_size[popcnt[s]] += n;

        std::vector<int> vertex_group_mask(n + 1, 0);
        for (int gi = 0; gi < g; ++gi)
        {
            for (int v : query.groups[gi])
            {
                if (v < 1 || v > n)
                    throw std::runtime_error("Query vertex id out of range.");
                vertex_group_mask[v] |= (1 << gi);
            }
        }

        std::vector<std::vector<double>> group_dist(g, std::vector<double>(n + 1, fp::kInf));
        for (int gi = 0; gi < g; ++gi)
        {
            auto& dist = group_dist[gi];
            std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<std::pair<double, int>>> pq;
            for (int v : query.groups[gi])
            {
                if (v < 1 || v > n)
                    throw std::runtime_error("Query vertex id out of range.");
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
                    continue;
                for (const auto& e : graph.adj[u])
                    if (dist[e.to] > d + e.w)
                        pq.push({dist[e.to] = d + e.w, e.to});
            }
        }

        std::vector<std::vector<double>> group_pair(g, std::vector<double>(g, fp::kInf));
        for (int a = 0; a < g; ++a)
            for (int b = 0; b < g; ++b)
                for (int v : query.groups[b])
                    group_pair[a][b] = std::min(group_pair[a][b], group_dist[a][v]);

        std::vector<double> mst_cache(1 << g, -1.0);
        auto MstHalf = [&](int mask)
        {
            if (mask == 0 || (mask & (mask - 1)) == 0)
                return 0.0;
            double& cached = mst_cache[mask];
            if (cached >= 0.0)
                return cached;
            std::vector<double> best_edge(g, fp::kInf);
            std::vector<char> used(g, 0);
            int start = 0;
            while (((mask >> start) & 1) == 0)
                ++start;
            best_edge[start] = 0.0;
            double sum = 0.0;
            for (int iter = 0; iter < popcnt[mask]; ++iter)
            {
                int u = -1;
                for (int a = 0; a < g; ++a)
                    if (((mask >> a) & 1) && !used[a] && (u < 0 || best_edge[a] < best_edge[u]))
                        u = a;
                used[u] = 1;
                sum += best_edge[u];
                for (int a = 0; a < g; ++a)
                    if (((mask >> a) & 1) && !used[a])
                        best_edge[a] = std::min(best_edge[a], group_pair[u][a]);
            }
            return cached = sum * 0.5;
        };

        auto LowerBound = [&](int v, int rem)
        {
            if (rem == 0)
                return 0.0;
            double farthest = 0.0, first = fp::kInf, second = fp::kInf;
            for (int gi = 0; gi < g; ++gi)
            {
                if (((rem >> gi) & 1) == 0)
                    continue;
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
                return farthest;
            return std::max(farthest, MstHalf(rem) + (first + second) * 0.5);
        };

        double best = fp::kInf;
        int best_root = 1;
        for (int v = 1; v <= n; ++v)
        {
            double star = 0.0;
            for (int gi = 0; gi < g; ++gi)
                star += group_dist[gi][v];
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
                std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<std::pair<double, int>>> pq;
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
                        continue;
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
                    return fp::kInf;
            }
            return total;
        };

        const double greedy_best = GreedyUpperBound(best_root);
        if (greedy_best < best)
            best = greedy_best;
        result.stats.greedy_upper = greedy_best;
        result.stats.preprocess_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - solve_start).count();
        auto dp_start = std::chrono::steady_clock::now();

        std::vector<std::vector<double>> dp(1 << g, std::vector<double>(n + 1, fp::kInf));
        std::vector<std::vector<double>> h(1 << g, std::vector<double>(n + 1, -1.0));
        std::vector<std::vector<int>> root_masks(n + 1);
        std::vector<std::vector<int>> active_vertices(1 << g);
        for(int i = 1; i <= n; i++)
            dp[0][i] = 0.0;

        auto SetDp = [&](int mask, int v, double value)
        {
            const bool first_seen = dp[mask][v] >= fp::kInf / 4;
            dp[mask][v] = value;
            if (first_seen)
            {
                root_masks[v].push_back(mask);
                active_vertices[mask].push_back(v);
            }
        };

        auto Modify = [&](int mask, int v, double w)
        {
            SetDp(mask, v, w);
            result.stats.total_valid++;
            result.stats.valid_by_size[popcnt[mask]]++;

            auto RelaxSameRoot = [&](int t)
            {
                result.stats.total_up_subset++;
                result.stats.merge_by_size[popcnt[mask]]++;
                int nxt = mask | t;
                const double cand = dp[mask][v] + dp[t][v];
                if (cand < dp[nxt][v])
                    SetDp(nxt, v, cand);
                h[nxt][v] = std::max(h[nxt][v], dp[mask][v]);
                if(nxt == U && dp[nxt][v] < best)
                    best = dp[nxt][v];
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
                        RelaxSameRoot(t);
                }
            }
            else
            {
                result.stats.merge_dense_checks += dense_count;
                for (int t = rem; t > 0; t = (t - 1) & rem)
                    if (dp[t][v] < fp::kInf / 4)
                        RelaxSameRoot(t);
            }
        };

        for (int gi = 0; gi < g; ++gi)
        {
            const int m = (1 << gi);
            for (int v : query.groups[gi])
            {
                if (v < 1 || v > n)
                    throw std::runtime_error("Query vertex id out of range.");
                Modify(m, v, 0.0);
            }
        }

        std::vector<int> order;
        for(int mask = 1; mask <= U; mask++)
            order.push_back(mask);
        std::sort(order.begin(), order.end(), [&](int a, int b){return popcnt[a] < popcnt[b];});

        std::vector<int> status(n + 1, 0);
        int stamp = 0;
        for(int mask : order)
        {
            if(popcnt[mask] > H)
                break;
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
            std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<std::pair<double, int>>> pq;

            double mx_used = 0;
            int used_cnt = 0;
            result.stats.full_seed_vertices_would += n;
            result.stats.active_seed_vertices += static_cast<long long>(active_vertices[mask].size());
            result.stats.active_by_size[popcnt[mask]] += static_cast<long long>(active_vertices[mask].size());
            result.stats.max_active_vertices = std::max(result.stats.max_active_vertices, static_cast<int>(active_vertices[mask].size()));
            for(int i : active_vertices[mask])
            {
                const double lbi = GetLb(i);
                if(dp[mask][i] + h[rem][i] <= best && dp[mask][i] + lbi <= best)
                    pq.push({dp[mask][i] + lbi, i}), result.stats.pq_pushes++, used_cnt++, status[i] = stamp, mx_used = std::max(mx_used, dp[mask][i]);
            }
            for(int i : active_vertices[mask])
                if((dp[mask][i] + h[rem][i] > best || dp[mask][i] + GetLb(i) > best) && dp[mask][i] < mx_used)
                    pq.push({dp[mask][i] + GetLb(i), i}), result.stats.pq_pushes++;

            while(!pq.empty() && used_cnt > 0)
            {
                auto [key, u] = pq.top();
                pq.pop();
                result.stats.pq_pops++;
                double d = dp[mask][u];
                if(key > d + GetLb(u) || d + GetLb(u) > best)
                    continue;
                result.stats.total_inqueue++;
                result.stats.inqueue_by_size[popcnt[mask]]++;
                if(status[u] == stamp)
                {
                    --used_cnt;
                    Modify(mask, u, d);
                }
                for(const auto& e : graph.adj[u])
                {
                    result.stats.relax_attempts++;
                    if(dp[mask][e.to] > d + e.w)
                    {
                        const double nd = d + e.w;
                        if(nd + GetLb(e.to) > best)
                            continue;
                        if(status[u] != status[e.to])
                        {
                            status[e.to] = status[u];
                            if(status[e.to] == stamp)
                                ++used_cnt;
                        }
                        SetDp(mask, e.to, nd);
                        result.stats.relax_success++;
                        pq.push({nd + GetLb(e.to), e.to});
                        result.stats.pq_pushes++;
                    }
                }
            }
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
}
