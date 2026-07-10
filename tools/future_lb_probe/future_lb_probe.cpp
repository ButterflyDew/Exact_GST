#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <string>
#include <vector>

#include "float_compare.h"
#include "graph_io.h"
#include "methods/DPBF/dpbf_solver.h"
#include "query_io.h"

namespace
{
using gst::Graph;
using gst::Query;
using gst::UndirectedEdge;

struct BoundStats
{
    std::string name;
    long long checked = 0;
    long long ratio_count = 0;
    long long violations = 0;
    long long consistency_checks = 0;
    long long consistency_violations = 0;
    long long splice_checks = 0;
    long long splice_violations = 0;
    long long stronger_than_current = 0;
    long long unsafe_stronger_than_current = 0;
    long long better_than_current = 0;
    double sum_ratio = 0.0;
    double sum_gap = 0.0;
    double sum_stronger_gain = 0.0;
    double sum_safe_gain = 0.0;
    double max_violation = 0.0;
    double max_consistency_violation = 0.0;
    double max_splice_violation = 0.0;
    double max_ratio = 0.0;
    double max_stronger_gain = 0.0;
    double max_safe_gain = 0.0;
};

struct ProbeStats
{
    long long states = 0;
    long long infeasible = 0;
    long long ratio_count = 0;
    double current_sum_ratio = 0.0;
    double current_max_ratio = 0.0;
    double current_sum_gap = 0.0;
    std::vector<BoundStats> bounds;
};

struct Options
{
    std::uint64_t seed = 1;
    int iterations = 100;
    int min_n = 4;
    int max_n = 10;
    int min_g = 2;
    int max_g = 7;
    int samples_per_instance = 64; // 0 means exhaustive over roots and nonempty masks.
    int splice_samples_per_instance = 64; // 0 disables the stronger subset-splice check.
};

using HeapItem = std::pair<double, int>;

struct MetricEdge
{
    int u = 0;
    int v = 0;
    double w = 0.0;
};

struct SmallDsu
{
    std::vector<int> parent;

    explicit SmallDsu(int n) : parent(n)
    {
        for (int i = 0; i < n; ++i)
            parent[i] = i;
    }

    int Find(int x)
    {
        while (parent[x] != x)
        {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }

    bool Union(int a, int b)
    {
        a = Find(a);
        b = Find(b);
        if (a == b)
            return false;
        parent[b] = a;
        return true;
    }
};

void AddEdge(Graph& graph, int u, int v, double w)
{
    UndirectedEdge edge;
    edge.id = static_cast<int>(graph.edges.size());
    edge.u = u;
    edge.v = v;
    edge.w = w;
    graph.edges.push_back(edge);
    graph.adj[u].push_back({v, edge.id, w});
    graph.adj[v].push_back({u, edge.id, w});
    graph.m = static_cast<int>(graph.edges.size());
}

Graph RandomGraph(std::mt19937_64& rng, int n)
{
    Graph graph;
    graph.n = n;
    graph.adj.assign(n + 1, {});
    std::uniform_int_distribution<int> weight(1, 30);
    for (int v = 2; v <= n; ++v)
    {
        std::uniform_int_distribution<int> parent(1, v - 1);
        AddEdge(graph, v, parent(rng), weight(rng));
    }
    std::bernoulli_distribution extra(0.32);
    for (int u = 1; u <= n; ++u)
        for (int v = u + 1; v <= n; ++v)
            if (extra(rng))
                AddEdge(graph, u, v, weight(rng));
    return graph;
}

Query RandomQuery(std::mt19937_64& rng, int n, int g)
{
    Query query;
    query.groups.resize(g);
    std::uniform_int_distribution<int> vertex(1, n);
    std::uniform_int_distribution<int> group_size(1, std::min(3, n));
    for (auto& group : query.groups)
    {
        const int target = group_size(rng);
        while (static_cast<int>(group.size()) < target)
        {
            int v = vertex(rng);
            if (std::find(group.begin(), group.end(), v) == group.end())
                group.push_back(v);
        }
        std::sort(group.begin(), group.end());
    }
    return query;
}

std::vector<std::vector<double>> ComputeGroupDistances(const Graph& graph, const Query& query)
{
    const int g = static_cast<int>(query.groups.size());
    std::vector<std::vector<double>> gd(g, std::vector<double>(graph.n + 1, gst::fp::kInf));
    std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
    for (int a = 0; a < g; ++a)
    {
        while (!heap.empty())
            heap.pop();
        for (int v : query.groups[a])
        {
            gd[a][v] = 0.0;
            heap.push({0.0, v});
        }
        while (!heap.empty())
        {
            auto [d, u] = heap.top();
            heap.pop();
            if (d != gd[a][u])
                continue;
            for (auto e : graph.adj[u])
            {
                double nd = d + e.w;
                if (nd < gd[a][e.to])
                {
                    gd[a][e.to] = nd;
                    heap.push({nd, e.to});
                }
            }
        }
    }
    return gd;
}

std::vector<std::vector<double>> ComputeGroupMetric(const Query& query,
                                                    const std::vector<std::vector<double>>& gd)
{
    const int g = static_cast<int>(query.groups.size());
    std::vector<std::vector<double>> gp(g, std::vector<double>(g, gst::fp::kInf));
    for (int a = 0; a < g; ++a)
        for (int b = 0; b < g; ++b)
            for (int v : query.groups[b])
                gp[a][b] = std::min(gp[a][b], gd[a][v]);
    return gp;
}

double GroupMstHalf(int mask,
                    const std::vector<int>& pc,
                    const std::vector<int>& first_bit,
                    const std::vector<std::vector<double>>& gp)
{
    if (!mask || !(mask & (mask - 1)))
        return 0.0;
    const int g = static_cast<int>(gp.size());
    double sum = 0.0;
    std::vector<double> d(g, gst::fp::kInf);
    std::vector<char> used(g, 0);
    int z = first_bit[mask];
    d[z] = 0.0;
    for (int it = 0; it < pc[mask]; ++it)
    {
        int u = -1;
        for (int a = 0; a < g; ++a)
            if ((mask >> a & 1) && !used[a] && (u < 0 || d[a] < d[u]))
                u = a;
        used[u] = 1;
        sum += d[u];
        for (int a = 0; a < g; ++a)
            if ((mask >> a & 1) && !used[a])
                d[a] = std::min(d[a], gp[u][a]);
    }
    return sum * 0.5;
}

double RootedMetricMstHalf(int root,
                           int mask,
                           const std::vector<int>& first_bit,
                           const std::vector<std::vector<double>>& gd,
                           const std::vector<std::vector<double>>& gp)
{
    std::vector<int> groups;
    for (int t = mask; t; t &= t - 1)
        groups.push_back(first_bit[t]);
    const int k = static_cast<int>(groups.size()) + 1;
    if (k <= 1)
        return 0.0;

    std::vector<double> d(k, gst::fp::kInf);
    std::vector<char> used(k, 0);
    d[0] = 0.0;
    double sum = 0.0;
    for (int it = 0; it < k; ++it)
    {
        int u = -1;
        for (int i = 0; i < k; ++i)
            if (!used[i] && (u < 0 || d[i] < d[u]))
                u = i;
        used[u] = 1;
        sum += d[u];
        for (int i = 0; i < k; ++i)
        {
            if (used[i])
                continue;
            double w = gst::fp::kInf;
            if (u == 0)
                w = gd[groups[i - 1]][root];
            else if (i == 0)
                w = gd[groups[u - 1]][root];
            else
                w = gp[groups[u - 1]][groups[i - 1]];
            d[i] = std::min(d[i], w);
        }
    }
    return sum * 0.5;
}

double OneTreeAllAnchorHalf(int root,
                            int mask,
                            const std::vector<int>& first_bit,
                            const std::vector<std::vector<double>>& gd,
                            const std::vector<std::vector<double>>& gp)
{
    std::vector<int> groups;
    for (int t = mask; t; t &= t - 1)
        groups.push_back(first_bit[t]);
    const int k = static_cast<int>(groups.size()) + 1;
    if (k <= 1)
        return 0.0;

    auto Metric = [&](int a, int b)
    {
        if (a == b)
            return 0.0;
        if (a == 0)
            return gd[groups[b - 1]][root];
        if (b == 0)
            return gd[groups[a - 1]][root];
        return gp[groups[a - 1]][groups[b - 1]];
    };

    if (k == 2)
        return Metric(0, 1);

    double best = 0.0;
    for (int anchor = 0; anchor < k; ++anchor)
    {
        double first = gst::fp::kInf;
        double second = gst::fp::kInf;
        for (int v = 0; v < k; ++v)
        {
            if (v == anchor)
                continue;
            const double w = Metric(anchor, v);
            if (w < first)
            {
                second = first;
                first = w;
            }
            else if (w < second)
            {
                second = w;
            }
        }

        std::vector<double> d(k, gst::fp::kInf);
        std::vector<char> used(k, 0);
        int start = (anchor == 0) ? 1 : 0;
        d[start] = 0.0;
        double mst = 0.0;
        for (int it = 0; it < k - 1; ++it)
        {
            int u = -1;
            for (int v = 0; v < k; ++v)
                if (v != anchor && !used[v] && (u < 0 || d[v] < d[u]))
                    u = v;
            used[u] = 1;
            mst += d[u];
            for (int v = 0; v < k; ++v)
            {
                if (v != anchor && !used[v])
                    d[v] = std::min(d[v], Metric(u, v));
            }
        }
        best = std::max(best, (mst + first + second) * 0.5);
    }
    return best;
}

double OneTreeAllAnchorHalfKruskal(int root,
                                   int mask,
                                   const std::vector<int>& first_bit,
                                   const std::vector<std::vector<double>>& gd,
                                   const std::vector<std::vector<double>>& gp)
{
    std::vector<int> groups;
    for (int t = mask; t; t &= t - 1)
        groups.push_back(first_bit[t]);
    const int k = static_cast<int>(groups.size());
    if (k <= 0)
        return 0.0;
    if (k == 1)
        return gd[groups[0]][root];

    const int node_count = k + 1; // node 0 is root; nodes 1..k are groups.
    std::vector<MetricEdge> group_edges;
    group_edges.reserve(k * (k - 1) / 2);
    for (int i = 0; i < k; ++i)
        for (int j = i + 1; j < k; ++j)
            group_edges.push_back({i + 1, j + 1, gp[groups[i]][groups[j]]});
    std::sort(group_edges.begin(), group_edges.end(), [](const MetricEdge& a, const MetricEdge& b)
    {
        if (a.w != b.w)
            return a.w < b.w;
        if (a.u != b.u)
            return a.u < b.u;
        return a.v < b.v;
    });

    std::vector<MetricEdge> root_edges;
    std::vector<double> root_w(k, gst::fp::kInf);
    root_edges.reserve(k);
    for (int i = 0; i < k; ++i)
    {
        root_w[i] = gd[groups[i]][root];
        root_edges.push_back({0, i + 1, gd[groups[i]][root]});
    }
    std::sort(root_edges.begin(), root_edges.end(), [](const MetricEdge& a, const MetricEdge& b)
    {
        if (a.w != b.w)
            return a.w < b.w;
        return a.v < b.v;
    });

    auto Incident = [&](int a, const MetricEdge& e)
    {
        return e.u == a || e.v == a;
    };

    auto EdgeWeight = [&](int a, int b)
    {
        if (a == 0)
            return root_w[b - 1];
        if (b == 0)
            return root_w[a - 1];
        return gp[groups[a - 1]][groups[b - 1]];
    };

    double best = 0.0;
    for (int anchor = 0; anchor < node_count; ++anchor)
    {
        double first = gst::fp::kInf;
        double second = gst::fp::kInf;
        for (int v = 0; v < node_count; ++v)
        {
            if (v == anchor)
                continue;
            const double w = EdgeWeight(anchor, v);
            if (w < first)
            {
                second = first;
                first = w;
            }
            else if (w < second)
            {
                second = w;
            }
        }

        SmallDsu dsu(node_count);
        const int target_edges = node_count - 2;
        int used = 0;
        double mst = 0.0;
        size_t gi = 0, ri = 0;
        while (used < target_edges)
        {
            while (gi < group_edges.size() && Incident(anchor, group_edges[gi]))
                ++gi;
            while (ri < root_edges.size() && Incident(anchor, root_edges[ri]))
                ++ri;
            const bool take_group =
                gi < group_edges.size() &&
                (ri >= root_edges.size() || group_edges[gi].w <= root_edges[ri].w);
            const MetricEdge edge = take_group ? group_edges[gi++] : root_edges[ri++];
            if (dsu.Union(edge.u, edge.v))
            {
                mst += edge.w;
                ++used;
            }
        }
        best = std::max(best, (mst + first + second) * 0.5);
    }
    return best;
}

double AnchorTwoEdgeHalf(int root,
                         int mask,
                         const std::vector<int>& first_bit,
                         const std::vector<std::vector<double>>& gd,
                         const std::vector<std::vector<double>>& gp)
{
    std::vector<int> groups;
    for (int t = mask; t; t &= t - 1)
        groups.push_back(first_bit[t]);
    const int k = static_cast<int>(groups.size()) + 1;
    if (k <= 1)
        return 0.0;

    auto Metric = [&](int a, int b)
    {
        if (a == b)
            return 0.0;
        if (a == 0)
            return gd[groups[b - 1]][root];
        if (b == 0)
            return gd[groups[a - 1]][root];
        return gp[groups[a - 1]][groups[b - 1]];
    };

    double best = 0.0;
    for (int anchor = 0; anchor < k; ++anchor)
    {
        double first = gst::fp::kInf;
        double second = gst::fp::kInf;
        for (int v = 0; v < k; ++v)
        {
            if (v == anchor)
                continue;
            const double w = Metric(anchor, v);
            if (w < first)
            {
                second = first;
                first = w;
            }
            else if (w < second)
            {
                second = w;
            }
        }
        if (second >= gst::fp::kInf / 4)
            best = std::max(best, first);
        else
            best = std::max(best, (first + second) * 0.5);
    }
    return best;
}

double ExactMetricTspTourHalf(int root,
                              int mask,
                              const std::vector<int>& first_bit,
                              const std::vector<std::vector<double>>& gd,
                              const std::vector<std::vector<double>>& gp)
{
    std::vector<int> groups;
    for (int t = mask; t; t &= t - 1)
        groups.push_back(first_bit[t]);
    const int k = static_cast<int>(groups.size());
    if (k <= 0)
        return 0.0;
    if (k == 1)
        return gd[groups[0]][root];

    const int full = (1 << k) - 1;
    std::vector<double> dp((full + 1) * k, gst::fp::kInf);
    auto At = [&](int s, int i) -> double&
    {
        return dp[s * k + i];
    };

    for (int i = 0; i < k; ++i)
        At(1 << i, i) = gd[groups[i]][root];

    for (int s = 1; s <= full; ++s)
    {
        for (int i = 0; i < k; ++i)
        {
            if (!(s >> i & 1) || At(s, i) >= gst::fp::kInf / 4)
                continue;
            const double cur = At(s, i);
            const int rem = full ^ s;
            for (int t = rem; t; t &= t - 1)
            {
                const int bit = t & -t;
                const int j = first_bit[bit];
                At(s | bit, j) = std::min(At(s | bit, j), cur + gp[groups[i]][groups[j]]);
            }
        }
    }

    double tour = gst::fp::kInf;
    for (int i = 0; i < k; ++i)
        tour = std::min(tour, At(full, i) + gd[groups[i]][root]);
    return tour * 0.5;
}

double CurrentLowerBound(int root,
                         int mask,
                         const std::vector<int>& pc,
                         const std::vector<int>& first_bit,
                         const std::vector<std::vector<double>>& gd,
                         const std::vector<std::vector<double>>& gp)
{
    if (!mask)
        return 0.0;
    double far = 0.0;
    double x = gst::fp::kInf;
    double y = gst::fp::kInf;
    for (int t = mask; t; t &= t - 1)
    {
        double z = gd[first_bit[t]][root];
        far = std::max(far, z);
        if (z < x)
        {
            y = x;
            x = z;
        }
        else if (z < y)
        {
            y = z;
        }
    }
    if (!(mask & (mask - 1)))
        return far;
    return std::max(far, GroupMstHalf(mask, pc, first_bit, gp) + (x + y) * 0.5);
}

double FarPlusNear(int root,
                   int mask,
                   const std::vector<int>& first_bit,
                   const std::vector<std::vector<double>>& gd)
{
    if (!mask)
        return 0.0;
    double far = 0.0;
    double near = gst::fp::kInf;
    for (int t = mask; t; t &= t - 1)
    {
        double z = gd[first_bit[t]][root];
        far = std::max(far, z);
        near = std::min(near, z);
    }
    return far + near;
}

double ExactFutureOpt(const Graph& graph, const Query& query, int root, int mask, const std::vector<int>& first_bit)
{
    Query future;
    future.groups.push_back({root});
    for (int t = mask; t; t &= t - 1)
        future.groups.push_back(query.groups[first_bit[t]]);
    auto result = gst::methods::dpbf::SolveOneQuery(graph, future);
    return result.feasible ? result.best_weight : gst::fp::kInf;
}

double ExactSpliceOpt(const Graph& graph,
                      const Query& query,
                      int from,
                      int to,
                      int mask,
                      const std::vector<int>& first_bit)
{
    Query bridge;
    bridge.groups.push_back({from});
    bridge.groups.push_back({to});
    for (int t = mask; t; t &= t - 1)
        bridge.groups.push_back(query.groups[first_bit[t]]);
    auto result = gst::methods::dpbf::SolveOneQuery(graph, bridge);
    return result.feasible ? result.best_weight : gst::fp::kInf;
}

double BoundValue(int index,
                  int root,
                  int mask,
                  const std::vector<int>& pc,
                  const std::vector<int>& first_bit,
                  const std::vector<std::vector<double>>& gd,
                  const std::vector<std::vector<double>>& gp)
{
    switch (index)
    {
    case 0:
        return CurrentLowerBound(root, mask, pc, first_bit, gd, gp);
    case 1:
        return RootedMetricMstHalf(root, mask, first_bit, gd, gp);
    case 2:
        return GroupMstHalf(mask, pc, first_bit, gp);
    case 3:
        return FarPlusNear(root, mask, first_bit, gd);
    case 4:
        return std::max(CurrentLowerBound(root, mask, pc, first_bit, gd, gp),
                        RootedMetricMstHalf(root, mask, first_bit, gd, gp));
    case 5:
        return std::max(CurrentLowerBound(root, mask, pc, first_bit, gd, gp),
                        OneTreeAllAnchorHalf(root, mask, first_bit, gd, gp));
    case 6:
        return std::max(CurrentLowerBound(root, mask, pc, first_bit, gd, gp),
                        AnchorTwoEdgeHalf(root, mask, first_bit, gd, gp));
    case 7:
        return std::max(CurrentLowerBound(root, mask, pc, first_bit, gd, gp),
                        OneTreeAllAnchorHalfKruskal(root, mask, first_bit, gd, gp));
    case 8:
        return std::max(CurrentLowerBound(root, mask, pc, first_bit, gd, gp),
                        ExactMetricTspTourHalf(root, mask, first_bit, gd, gp));
    default:
        return 0.0;
    }
}

void AddBoundResult(BoundStats& stat, double bound, double current, double exact)
{
    ++stat.checked;
    if (exact > 0.0 && exact < gst::fp::kInf / 4)
    {
        ++stat.ratio_count;
        stat.sum_ratio += bound / exact;
        stat.sum_gap += exact - bound;
        stat.max_ratio = std::max(stat.max_ratio, bound / exact);
    }
    if (bound > exact + 1e-6)
    {
        ++stat.violations;
        stat.max_violation = std::max(stat.max_violation, bound - exact);
    }
    if (bound > current + 1e-9)
    {
        ++stat.stronger_than_current;
        stat.sum_stronger_gain += bound - current;
        stat.max_stronger_gain = std::max(stat.max_stronger_gain, bound - current);
        if (bound > exact + 1e-6)
            ++stat.unsafe_stronger_than_current;
    }
    if (bound > current + 1e-9 && bound <= exact + 1e-6)
    {
        ++stat.better_than_current;
        stat.sum_safe_gain += bound - current;
        stat.max_safe_gain = std::max(stat.max_safe_gain, bound - current);
    }
}

void AddConsistencyResult(BoundStats& stat, double from, double to, double edge_weight)
{
    ++stat.consistency_checks;
    const double overflow = from - (edge_weight + to);
    if (overflow > 1e-6)
    {
        ++stat.consistency_violations;
        stat.max_consistency_violation = std::max(stat.max_consistency_violation, overflow);
    }
}

void AddSpliceResult(BoundStats& stat, double larger, double smaller, double exact_bridge)
{
    ++stat.splice_checks;
    const double overflow = larger - (smaller + exact_bridge);
    if (overflow > 1e-6)
    {
        ++stat.splice_violations;
        stat.max_splice_violation = std::max(stat.max_splice_violation, overflow);
    }
}

void ProbeSpliceValidity(const Graph& graph,
                         const Query& query,
                         const std::vector<std::vector<double>>& gd,
                         const std::vector<std::vector<double>>& gp,
                         const std::vector<int>& pc,
                         const std::vector<int>& first_bit,
                         int full_mask,
                         int samples,
                         std::mt19937_64& rng,
                         ProbeStats& stats)
{
    if (samples <= 0)
        return;
    std::uniform_int_distribution<int> root_dist(1, graph.n);
    std::uniform_int_distribution<int> mask_dist(1, full_mask);
    for (int sample = 0; sample < samples; ++sample)
    {
        const int mask = mask_dist(rng);
        const int submask = static_cast<int>(rng() & static_cast<std::uint64_t>(mask));
        const int from = root_dist(rng);
        const int to = root_dist(rng);
        const double bridge = ExactSpliceOpt(graph, query, from, to, mask ^ submask, first_bit);
        for (int i = 0; i < static_cast<int>(stats.bounds.size()); ++i)
        {
            const double larger = BoundValue(i, from, mask, pc, first_bit, gd, gp);
            const double smaller = BoundValue(i, to, submask, pc, first_bit, gd, gp);
            AddSpliceResult(stats.bounds[i], larger, smaller, bridge);
        }
    }
}

void ProbeState(const Graph& graph,
                const Query& query,
                const std::vector<std::vector<double>>& gd,
                const std::vector<std::vector<double>>& gp,
                const std::vector<int>& pc,
                const std::vector<int>& first_bit,
                int root,
                int mask,
                ProbeStats& stats)
{
    const double exact = ExactFutureOpt(graph, query, root, mask, first_bit);
    if (exact >= gst::fp::kInf / 4)
    {
        ++stats.infeasible;
        return;
    }
    const double current = CurrentLowerBound(root, mask, pc, first_bit, gd, gp);
    ++stats.states;
    if (exact > 0.0)
    {
        ++stats.ratio_count;
        stats.current_sum_ratio += current / exact;
        stats.current_sum_gap += exact - current;
        stats.current_max_ratio = std::max(stats.current_max_ratio, current / exact);
    }

    AddBoundResult(stats.bounds[0], current, current, exact);
    for (int i = 1; i < static_cast<int>(stats.bounds.size()); ++i)
        AddBoundResult(stats.bounds[i], BoundValue(i, root, mask, pc, first_bit, gd, gp), current, exact);
}

void ProbeConsistency(const Graph& graph,
                      const std::vector<std::vector<double>>& gd,
                      const std::vector<std::vector<double>>& gp,
                      const std::vector<int>& pc,
                      const std::vector<int>& first_bit,
                      int full_mask,
                      ProbeStats& stats)
{
    std::vector<std::vector<double>> values(stats.bounds.size(),
                                            std::vector<double>((full_mask + 1) * (graph.n + 1), 0.0));
    auto At = [&](int bound_index, int mask, int root) -> double&
    {
        return values[bound_index][mask * (graph.n + 1) + root];
    };

    for (int mask = 1; mask <= full_mask; ++mask)
        for (int root = 1; root <= graph.n; ++root)
            for (int i = 0; i < static_cast<int>(stats.bounds.size()); ++i)
                At(i, mask, root) = BoundValue(i, root, mask, pc, first_bit, gd, gp);

    for (int mask = 1; mask <= full_mask; ++mask)
    {
        for (const auto& edge : graph.edges)
        {
            for (int i = 0; i < static_cast<int>(stats.bounds.size()); ++i)
            {
                AddConsistencyResult(stats.bounds[i], At(i, mask, edge.u), At(i, mask, edge.v), edge.w);
                AddConsistencyResult(stats.bounds[i], At(i, mask, edge.v), At(i, mask, edge.u), edge.w);
            }
        }
    }
}

int Usage()
{
    std::cerr
        << "Usage:\n"
        << "  gst_future_lb_probe [seed=1] [iterations=100] [min_n=4] [max_n=10]"
        << " [min_g=2] [max_g=7] [samples_per_instance=64] [splice_samples_per_instance=64]\n"
        << "samples_per_instance=0 runs exhaustive roots x nonempty masks per instance;"
        << " splice_samples_per_instance=0 disables subset-splice checks.\n";
    return 2;
}

Options ParseOptions(int argc, char** argv)
{
    Options opt;
    if (argc > 1)
        opt.seed = std::strtoull(argv[1], nullptr, 10);
    if (argc > 2)
        opt.iterations = std::atoi(argv[2]);
    if (argc > 3)
        opt.min_n = std::atoi(argv[3]);
    if (argc > 4)
        opt.max_n = std::atoi(argv[4]);
    if (argc > 5)
        opt.min_g = std::atoi(argv[5]);
    if (argc > 6)
        opt.max_g = std::atoi(argv[6]);
    if (argc > 7)
        opt.samples_per_instance = std::atoi(argv[7]);
    if (argc > 8)
        opt.splice_samples_per_instance = std::atoi(argv[8]);
    return opt;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc > 9)
        return Usage();
    Options opt = ParseOptions(argc, argv);
    if (opt.iterations < 0 || opt.min_n > opt.max_n || opt.min_g > opt.max_g ||
        opt.min_n < 1 || opt.min_g < 1 || opt.max_g >= 20 || opt.samples_per_instance < 0 ||
        opt.splice_samples_per_instance < 0)
        return Usage();

    std::mt19937_64 rng(opt.seed);
    std::uniform_int_distribution<int> n_dist(opt.min_n, opt.max_n);
    std::uniform_int_distribution<int> g_dist(opt.min_g, opt.max_g);

    ProbeStats stats;
    stats.bounds.push_back({"current_lb"});
    stats.bounds.push_back({"rooted_metric_mst_half"});
    stats.bounds.push_back({"group_mst_half"});
    stats.bounds.push_back({"far_plus_near_negative_control"});
    stats.bounds.push_back({"max_current_rooted_metric_mst_half"});
    stats.bounds.push_back({"max_current_all_anchor_one_tree_half"});
    stats.bounds.push_back({"max_current_anchor_two_edge_half"});
    stats.bounds.push_back({"max_current_all_anchor_one_tree_kruskal"});
    stats.bounds.push_back({"max_current_exact_metric_tsp_tour_half"});

    for (int it = 1; it <= opt.iterations; ++it)
    {
        const int n = n_dist(rng);
        const int g = g_dist(rng);
        Graph graph = RandomGraph(rng, n);
        Query query = RandomQuery(rng, n, g);
        const int full = (1 << g) - 1;
        std::vector<int> pc(1 << g, 0);
        std::vector<int> first_bit(1 << g, 0);
        for (int s = 1; s <= full; ++s)
        {
            pc[s] = pc[s >> 1] + (s & 1);
            first_bit[s] = (s & 1) ? 0 : first_bit[s >> 1] + 1;
        }
        auto gd = ComputeGroupDistances(graph, query);
        auto gp = ComputeGroupMetric(query, gd);
        ProbeConsistency(graph, gd, gp, pc, first_bit, full, stats);
        ProbeSpliceValidity(graph, query, gd, gp, pc, first_bit, full,
                            opt.splice_samples_per_instance, rng, stats);

        if (opt.samples_per_instance == 0)
        {
            for (int root = 1; root <= n; ++root)
                for (int mask = 1; mask <= full; ++mask)
                    ProbeState(graph, query, gd, gp, pc, first_bit, root, mask, stats);
        }
        else
        {
            std::uniform_int_distribution<int> root_dist(1, n);
            std::uniform_int_distribution<int> mask_dist(1, full);
            for (int sample = 0; sample < opt.samples_per_instance; ++sample)
                ProbeState(graph, query, gd, gp, pc, first_bit, root_dist(rng), mask_dist(rng), stats);
        }
    }

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "seed=" << opt.seed
              << " iterations=" << opt.iterations
              << " states=" << stats.states
              << " infeasible=" << stats.infeasible
              << " samples_per_instance=" << opt.samples_per_instance
              << " splice_samples_per_instance=" << opt.splice_samples_per_instance << '\n';
    if (stats.ratio_count)
    {
        std::cout << "current_ratio_count=" << stats.ratio_count
                  << " current_avg_ratio=" << (stats.current_sum_ratio / stats.ratio_count)
                  << " current_avg_gap=" << (stats.current_sum_gap / stats.ratio_count)
                  << " current_max_ratio=" << stats.current_max_ratio << '\n';
    }
    for (const auto& bound : stats.bounds)
    {
        const double denom = bound.ratio_count ? static_cast<double>(bound.ratio_count) : 1.0;
        const double stronger_denom = bound.stronger_than_current
                                          ? static_cast<double>(bound.stronger_than_current)
                                          : 1.0;
        const double safe_denom = bound.better_than_current
                                      ? static_cast<double>(bound.better_than_current)
                                      : 1.0;
        std::cout << "bound=" << bound.name
                  << " checked=" << bound.checked
                  << " ratio_count=" << bound.ratio_count
                  << " violations=" << bound.violations
                  << " max_violation=" << bound.max_violation
                  << " consistency_checks=" << bound.consistency_checks
                  << " consistency_violations=" << bound.consistency_violations
                  << " max_consistency_violation=" << bound.max_consistency_violation
                  << " splice_checks=" << bound.splice_checks
                  << " splice_violations=" << bound.splice_violations
                  << " max_splice_violation=" << bound.max_splice_violation
                  << " avg_ratio=" << (bound.sum_ratio / denom)
                  << " avg_gap=" << (bound.sum_gap / denom)
                  << " stronger_than_current=" << bound.stronger_than_current
                  << " unsafe_stronger_than_current=" << bound.unsafe_stronger_than_current
                  << " avg_stronger_gain=" << (bound.sum_stronger_gain / stronger_denom)
                  << " max_stronger_gain=" << bound.max_stronger_gain
                  << " better_than_current=" << bound.better_than_current
                  << " avg_safe_gain=" << (bound.sum_safe_gain / safe_denom)
                  << " max_safe_gain=" << bound.max_safe_gain
                  << " max_ratio=" << bound.max_ratio << '\n';
    }
    return 0;
}
