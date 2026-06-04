#include "test7.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <stdexcept>
#include <utility>
#include <queue>
#include <iostream>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

namespace gst::methods::test7
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
            throw std::runtime_error("Test7 currently supports group count <= 22.");
        }
        if (!IsQueryFeasible(graph, query))
        {
            result.best_weight = -1.0;
            result.feasible = false;
            result.stats.valid_by_size.assign(g + 1, 0);
            result.stats.total_by_size.assign(g + 1, 0);
            return result;
        }

        auto Init_result = [&]()
        {
            result.stats.total_by_size.assign(g + 1, 0);
            result.stats.valid_by_size.assign(g + 1, 0);
            result.stats.total_valid = 0;
            result.stats.total_up_subset = 0;
            result.stats.total_inqueue = 0;
            result.stats.dense_up_subset_would = 0;
            result.stats.lb_filtered_seed = 0;
            result.stats.lb_filtered_relax = 0;
            result.stats.initial_upper = -1.0;
        };
        Init_result();

        int H = g / 2, U = (1 << g) - 1;
        std::vector<int> popcnt(1 << g);
        for(int s = 0; s <= U; s++)
            popcnt[s] = popcnt[s >> 1] + (s & 1), result.stats.total_by_size[popcnt[s]] += n;

        std::vector<std::vector<double>> group_dist(g, std::vector<double>(n + 1, fp::kInf));
        for (int gi = 0; gi < g; ++gi)
        {
            auto& dist = group_dist[gi];
            std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<std::pair<double, int>>> pq;
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
                    continue;
                for (const auto& e : graph.adj[u])
                {
                    if (dist[e.to] > d + e.w)
                        pq.push({dist[e.to] = d + e.w, e.to});
                }
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
        for (int v = 1; v <= n; ++v)
        {
            double star = 0.0;
            for (int gi = 0; gi < g; ++gi)
                star += group_dist[gi][v];
            best = std::min(best, star);
        }
        result.stats.initial_upper = best;

        std::vector<std::vector<double>> dp(1 << g, std::vector<double>(n + 1, fp::kInf));
        std::vector<std::vector<double>> h(1 << g, std::vector<double>(n + 1, -1.0)); // h <- max dp
        std::vector<std::vector<int>> root_masks(n + 1);
        for(int i = 1; i <= n; i++)
            dp[0][i] = 0.0;

        auto SetDp = [&](int mask, int v, double value)
        {
            const bool first_seen = dp[mask][v] >= fp::kInf / 4;
            dp[mask][v] = value;
            if (first_seen)
                root_masks[v].push_back(mask);
        };

        auto Modify = [&](int mask, int v, double w)
        {
            SetDp(mask, v, w);
            result.stats.total_valid++;
            result.stats.valid_by_size[popcnt[mask]]++;
            result.stats.dense_up_subset_would += (1LL << popcnt[U ^ mask]);

            auto RelaxSameRoot = [&](int t)
            {
                result.stats.total_up_subset++;
                int nxt = mask | t;
                const double cand = dp[mask][v] + dp[t][v];
                if (cand < dp[nxt][v])
                {
                    SetDp(nxt, v, cand);
                }
                h[nxt][v] = std::max(h[nxt][v], dp[mask][v]);
                if(nxt == U && dp[nxt][v] < best)
                {
                    best = dp[nxt][v];
                }
            };

            RelaxSameRoot(0);
            const int old_size = static_cast<int>(root_masks[v].size());
            for (int idx = 0; idx < old_size; ++idx)
            {
                const int t = root_masks[v][idx];
                if (t != mask && (t & mask) == 0)
                    RelaxSameRoot(t);
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

        std::vector <int> order;
        for(int mask = 1; mask <= U; mask++)
            order.push_back(mask);
        std::sort(order.begin(), order.end(), [&](int a, int b){return popcnt[a] < popcnt[b];});
        for(int mask : order)
        {
            if(popcnt[mask] > H)
                break;
            const int rem = U ^ mask;
            std::vector<double> lb(n + 1, 0.0);
            for (int i = 1; i <= n; ++i)
                lb[i] = LowerBound(i, rem);
            std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<std::pair<double, int>>> pq;

            double mx_used = 0;
            int used_cnt = 0;
            std::vector <int> status(n + 1, 0);
            for(int i = 1; i <= n; i++)
            {
                if(dp[mask][i] + h[rem][i] <= best && dp[mask][i] + lb[i] <= best)
                    pq.push({dp[mask][i] + lb[i], i}), used_cnt++, status[i] = 1, mx_used = std::max(mx_used, dp[mask][i]);
                else if(dp[mask][i] + h[rem][i] <= best && dp[mask][i] + lb[i] > best)
                    result.stats.lb_filtered_seed++;
            }
            for(int i = 1; i <= n; i++)
                if((dp[mask][i] + h[rem][i] > best || dp[mask][i] + lb[i] > best) && dp[mask][i] < mx_used)
                    pq.push({dp[mask][i] + lb[i], i});

            while(!pq.empty() && used_cnt > 0)
            {
                auto [key, u] = pq.top();
                pq.pop();
                double d = dp[mask][u];
                if(key > d + lb[u] || d + lb[u] > best)
                    continue;
                result.stats.total_inqueue++;
                if(status[u])
                {
                    --used_cnt;
                    Modify(mask, u, d);
                }
                for(const auto& e : graph.adj[u])
                {
                    if(dp[mask][e.to] > d + e.w)
                    {
                        const double nd = d + e.w;
                        if(nd + lb[e.to] > best)
                        {
                            result.stats.lb_filtered_relax++;
                            continue;
                        }
                        if(status[u] != status[e.to])
                        {
                            status[e.to] = status[u];
                            if(status[e.to] == 1)
                                ++used_cnt;
                        }
                        SetDp(mask, e.to, nd);
                        pq.push({nd + lb[e.to], e.to});
                    }

                }
            }
        }
        result.stats.valid_by_size.resize(H + 1);
        result.best_weight = best;
        result.feasible = true;
        return result;
    }
}
