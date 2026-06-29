#include "half_dpbf_solver.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "../../float_compare.h"

namespace gst::methods::half_dpbf
{

namespace
{

enum class ParentType : unsigned char
{
    kNone = 0,
    kBase = 1,
    kMerge = 2,
    kPath = 3,
};

struct ParentInfo
{
    ParentType type = ParentType::kNone;
    int a = -1;
    int b = -1;
    int edge_id = -1;
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

void RelaxByDijkstra(const Graph& graph,
                     int mask,
                     std::vector<std::vector<double>>& dp,
                     std::vector<std::vector<ParentInfo>>& parent)
{
    using Node = std::pair<double, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
    for (int v = 1; v <= graph.n; ++v)
    {
        if (dp[mask][v] < fp::kInf / 4)
        {
            pq.push({dp[mask][v], v});
        }
    }

    while (!pq.empty())
    {
        const auto [dist_u, u] = pq.top();
        pq.pop();
        if (fp::Cmp(dist_u, dp[mask][u]) > 0)
        {
            continue;
        }
        for (const auto& e : graph.adj[u])
        {
            const double cand = dist_u + e.w;
            if (fp::Lt(cand, dp[mask][e.to]))
            {
                dp[mask][e.to] = cand;
                parent[mask][e.to] = ParentInfo{ParentType::kPath, u, -1, e.edge_id};
                pq.push({cand, e.to});
            }
        }
    }
}

}  // namespace

SolveResult SolveOneQuery(const Graph& graph, const Query& query)
{
    const int g = static_cast<int>(query.groups.size());
    SolveResult result;
    if (g == 0)
    {
        result.best_weight = 0.0;
        result.feasible = true;
        return result;
    }
    if (g >= 31)
    {
        throw std::runtime_error("Half_DPBF currently supports group count <= 30.");
    }

    const int full_mask = (1 << g) - 1;
    const int half_limit = (g + 1) / 2;
    std::vector<int> bit_count(1 << g, 0);
    for (int mask = 1; mask <= full_mask; ++mask)
    {
        bit_count[mask] = CountBits(mask);
    }

    // 仅计算 |mask| <= ceil(g/2) 的状态。
    std::vector<std::vector<double>> dp(1 << g, std::vector<double>(graph.n + 1, fp::kInf));
    std::vector<std::vector<ParentInfo>> parent(1 << g, std::vector<ParentInfo>(graph.n + 1));

    for (int gi = 0; gi < g; ++gi)
    {
        const int mask = (1 << gi);
        for (int v : query.groups[gi])
        {
            if (v < 1 || v > graph.n)
            {
                throw std::runtime_error("Query vertex id out of range.");
            }
            dp[mask][v] = 0.0;
            parent[mask][v] = ParentInfo{ParentType::kBase, gi, -1, -1};
        }
        RelaxByDijkstra(graph, mask, dp, parent);
    }

    for (int mask = 1; mask <= full_mask; ++mask)
    {
        if (bit_count[mask] <= 1 || bit_count[mask] > half_limit)
        {
            continue;
        }
        for (int sub = (mask - 1) & mask; sub > 0; sub = (sub - 1) & mask)
        {
            int other = mask ^ sub;
            if (sub > other)
            {
                continue;
            }
            for (int v = 1; v <= graph.n; ++v)
            {
                const double cand = dp[sub][v] + dp[other][v];
                if (fp::Lt(cand, dp[mask][v]))
                {
                    dp[mask][v] = cand;
                    parent[mask][v] = ParentInfo{ParentType::kMerge, sub, other, -1};
                }
            }
        }
        RelaxByDijkstra(graph, mask, dp, parent);
    }

    // 预先收集每个根可用的小集合状态。
    std::vector<std::vector<int>> valid_small_masks(graph.n + 1);
    for (int v = 1; v <= graph.n; ++v)
    {
        for (int mask = 1; mask <= full_mask; ++mask)
        {
            if (bit_count[mask] <= half_limit && fp::Lt(dp[mask][v], fp::kInf / 4))
            {
                valid_small_masks[v].push_back(mask);
            }
        }
    }

    int best_root = -1;
    double best = fp::kInf;
    std::vector<int> best_prev_mask(1 << g, -1);
    std::vector<int> best_take_sub(1 << g, -1);

    // 枚举根，做“集合覆盖”DP：从多个小集合拼成全集。
    for (int root = 1; root <= graph.n; ++root)
    {
        if (valid_small_masks[root].empty())
        {
            continue;
        }
        std::vector<double> cover(1 << g, fp::kInf);
        std::vector<int> prev_mask(1 << g, -1);
        std::vector<int> take_sub(1 << g, -1);
        cover[0] = 0.0;

        for (int mask = 0; mask <= full_mask; ++mask)
        {
            if (!fp::Lt(cover[mask], fp::kInf / 4))
            {
                continue;
            }
            for (int sub : valid_small_masks[root])
            {
                int nmask = mask | sub;
                const double cand = cover[mask] + dp[sub][root];
                if (fp::Lt(cand, cover[nmask]))
                {
                    cover[nmask] = cand;
                    prev_mask[nmask] = mask;
                    take_sub[nmask] = sub;
                }
            }
        }

        if (fp::Lt(cover[full_mask], best))
        {
            best = cover[full_mask];
            best_root = root;
            best_prev_mask = std::move(prev_mask);
            best_take_sub = std::move(take_sub);
        }
    }

    if (!fp::Lt(best, fp::kInf / 4))
    {
        result.best_weight = -1.0;
        result.feasible = false;
        return result;
    }

    result.best_weight = best;
    result.feasible = true;

    // 统计小集合状态中低于 V, V/2, V/4, V/8 的数量。
    for (int mask = 1; mask <= full_mask; ++mask)
    {
        if (bit_count[mask] > half_limit)
        {
            continue;
        }
        for (int v = 1; v <= graph.n; ++v)
        {
            const double cur = dp[mask][v];
            if (fp::Lt(cur, fp::kInf / 4))
            {
                ++result.stats.stage1_total;
            }
            if (!fp::Lt(cur, best))
            {
                continue;
            }
            ++result.stats.lt_v;
            if (fp::Lt(cur, best / 2.0))
            {
                ++result.stats.lt_v2;
            }
            if (fp::Lt(cur, best / 4.0))
            {
                ++result.stats.lt_v4;
            }
            if (fp::Lt(cur, best / 8.0))
            {
                ++result.stats.lt_v8;
            }
        }
    }

    return result;
}

}  // namespace gst::methods::half_dpbf
