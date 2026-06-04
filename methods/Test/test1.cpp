#include "test1.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <stdexcept>
#include <utility>

#include "../../float_compare.h"

namespace gst::methods::test1
{

namespace
{

struct ParentInfoDummy
{
};

int CountBits(int x)
{
    int c = 0;
    while (x > 0)
    {
        x &= (x - 1);
        ++c;
    }
    return c;
}

long long Comb(int n, int k)
{
    if (k < 0 || k > n)
    {
        return 0;
    }
    k = std::min(k, n - k);
    long long ans = 1;
    for (int i = 1; i <= k; ++i)
    {
        ans = ans * (n - k + i) / i;
    }
    return ans;
}

void RelaxByDijkstra(const Graph& graph, std::vector<double>& dist)
{
    using Node = std::pair<double, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
    for (int v = 1; v <= graph.n; ++v)
    {
        if (dist[v] < fp::kInf / 4)
        {
            pq.push({dist[v], v});
        }
    }
    while (!pq.empty())
    {
        const auto [du, u] = pq.top();
        pq.pop();
        if (fp::Cmp(du, dist[u]) > 0)
        {
            continue;
        }
        for (const auto& e : graph.adj[u])
        {
            const double cand = du + e.w;
            if (fp::Lt(cand, dist[e.to]))
            {
                dist[e.to] = cand;
                pq.push({cand, e.to});
            }
        }
    }
}

}  // namespace

SolveResult SolveOneQuery(const Graph& graph, const Query& query, dpbf::OutputMode /*output_mode*/)
{
    const int g = static_cast<int>(query.groups.size());
    SolveResult result;
    if (g == 0)
    {
        result.best_weight = 0.0;
        result.feasible = true;
        return result;
    }
    if (g >= 25)
    {
        throw std::runtime_error("Test1 currently supports group count <= 24.");
    }

    const int full_mask = (1 << g) - 1;
    const int q = (g + 3) / 4;  // ceil(g/4)
    const int h = g / 2;        // floor(g/2)

    std::vector<int> bit_count(1 << g, 0);
    std::vector<std::vector<int>> masks_by_size(g + 1);
    for (int mask = 1; mask <= full_mask; ++mask)
    {
        bit_count[mask] = bit_count[mask >> 1] + (mask & 1);
        masks_by_size[bit_count[mask]].push_back(mask);
    }

    // 仅维护 |s|<=q 的 dp。
    std::vector<std::vector<double>> dp(1 << g, std::vector<double>(graph.n + 1, fp::kInf));
    for (int gi = 0; gi < g; ++gi)
    {
        int mask = (1 << gi);
        for (int v : query.groups[gi])
        {
            if (v < 1 || v > graph.n)
            {
                throw std::runtime_error("Query vertex id out of range.");
            }
            dp[mask][v] = 0.0;
        }
        RelaxByDijkstra(graph, dp[mask]);
    }

    for (int sz = 2; sz <= q; ++sz)
    {
        for (int mask : masks_by_size[sz])
        {
            auto& cur = dp[mask];
            for (int sub = (mask - 1) & mask; sub > 0; sub = (sub - 1) & mask)
            {
                int other = mask ^ sub;
                if (sub > other)
                {
                    continue;
                }
                for (int v = 1; v <= graph.n; ++v)
                {
                    double cand = dp[sub][v] + dp[other][v];
                    if (fp::Lt(cand, cur[v]))
                    {
                        cur[v] = cand;
                    }
                }
            }
            RelaxByDijkstra(graph, cur);
        }
    }

    // Best_q：用 <=q 的集合做覆盖 DP。
    double best_q = fp::kInf;
    std::vector<int> small_masks;
    for (int sz = 1; sz <= q; ++sz)
    {
        for (int mask : masks_by_size[sz])
        {
            small_masks.push_back(mask);
        }
    }
    for (int v = 1; v <= graph.n; ++v)
    {
        std::vector<double> cover(1 << g, fp::kInf);
        cover[0] = 0.0;
        for (int mask = 0; mask <= full_mask; ++mask)
        {
            if (!fp::Lt(cover[mask], fp::kInf / 4))
            {
                continue;
            }
            for (int sub : small_masks)
            {
                if (!fp::Lt(dp[sub][v], fp::kInf / 4))
                {
                    continue;
                }
                int nmask = mask | sub;
                double cand = cover[mask] + dp[sub][v];
                if (fp::Lt(cand, cover[nmask]))
                {
                    cover[nmask] = cand;
                }
            }
        }
        if (fp::Lt(cover[full_mask], best_q))
        {
            best_q = cover[full_mask];
        }
    }

    if (!fp::Lt(best_q, fp::kInf / 4))
    {
        result.best_weight = -1.0;
        result.feasible = false;
        result.stats.valid_by_size.assign(g + 1, 0);
        result.stats.total_by_size.assign(g + 1, 0);
        return result;
    }

    result.best_weight = best_q;
    result.feasible = true;
    result.stats.valid_by_size.assign(g + 1, 0);
    result.stats.total_by_size.assign(g + 1, 0);
    for (int k = q + 1; k <= h; ++k)
    {
        result.stats.total_by_size[k] = Comb(g, k) * graph.n;
    }

    // 对每个根 v，先求 g_q(v,s)=max_{t subset s, |t|<=q} dp(v,t)，再判断有效状态。
    for (int v = 1; v <= graph.n; ++v)
    {
        std::vector<double> gq(1 << g, -1.0);
        for (int sub : small_masks)
        {
            if (fp::Lt(dp[sub][v], fp::kInf / 4))
            {
                gq[sub] = std::max(gq[sub], dp[sub][v]);
            }
        }
        for (int b = 0; b < g; ++b)
        {
            for (int mask = 0; mask <= full_mask; ++mask)
            {
                if (mask & (1 << b))
                {
                    gq[mask] = std::max(gq[mask], gq[mask ^ (1 << b)]);
                }
            }
        }

        std::vector<int> half_masks;
        half_masks.reserve(1 << g);
        for (int sz = 1; sz <= h; ++sz)
        {
            for (int mask : masks_by_size[sz])
            {
                if (gq[mask] >= 0.0)
                {
                    half_masks.push_back(mask);
                }
            }
        }

        // 用 <=h 的块覆盖任意集合（块代价是 g_q）。
        std::vector<double> cover_h(1 << g, fp::kInf);
        cover_h[0] = 0.0;
        for (int mask = 0; mask <= full_mask; ++mask)
        {
            if (!fp::Lt(cover_h[mask], fp::kInf / 4))
            {
                continue;
            }
            for (int sub : half_masks)
            {
                int nmask = mask | sub;
                double cand = cover_h[mask] + gq[sub];
                if (fp::Lt(cand, cover_h[nmask]))
                {
                    cover_h[nmask] = cand;
                }
            }
        }

        // 统计 (v,s) 是否有效：要求存在覆盖 U 的拆分且包含 s。
        for (int k = q + 1; k <= h; ++k)
        {
            for (int s : masks_by_size[k])
            {
                if (gq[s] < 0.0)
                {
                    continue;
                }
                int rem = full_mask ^ s;
                if (!fp::Lt(cover_h[rem], fp::kInf / 4))
                {
                    continue;
                }
                if (fp::Lt(gq[s] + cover_h[rem], best_q))
                {
                    ++result.stats.total_valid;
                    ++result.stats.valid_by_size[k];
                }
            }
        }
    }

    return result;
}

}  // namespace gst::methods::test1
