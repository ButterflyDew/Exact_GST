#include "dpbf_solver.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "../../float_compare.h"

namespace gst::methods::dpbf
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
    int a = -1;       // base: 组号；merge: 左子掩码；path: 前驱点
    int b = -1;       // merge: 右子掩码
    int edge_id = -1; // path: 采用的边 id
};

// 对固定 mask 执行一次最短路松弛：把“同一个已覆盖组集合”在图上扩展到其它点。
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
        throw std::runtime_error("DPBF currently supports group count <= 30.");
    }

    const int full_mask = (1 << g) - 1;
    std::vector<std::vector<double>> dp(1 << g, std::vector<double>(graph.n + 1, fp::kInf));
    std::vector<std::vector<ParentInfo>> parent(1 << g, std::vector<ParentInfo>(graph.n + 1));

    // DP 含义：dp[mask][v] 表示“已覆盖 mask 所指组集合，并且当前汇聚在点 v”的最小代价。
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
        if ((mask & (mask - 1)) == 0)
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

    int best_root = -1;
    double best = fp::kInf;
    for (int v = 1; v <= graph.n; ++v)
    {
        if (fp::Lt(dp[full_mask][v], best))
        {
            best = dp[full_mask][v];
            best_root = v;
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
    return result;
}

}  // namespace gst::methods::dpbf
