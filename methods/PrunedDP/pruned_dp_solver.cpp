/*
 * PrunedDP++ baseline (pinkyhead 2025/9/15)
 * 接入 GST Framework：使用统一 Graph/Query 接口，仅输出最优边权。
 */
#include "pruned_dp_solver.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

namespace gst::methods::pruned_dp
{

namespace
{

using db = long double;

const db kEps = static_cast<db>(1e-10);
const db kInf = static_cast<db>(std::numeric_limits<double>::max() / 3.0);

struct Node
{
    int v = 0;
    int X = 0;
    db cost = 0.0;
    db lower_bound = 0.0;

    Node(int v, int X, db cost, db lower_bound)
        : v(v), X(X), cost(cost), lower_bound(lower_bound)
    {
    }

    bool operator<(const Node& other) const
    {
        if (lower_bound == other.lower_bound)
        {
            return cost > other.cost;
        }
        return lower_bound > other.lower_bound;
    }
};

struct SolverContext
{
    int n = 0;
    int group_count = 0;
    int new_n = 0;
    int mask = 0;

    std::vector<std::vector<int>> groups;
    std::vector<std::vector<int>> color;
    std::vector<std::vector<std::pair<int, db>>> adj;
    std::vector<std::vector<std::vector<db>>> w_pair;
    std::vector<std::vector<db>> w_single;
    std::vector<std::vector<std::pair<int, db>>> dp_state;
    std::priority_queue<Node> pq;
    std::vector<std::vector<db>> dist;
    std::vector<std::vector<db>> predict;

    db best = kInf;
    PrunedDpStats stats;
};

int CountBits(int x)
{
    int count = 0;
    while (x != 0)
    {
        x &= (x - 1);
        ++count;
    }
    return count;
}

namespace all_paths
{

struct Path
{
    int u = 0;
    int v = 0;
    int X = 0;
    db cost = 0.0;

    Path(int u, int v, int X, db cost) : u(u), v(v), X(X), cost(cost)
    {
    }

    bool operator<(const Path& other) const
    {
        return cost > other.cost;
    }
};

void InitVisited(std::vector<std::vector<std::vector<int>>>& visited, int group_count, int mask)
{
    visited.assign(group_count + 1, std::vector<std::vector<int>>(group_count + 1, std::vector<int>(mask + 1, 0)));
}

void CalW(SolverContext& ctx)
{
    std::vector<std::vector<std::vector<int>>> visited;
    InitVisited(visited, ctx.group_count, ctx.mask);
    std::priority_queue<Path> q;

    for (int i = 1; i <= ctx.group_count; ++i)
    {
        for (int j = 1; j <= ctx.group_count; ++j)
        {
            q.push(Path(i, j, (1 << (i - 1)) | (1 << (j - 1)), ctx.dist[i][j + ctx.n]));
            q.push(Path(i, j, 1 << (i - 1), ctx.dist[i][j + ctx.n]));
            q.push(Path(i, j, 1 << (j - 1), ctx.dist[i][j + ctx.n]));
            q.push(Path(i, j, 0, ctx.dist[i][j + ctx.n]));
        }
    }

    while (!q.empty())
    {
        const Path it = q.top();
        q.pop();
        if (visited[it.u][it.v][it.X])
        {
            continue;
        }
        visited[it.u][it.v][it.X] = 1;
        ctx.w_single[it.u][it.X] = std::min(ctx.w_single[it.u][it.X], ctx.w_pair[it.u][it.v][it.X]);
        for (int i = 1; i <= ctx.group_count; ++i)
        {
            const int new_x = it.X | (1 << (i - 1));
            const db new_cost = it.cost + ctx.dist[it.v][i + ctx.n];
            if (new_cost < ctx.w_pair[it.u][i][new_x])
            {
                ctx.w_pair[it.u][i][new_x] = new_cost;
                q.push(Path(it.u, i, new_x, new_cost));
            }
            if (new_cost < ctx.w_pair[it.u][i][it.X])
            {
                ctx.w_pair[it.u][i][it.X] = new_cost;
                q.push(Path(it.u, i, it.X, new_cost));
            }
        }
    }

    for (int i = 1; i <= ctx.n; ++i)
    {
        ctx.predict[i][0] = 0;
        for (int j = 1; j <= ctx.mask; ++j)
        {
            db min_1 = kInf;
            db max_2 = 0;
            db max_3 = 0;
            for (int u = 1; u <= ctx.group_count; ++u)
            {
                if (!(j & (1 << (u - 1))))
                {
                    continue;
                }
                db min_2 = kInf;
                for (int v = 1; v <= ctx.group_count; ++v)
                {
                    if (!(j & (1 << (v - 1))))
                    {
                        continue;
                    }
                    min_1 = std::min(min_1, ctx.dist[u][i] + ctx.w_pair[u][v][j] + ctx.dist[v][i]);
                    min_2 = std::min(min_2, ctx.dist[v][i]);
                }
                max_2 = std::max(max_2, ctx.dist[u][i] + ctx.w_single[u][j] + min_2);
            }
            for (int k = 1; k <= ctx.group_count; ++k)
            {
                if (!(j & (1 << (k - 1))))
                {
                    continue;
                }
                max_3 = std::max(max_3, ctx.dist[k][i]);
            }
            ctx.predict[i][j] = std::max(std::max(min_1 / 2, max_2 / 2), max_3);
        }
    }
}

}  // namespace all_paths

void CalDist(SolverContext& ctx)
{
    using DistNode = std::pair<db, int>;
    std::priority_queue<DistNode, std::vector<DistNode>, std::greater<DistNode>> q;
    for (int i = 1; i <= ctx.group_count; ++i)
    {
        std::vector<bool> used(ctx.new_n + 1, false);
        ctx.dist[i][i + ctx.n] = 0;
        q.push({0, i + ctx.n});
        while (!q.empty())
        {
            const auto [val, u] = q.top();
            q.pop();
            if (used[u])
            {
                continue;
            }
            used[u] = true;
            for (const auto& [v, w] : ctx.adj[u])
            {
                if (ctx.dist[i][v] > ctx.dist[i][u] + w)
                {
                    ctx.dist[i][v] = ctx.dist[i][u] + w;
                    if (v <= ctx.n)
                    {
                        q.push({ctx.dist[i][v], v});
                    }
                }
            }
        }
    }

    ctx.adj.resize(ctx.n + 1);
    for (int i = 1; i <= ctx.n; ++i)
    {
        while (!ctx.adj[i].empty() && ctx.adj[i].back().first > ctx.n)
        {
            ctx.adj[i].pop_back();
        }
    }
}

db LowerBound(const SolverContext& ctx, int v, int X, db cost)
{
    const int inv_x = X ^ ctx.mask;
    return ctx.predict[v][inv_x] + cost;
}

void Update(SolverContext& ctx, int v, int X, db cost, db lower_bound)
{
    ctx.stats.update_calls++;
    if (ctx.dp_state[v][X].first)
    {
        ctx.stats.update_finalized_skip++;
        return;
    }
    lower_bound = std::max(LowerBound(ctx, v, X, cost), lower_bound);
    if (lower_bound >= ctx.best)
    {
        ctx.stats.update_bound_pruned++;
        return;
    }
    if (X == ctx.mask)
    {
        if (cost < ctx.best)
        {
            ctx.best = cost;
            ctx.stats.best_full_updates++;
        }
    }
    ctx.stats.pq_pushes++;
    ctx.stats.update_pushes++;
    const int bits = CountBits(X);
    if (bits >= 0 && bits < static_cast<int>(ctx.stats.update_push_by_size.size()))
    {
        ctx.stats.update_push_by_size[bits]++;
    }
    ctx.pq.push(Node(v, X, cost, lower_bound));
}

db PrunedDpPlusPlus(SolverContext& ctx)
{
    auto phase_start = std::chrono::steady_clock::now();
    CalDist(ctx);
    ctx.stats.dist_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - phase_start).count();
    phase_start = std::chrono::steady_clock::now();
    all_paths::CalW(ctx);
    ctx.stats.calw_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - phase_start).count();

    for (int i = 1; i <= ctx.n; ++i)
    {
        for (const int group_id : ctx.color[i])
        {
            const int X = 1 << (group_id - 1);
            ctx.stats.initial_pushes++;
            ctx.stats.pq_pushes++;
            ctx.pq.push(Node(i, X, 0, LowerBound(ctx, i, X, 0)));
        }
    }

    phase_start = std::chrono::steady_clock::now();
    while (!ctx.pq.empty())
    {
        const Node cur = ctx.pq.top();
        ctx.pq.pop();
        ctx.stats.pq_pops++;
        if (ctx.dp_state[cur.v][cur.X].first)
        {
            ctx.stats.stale_pops++;
            continue;
        }
        ctx.dp_state[cur.v][cur.X] = {1, cur.cost};
        ctx.stats.finalized_labels++;
        const int cur_bits = CountBits(cur.X);
        if (cur_bits >= 0 && cur_bits < static_cast<int>(ctx.stats.finalized_by_size.size()))
        {
            ctx.stats.finalized_by_size[cur_bits]++;
        }
        if (cur.cost >= ctx.best)
        {
            ctx.stats.cost_ge_best_skips++;
            continue;
        }
        if (cur.X == ctx.mask)
        {
            ctx.best = cur.cost;
            continue;
        }

        const int inv_x = ctx.mask ^ cur.X;
        db expect_cost = cur.cost;
        for (int i = 1; i <= ctx.group_count; ++i)
        {
            if ((1 << (i - 1)) & inv_x)
            {
                expect_cost += ctx.dist[i][cur.v];
                expect_cost = std::min(expect_cost, kInf);
            }
        }
        if (expect_cost < ctx.best)
        {
            ctx.best = expect_cost;
            ctx.stats.best_expect_updates++;
        }

        if (ctx.dp_state[cur.v][inv_x].first)
        {
            Update(ctx, cur.v, ctx.mask, cur.cost + ctx.dp_state[cur.v][inv_x].second, cur.lower_bound);
        }

        if (cur.cost <= ctx.best / 2 + kEps)
        {
            for (const auto& [u, val] : ctx.adj[cur.v])
            {
                assert(u <= ctx.n);
                ctx.stats.edge_relax_attempts++;
                const db new_cost = cur.cost + val;
                Update(ctx, u, cur.X, new_cost, cur.lower_bound);
            }
            for (int i = inv_x; i >= 1; i = (i - 1) & inv_x)
            {
                ctx.stats.merge_submask_attempts++;
                if (!ctx.dp_state[cur.v][i].first)
                {
                    continue;
                }
                ctx.stats.merge_state_hits++;
                const db new_cost = cur.cost + ctx.dp_state[cur.v][i].second;
                if (new_cost <= 2.0 / 3 * ctx.best + kEps)
                {
                    ctx.stats.merge_cost_gate_pass++;
                    Update(ctx, cur.v, i | cur.X, new_cost, cur.lower_bound);
                }
            }
        }
    }
    ctx.stats.search_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - phase_start).count();

    return ctx.best;
}

void ClearState(SolverContext& ctx)
{
    ctx.new_n = ctx.n + ctx.group_count;
    ctx.mask = (1 << ctx.group_count) - 1;

    ctx.groups.assign(ctx.group_count + 1, {});
    ctx.color.assign(ctx.n + 1, {});
    while (!ctx.pq.empty())
    {
        ctx.pq.pop();
    }

    ctx.w_pair.assign(ctx.group_count + 1, std::vector<std::vector<db>>(ctx.group_count + 1));
    for (int i = 1; i <= ctx.group_count; ++i)
    {
        for (int j = 1; j <= ctx.group_count; ++j)
        {
            ctx.w_pair[i][j].assign(ctx.mask + 1, kInf);
        }
    }

    ctx.w_single.assign(ctx.group_count + 1, std::vector<db>(ctx.mask + 1, kInf));
    ctx.dp_state.assign(ctx.n + 1, std::vector<std::pair<int, db>>(ctx.mask + 1, {0, kInf}));
    ctx.best = kInf;

    ctx.dist.assign(ctx.group_count + 1, std::vector<db>(ctx.new_n + 1, kInf));
    ctx.predict.assign(ctx.n + 1, std::vector<db>(ctx.mask + 1, 0));
    ctx.stats = PrunedDpStats{};
    ctx.stats.n = ctx.n;
    ctx.stats.g = ctx.group_count;
    ctx.stats.finalized_by_size.assign(ctx.group_count + 1, 0);
    ctx.stats.update_push_by_size.assign(ctx.group_count + 1, 0);
}

void BuildAdjacency(const Graph& graph, const Query& query, SolverContext& ctx)
{
    const auto build_start = std::chrono::steady_clock::now();
    ctx.n = graph.n;
    ctx.group_count = static_cast<int>(query.groups.size());
    ClearState(ctx);
    ctx.stats.m = graph.m;

    for (int gi = 0; gi < ctx.group_count; ++gi)
    {
        const int group_id = gi + 1;
        ctx.stats.total_group_vertices += static_cast<int>(query.groups[gi].size());
        ctx.stats.max_group_size =
            std::max(ctx.stats.max_group_size, static_cast<int>(query.groups[gi].size()));
        for (const int u : query.groups[gi])
        {
            ctx.groups[group_id].push_back(u);
            ctx.color[u].push_back(group_id);
        }
    }

    ctx.adj.assign(ctx.new_n + 1, {});
    for (int u = 1; u <= graph.n; ++u)
    {
        for (const auto& edge : graph.adj[u])
        {
            ctx.adj[u].push_back({edge.to, static_cast<db>(edge.w)});
        }
    }

    for (int i = 1; i <= ctx.group_count; ++i)
    {
        const int virtual_vertex = i + ctx.n;
        for (const int u : ctx.groups[i])
        {
            ctx.adj[virtual_vertex].push_back({u, 0});
            ctx.adj[u].push_back({virtual_vertex, 0});
        }
    }
    ctx.stats.build_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - build_start).count();
}

double SolveWeight(const Graph& graph, const Query& query, PrunedDpStats* stats)
{
    const auto total_start = std::chrono::steady_clock::now();
    SolverContext ctx;
    BuildAdjacency(graph, query, ctx);
    const db ans = PrunedDpPlusPlus(ctx);
    ctx.stats.total_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - total_start).count();
    if (stats != nullptr)
    {
        *stats = std::move(ctx.stats);
    }
    if (ans >= kInf)
    {
        return -1.0;
    }
    return static_cast<double>(ans);
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
        throw std::runtime_error("PrunedDP currently supports group count <= 30.");
    }
    if (!IsQueryFeasible(graph, query))
    {
        result.best_weight = -1.0;
        result.feasible = false;
        return result;
    }

    const double weight = SolveWeight(graph, query, &result.stats);
    if (weight < 0.0)
    {
        result.best_weight = -1.0;
        result.feasible = false;
        return result;
    }

    result.best_weight = weight;
    result.feasible = true;
    return result;
}

}  // namespace gst::methods::pruned_dp
