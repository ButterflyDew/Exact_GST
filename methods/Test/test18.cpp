#include "test18.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

#include "../../float_compare.h"
#include "../../memory_usage.h"
#include "../../query_feasibility.h"

namespace gst::methods::test18
{
namespace
{
struct State
{
    std::vector<int> v, cover;
    std::vector<double> d, need, dense;
    int count = 0;
    bool ready = false, light = false, dense_row = false;
};

long long LogCost(size_t x)
{
    long long r = 1;
    for (size_t p = 2; p < x; p <<= 1)
        ++r;
    return r;
}

bool ProgressLoggingEnabled()
{
    const char* value = std::getenv("GST_TEST18_PROGRESS");
    if (!value || !value[0])
        return false;
    std::string_view flag(value);
    return flag != "0" && flag != "false" && flag != "FALSE" && flag != "off" && flag != "OFF";
}

bool AstarOrderingEnabled()
{
    const char* value = std::getenv("GST_TEST18_ASTAR_ORDER");
    if (!value || !value[0])
        return false;
    std::string_view flag(value);
    return flag != "0" && flag != "false" && flag != "FALSE" && flag != "off" && flag != "OFF";
}

template <class Use>
void JoinRows(const std::vector<int>& av,
              const std::vector<double>& ad,
              const std::vector<int>& bv,
              const std::vector<double>& bd,
              Use&& use,
              long long& scans,
              long long& hits)
{
    const long long linear = static_cast<long long>(av.size() + bv.size());
    const long long abin = static_cast<long long>(av.size()) * LogCost(bv.size() + 1);
    const long long bbin = static_cast<long long>(bv.size()) * LogCost(av.size() + 1);

    if (!av.empty() && !bv.empty() && abin * 3 < linear * 2)
    {
        scans += abin;
        for (size_t i = 0; i < av.size(); ++i)
        {
            auto it = std::lower_bound(bv.begin(), bv.end(), av[i]);
            if (it != bv.end() && *it == av[i])
            {
                size_t j = it - bv.begin();
                use(av[i], ad[i], bd[j], i, j), ++hits;
            }
        }
        return;
    }
    if (!av.empty() && !bv.empty() && bbin * 3 < linear * 2)
    {
        scans += bbin;
        for (size_t j = 0; j < bv.size(); ++j)
        {
            auto it = std::lower_bound(av.begin(), av.end(), bv[j]);
            if (it != av.end() && *it == bv[j])
            {
                size_t i = it - av.begin();
                use(bv[j], ad[i], bd[j], i, j), ++hits;
            }
        }
        return;
    }

    scans += linear;
    size_t i = 0, j = 0;
    while (i < av.size() && j < bv.size())
    {
        if (av[i] < bv[j])
            ++i;
        else if (bv[j] < av[i])
            ++j;
        else
            use(av[i], ad[i], bd[j], i, j), ++hits, ++i, ++j;
    }
}

using HeapItem = std::pair<double, int>;

std::vector<std::vector<double>> ComputeGroupDistances(const Graph& graph, const Query& query)
{
    std::vector<std::vector<double>> gd(query.groups.size(), std::vector<double>(graph.n + 1, fp::kInf));
    std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> q;
    for (int a = 0; a < static_cast<int>(query.groups.size()); ++a)
    {
        while (!q.empty())
            q.pop();
        for (int v : query.groups[a])
            if (gd[a][v])
                gd[a][v] = 0, q.push({0, v});
        while (!q.empty())
        {
            auto [d, u] = q.top();
            q.pop();
            if (d != gd[a][u])
                continue;
            for (auto e : graph.adj[u])
                if (d + e.w < gd[a][e.to])
                    gd[a][e.to] = d + e.w, q.push({gd[a][e.to], e.to});
        }
    }
    return gd;
}

struct LeafReduction
{
    Graph graph;
    Query query;
    std::vector<std::vector<double>> gd;
    long long removed_vertices = 0;
    long long removed_edges = 0;
    long long removed_components = 0;
    long long two_portal_components = 0;
    long long two_portal_vertices = 0;
    long long two_portal_edges_added = 0;
    long long three_portal_components = 0;
    long long three_portal_vertices = 0;
    long long three_portal_hubs_added = 0;
    long long three_portal_edges_added = 0;
    long long four_portal_components = 0;
    long long four_portal_vertices = 0;
    long long four_portal_hubs_added = 0;
    long long four_portal_edges_added = 0;
};

struct ThreePortalHub
{
    std::array<int, 3> portals{};
    std::array<double, 3> weights{};
};

struct FourPortalHub
{
    std::array<int, 4> portals{};
    std::array<double, 4> weights{};
};

struct DegreeReduction
{
    Graph graph;
    Query query;
    long long removed_vertices = 0;
    long long removed_edges = 0;
    long long leaf_vertices = 0;
    long long dead_vertices = 0;
    long long contracted_vertices = 0;
};

void AddReducedEdge(Graph& graph, int u, int v, double w)
{
    if (u == v)
        return;
    UndirectedEdge edge;
    edge.id = static_cast<int>(graph.edges.size());
    edge.u = u;
    edge.v = v;
    edge.w = w;
    graph.edges.push_back(edge);
    graph.adj[u].push_back({v, edge.id, w});
    graph.adj[v].push_back({u, edge.id, w});
}

DegreeReduction ReduceSteinerLeavesAndChains(const Graph& graph, const Query& query)
{
    DegreeReduction result;
    const int n = graph.n;

    std::vector<char> protected_vertex(n + 1);
    for (const auto& group : query.groups)
        for (int v : group)
            protected_vertex[v] = 1;

    std::vector<char> active(n + 1, 1);
    std::vector<int> degree(n + 1, 0);
    for (const auto& e : graph.edges)
        ++degree[e.u], ++degree[e.v];

    std::queue<int> q;
    for (int v = 1; v <= n; ++v)
        if (!protected_vertex[v] && degree[v] <= 1)
            q.push(v);

    while (!q.empty())
    {
        int u = q.front();
        q.pop();
        if (!active[u] || protected_vertex[u] || degree[u] > 1)
            continue;
        active[u] = 0;
        ++result.leaf_vertices;
        for (auto e : graph.adj[u])
        {
            int v = e.to;
            if (!active[v])
                continue;
            --degree[v];
            if (!protected_vertex[v] && degree[v] <= 1)
                q.push(v);
        }
    }

    std::vector<char> seen_component(n + 1);
    std::vector<int> component;
    for (int start = 1; start <= n; ++start)
    {
        if (!active[start] || seen_component[start])
            continue;
        bool has_protected = false;
        component.clear();
        component.push_back(start);
        seen_component[start] = 1;
        for (size_t i = 0; i < component.size(); ++i)
        {
            int u = component[i];
            if (protected_vertex[u])
                has_protected = true;
            for (auto e : graph.adj[u])
            {
                int v = e.to;
                if (active[v] && !seen_component[v])
                {
                    seen_component[v] = 1;
                    component.push_back(v);
                }
            }
        }
        if (has_protected)
            continue;
        result.dead_vertices += static_cast<long long>(component.size());
        for (int v : component)
            active[v] = 0, degree[v] = 0;
    }

    std::vector<char> core(n + 1);
    for (int v = 1; v <= n; ++v)
        if (active[v] && (protected_vertex[v] || degree[v] != 2))
            core[v] = 1;

    std::vector<int> old_to_new(n + 1);
    int new_n = 0;
    for (int v = 1; v <= n; ++v)
        if (core[v])
            old_to_new[v] = ++new_n;

    if (new_n == n)
        return result;

    result.graph.n = new_n;
    result.graph.adj.assign(new_n + 1, {});
    std::vector<char> used_edge(graph.edges.size());

    auto TraceChain = [&](int start, int next, int first_edge, double first_weight)
    {
        int cur = next;
        int edge_id = first_edge;
        double total = first_weight;
        while (true)
        {
            used_edge[edge_id] = 1;
            if (core[cur])
            {
                AddReducedEdge(result.graph, old_to_new[start], old_to_new[cur], total);
                return;
            }
            int next_vertex = 0;
            int next_edge = -1;
            double next_weight = 0.0;
            for (auto e : graph.adj[cur])
            {
                if (!active[e.to] || e.edge_id == edge_id)
                    continue;
                next_vertex = e.to;
                next_edge = e.edge_id;
                next_weight = e.w;
                break;
            }
            if (next_edge < 0 || used_edge[next_edge])
                return;
            cur = next_vertex;
            edge_id = next_edge;
            total += next_weight;
        }
    };

    for (const auto& e : graph.edges)
    {
        if (!active[e.u] || !active[e.v])
            continue;
        if (core[e.u] && core[e.v])
        {
            AddReducedEdge(result.graph, old_to_new[e.u], old_to_new[e.v], e.w);
            continue;
        }
        if (used_edge[e.id])
            continue;
        if (core[e.u] && !core[e.v])
            TraceChain(e.u, e.v, e.id, e.w);
        else if (core[e.v] && !core[e.u])
            TraceChain(e.v, e.u, e.id, e.w);
    }

    result.graph.m = static_cast<int>(result.graph.edges.size());
    result.query.groups.resize(query.groups.size());
    for (int a = 0; a < static_cast<int>(query.groups.size()); ++a)
        for (int v : query.groups[a])
            result.query.groups[a].push_back(old_to_new[v]);

    result.removed_vertices = graph.n - result.graph.n;
    result.removed_edges = graph.m - result.graph.m;
    result.contracted_vertices = result.removed_vertices - result.leaf_vertices - result.dead_vertices;
    return result;
}

LeafReduction RemoveVoronoiLeafComponents(const Graph& graph,
                                          const Query& query,
                                          const std::vector<std::vector<double>>& gd)
{
    LeafReduction result;
    const int n = graph.n;
    const int g = static_cast<int>(query.groups.size());
    std::vector<int> color(n + 1);
    for (int a = 0; a < g; ++a)
        for (int v : query.groups[a])
            color[v] |= 1 << a;

    std::vector<int> owner(n + 1, -1);
    for (int v = 1; v <= n; ++v)
    {
        int who = 0;
        for (int a = 1; a < g; ++a)
            if (gd[a][v] < gd[who][v])
                who = a;
        owner[v] = who;
    }

    std::vector<char> boundary(n + 1);
    for (const auto& e : graph.edges)
        if (owner[e.u] != owner[e.v])
            boundary[e.u] = 1, boundary[e.v] = 1;

    std::vector<char> seen(n + 1), remove(n + 1);
    std::vector<int> queue, portals_seen;
    std::vector<int> portal_mark(n + 1);
    int portal_stamp = 0;
    std::vector<int> local_mark(n + 1);
    std::vector<double> local_dist(n + 1, fp::kInf);
    int local_stamp = 0;
    std::vector<std::tuple<int, int, double>> two_portal_edges;
    std::vector<std::tuple<int, int, double>> three_portal_pair_edges;
    std::vector<ThreePortalHub> three_portal_hubs;
    std::vector<std::tuple<int, int, double>> four_portal_pair_edges;
    std::vector<ThreePortalHub> four_portal_triple_hubs;
    std::vector<FourPortalHub> four_portal_hubs;

    auto ComponentPortalDistance = [&](const std::vector<int>& component, int source, int target)
    {
        ++local_stamp;
        local_mark[source] = local_stamp;
        local_mark[target] = local_stamp;
        for (int v : component)
            local_mark[v] = local_stamp;

        std::vector<int> touched;
        std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
        local_dist[source] = 0.0;
        touched.push_back(source);
        heap.push({0.0, source});
        while (!heap.empty())
        {
            auto [d, u] = heap.top();
            heap.pop();
            if (d != local_dist[u])
                continue;
            if (u == target)
                break;
            for (auto e : graph.adj[u])
            {
                if (local_mark[e.to] != local_stamp)
                    continue;
                if ((u == source && e.to == target) || (u == target && e.to == source))
                    continue;
                double nd = d + e.w;
                if (nd < local_dist[e.to])
                {
                    if (local_dist[e.to] == fp::kInf)
                        touched.push_back(e.to);
                    local_dist[e.to] = nd;
                    heap.push({nd, e.to});
                }
            }
        }
        double answer = local_dist[target];
        for (int v : touched)
            local_dist[v] = fp::kInf;
        return answer;
    };

    auto ComponentPortalDistances = [&](const std::vector<int>& component,
                                        const std::vector<int>& portals,
                                        const std::vector<int>& local_nodes,
                                        int source)
    {
        ++local_stamp;
        for (int v : portals)
            local_mark[v] = local_stamp;
        for (int v : component)
            local_mark[v] = local_stamp;

        std::vector<int> touched;
        std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
        local_dist[source] = 0.0;
        touched.push_back(source);
        heap.push({0.0, source});
        while (!heap.empty())
        {
            auto [d, u] = heap.top();
            heap.pop();
            if (d != local_dist[u])
                continue;
            for (auto e : graph.adj[u])
            {
                if (local_mark[e.to] != local_stamp)
                    continue;
                if (boundary[u] && boundary[e.to])
                    continue;
                double nd = d + e.w;
                if (nd < local_dist[e.to])
                {
                    if (local_dist[e.to] == fp::kInf)
                        touched.push_back(e.to);
                    local_dist[e.to] = nd;
                    heap.push({nd, e.to});
                }
            }
        }

        std::vector<double> answer;
        answer.reserve(local_nodes.size());
        for (int v : local_nodes)
            answer.push_back(local_dist[v]);
        for (int v : touched)
            local_dist[v] = fp::kInf;
        return answer;
    };

    // Pair edges preserve every two-portal use; this hub preserves the exact
    // three-portal Steiner cost without making any pair connection cheaper.
    auto BuildThreePortalWeights = [](double d01, double d02, double d12, double triple,
                                      std::array<double, 3>& weights)
    {
        const double eps = 1e-8;
        if (d01 >= fp::kInf || d02 >= fp::kInf || d12 >= fp::kInf || triple >= fp::kInf)
            return false;
        if (triple + eps < d01 || triple + eps < d02 || triple + eps < d12)
            return false;

        double w0 = 0.5 * (d01 + d02 - d12);
        double w1 = 0.5 * (d01 + d12 - d02);
        double w2 = 0.5 * (d02 + d12 - d01);
        if (w0 < -eps || w1 < -eps || w2 < -eps)
            return false;
        w0 = std::max(0.0, w0);
        w1 = std::max(0.0, w1);
        w2 = std::max(0.0, w2);

        double extra = triple - (w0 + w1 + w2);
        if (extra < -eps)
            return false;
        if (extra > 0.0)
            w0 += extra;

        if (w0 + w1 + eps < d01 || w0 + w2 + eps < d02 || w1 + w2 + eps < d12)
            return false;
        weights = {w0, w1, w2};
        return true;
    };

    auto SmallPopcount = [](int mask)
    {
        int count = 0;
        while (mask)
        {
            mask &= mask - 1;
            ++count;
        }
        return count;
    };

    auto Solve3x3 = [](std::array<std::array<double, 3>, 3> a, std::array<double, 3> b,
                       std::array<double, 3>& x)
    {
        const double eps = 1e-9;
        for (int col = 0; col < 3; ++col)
        {
            int pivot = col;
            for (int row = col + 1; row < 3; ++row)
                if (std::fabs(a[row][col]) > std::fabs(a[pivot][col]))
                    pivot = row;
            if (std::fabs(a[pivot][col]) < eps)
                return false;
            if (pivot != col)
            {
                std::swap(a[pivot], a[col]);
                std::swap(b[pivot], b[col]);
            }
            const double div = a[col][col];
            for (int j = col; j < 3; ++j)
                a[col][j] /= div;
            b[col] /= div;
            for (int row = 0; row < 3; ++row)
            {
                if (row == col)
                    continue;
                const double factor = a[row][col];
                for (int j = col; j < 3; ++j)
                    a[row][j] -= factor * a[col][j];
                b[row] -= factor * b[col];
            }
        }
        x = b;
        return true;
    };

    auto BuildFourPortalWeights = [&](const std::array<double, 16>& cost,
                                      std::array<double, 4>& weights)
    {
        const double eps = 1e-8;
        const double quad = cost[15];
        if (quad >= fp::kInf)
            return false;

        std::vector<std::array<double, 4>> inequalities;
        auto AddInequality = [&](double a0, double a1, double a2, double rhs)
        {
            inequalities.push_back({a0, a1, a2, rhs});
        };

        AddInequality(-1.0, 0.0, 0.0, 0.0);
        AddInequality(0.0, -1.0, 0.0, 0.0);
        AddInequality(0.0, 0.0, -1.0, 0.0);
        AddInequality(1.0, 1.0, 1.0, quad);

        for (int mask = 1; mask < 15; ++mask)
        {
            const int pc = SmallPopcount(mask);
            if (pc < 2 || pc > 3 || cost[mask] >= fp::kInf)
                continue;
            if (mask & 8)
            {
                double coeff[3] = {0.0, 0.0, 0.0};
                for (int i = 0; i < 3; ++i)
                    if (!(mask >> i & 1))
                        coeff[i] = 1.0;
                AddInequality(coeff[0], coeff[1], coeff[2], quad - cost[mask]);
            }
            else
            {
                double coeff[3] = {0.0, 0.0, 0.0};
                for (int i = 0; i < 3; ++i)
                    if (mask >> i & 1)
                        coeff[i] = -1.0;
                AddInequality(coeff[0], coeff[1], coeff[2], -cost[mask]);
            }
        }

        auto FeasiblePoint = [&](const std::array<double, 3>& x)
        {
            for (const auto& ineq : inequalities)
            {
                const double lhs = ineq[0] * x[0] + ineq[1] * x[1] + ineq[2] * x[2];
                if (lhs > ineq[3] + eps)
                    return false;
            }
            return true;
        };

        for (int i = 0; i < static_cast<int>(inequalities.size()); ++i)
            for (int j = i + 1; j < static_cast<int>(inequalities.size()); ++j)
                for (int k = j + 1; k < static_cast<int>(inequalities.size()); ++k)
                {
                    std::array<std::array<double, 3>, 3> a = {
                        std::array<double, 3>{inequalities[i][0], inequalities[i][1], inequalities[i][2]},
                        std::array<double, 3>{inequalities[j][0], inequalities[j][1], inequalities[j][2]},
                        std::array<double, 3>{inequalities[k][0], inequalities[k][1], inequalities[k][2]}};
                    std::array<double, 3> b = {inequalities[i][3], inequalities[j][3], inequalities[k][3]};
                    std::array<double, 3> x{};
                    if (Solve3x3(a, b, x) && FeasiblePoint(x))
                    {
                        const double w3 = quad - x[0] - x[1] - x[2];
                        if (w3 < -eps)
                            continue;
                        weights = {std::max(0.0, x[0]), std::max(0.0, x[1]),
                                   std::max(0.0, x[2]), std::max(0.0, w3)};
                        return true;
                    }
                }
        return false;
    };

    std::vector<int> local_index(n + 1, -1);
    auto ComputeFourPortalCosts = [&](const std::vector<int>& component,
                                      const std::vector<int>& portals)
    {
        std::vector<int> local_nodes = portals;
        local_nodes.insert(local_nodes.end(), component.begin(), component.end());
        for (int i = 0; i < static_cast<int>(local_nodes.size()); ++i)
            local_index[local_nodes[i]] = i;

        const int local_n = static_cast<int>(local_nodes.size());
        std::vector<std::vector<double>> dist(16, std::vector<double>(local_n, fp::kInf));
        for (int i = 0; i < 4; ++i)
            dist[1 << i][i] = 0.0;

        for (int mask = 1; mask < 16; ++mask)
        {
            for (int sub = (mask - 1) & mask; sub; sub = (sub - 1) & mask)
            {
                const int other = mask ^ sub;
                if (!other || sub > other)
                    continue;
                for (int i = 0; i < local_n; ++i)
                {
                    const double merged = dist[sub][i] + dist[other][i];
                    if (merged < dist[mask][i])
                        dist[mask][i] = merged;
                }
            }

            std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
            for (int i = 0; i < local_n; ++i)
                if (dist[mask][i] < fp::kInf)
                    heap.push({dist[mask][i], i});
            while (!heap.empty())
            {
                auto [d, ui] = heap.top();
                heap.pop();
                if (d != dist[mask][ui])
                    continue;
                const int u = local_nodes[ui];
                for (auto e : graph.adj[u])
                {
                    const int vi = local_index[e.to];
                    if (vi < 0)
                        continue;
                    if (boundary[u] && boundary[e.to])
                        continue;
                    const double nd = d + e.w;
                    if (nd < dist[mask][vi])
                    {
                        dist[mask][vi] = nd;
                        heap.push({nd, vi});
                    }
                }
            }
        }

        std::array<double, 16> cost{};
        cost.fill(fp::kInf);
        cost[0] = 0.0;
        for (int mask = 1; mask < 16; ++mask)
            for (int i = 0; i < local_n; ++i)
                cost[mask] = std::min(cost[mask], dist[mask][i]);

        for (int v : local_nodes)
            local_index[v] = -1;
        return cost;
    };

    for (int start = 1; start <= n; ++start)
    {
        if (boundary[start] || seen[start])
            continue;
        int own = owner[start];
        bool has_terminal = false;
        int portals = 0;
        ++portal_stamp;
        queue.clear();
        portals_seen.clear();
        queue.push_back(start);
        seen[start] = 1;
        for (size_t qi = 0; qi < queue.size(); ++qi)
        {
            int u = queue[qi];
            if (color[u])
                has_terminal = true;
            for (auto e : graph.adj[u])
            {
                int v = e.to;
                if (boundary[v])
                {
                    if (portal_mark[v] != portal_stamp)
                    {
                        portal_mark[v] = portal_stamp;
                        portals_seen.push_back(v);
                        ++portals;
                    }
                    continue;
                }
                if (!seen[v] && owner[v] == own)
                {
                    seen[v] = 1;
                    queue.push_back(v);
                }
            }
        }
        if (!has_terminal && portals <= 1)
        {
            ++result.removed_components;
            result.removed_vertices += static_cast<long long>(queue.size());
            for (int v : queue)
                remove[v] = 1;
        }
        else if (!has_terminal && portals == 2)
        {
            double bridge = ComponentPortalDistance(queue, portals_seen[0], portals_seen[1]);
            if (bridge < fp::kInf)
            {
                ++result.removed_components;
                ++result.two_portal_components;
                result.removed_vertices += static_cast<long long>(queue.size());
                result.two_portal_vertices += static_cast<long long>(queue.size());
                two_portal_edges.push_back({portals_seen[0], portals_seen[1], bridge});
                for (int v : queue)
                    remove[v] = 1;
            }
        }
        else if (!has_terminal && portals == 3 && queue.size() > 1)
        {
            std::vector<int> local_nodes = portals_seen;
            local_nodes.insert(local_nodes.end(), queue.begin(), queue.end());
            std::vector<double> d0 =
                ComponentPortalDistances(queue, portals_seen, local_nodes, portals_seen[0]);
            std::vector<double> d1 =
                ComponentPortalDistances(queue, portals_seen, local_nodes, portals_seen[1]);
            std::vector<double> d2 =
                ComponentPortalDistances(queue, portals_seen, local_nodes, portals_seen[2]);

            const double d01 = d0[1], d02 = d0[2], d12 = d1[2];
            double triple = fp::kInf;
            for (size_t i = 0; i < local_nodes.size(); ++i)
                if (d0[i] < fp::kInf && d1[i] < fp::kInf && d2[i] < fp::kInf)
                    triple = std::min(triple, d0[i] + d1[i] + d2[i]);

            ThreePortalHub hub;
            hub.portals = {portals_seen[0], portals_seen[1], portals_seen[2]};
            if (BuildThreePortalWeights(d01, d02, d12, triple, hub.weights))
            {
                ++result.removed_components;
                ++result.three_portal_components;
                result.removed_vertices += static_cast<long long>(queue.size());
                result.three_portal_vertices += static_cast<long long>(queue.size());
                three_portal_pair_edges.push_back({portals_seen[0], portals_seen[1], d01});
                three_portal_pair_edges.push_back({portals_seen[0], portals_seen[2], d02});
                three_portal_pair_edges.push_back({portals_seen[1], portals_seen[2], d12});
                three_portal_hubs.push_back(hub);
                for (int v : queue)
                    remove[v] = 1;
            }
        }
        // The four-portal gadget adds exactly five hubs; require a strict vertex-count win.
        else if (!has_terminal && portals == 4 && queue.size() > 5)
        {
            const std::array<double, 16> cost = ComputeFourPortalCosts(queue, portals_seen);
            FourPortalHub quad_hub;
            quad_hub.portals = {portals_seen[0], portals_seen[1], portals_seen[2], portals_seen[3]};
            std::vector<ThreePortalHub> triple_hubs;
            bool ok = BuildFourPortalWeights(cost, quad_hub.weights);

            const std::array<std::array<int, 3>, 4> triples = {
                std::array<int, 3>{0, 1, 2},
                std::array<int, 3>{0, 1, 3},
                std::array<int, 3>{0, 2, 3},
                std::array<int, 3>{1, 2, 3}};
            for (const auto& tri : triples)
            {
                if (!ok)
                    break;
                const int a = tri[0], b = tri[1], c = tri[2];
                ThreePortalHub hub;
                hub.portals = {portals_seen[a], portals_seen[b], portals_seen[c]};
                const double dab = cost[(1 << a) | (1 << b)];
                const double dac = cost[(1 << a) | (1 << c)];
                const double dbc = cost[(1 << b) | (1 << c)];
                const double triple = cost[(1 << a) | (1 << b) | (1 << c)];
                ok = BuildThreePortalWeights(dab, dac, dbc, triple, hub.weights);
                if (ok)
                    triple_hubs.push_back(hub);
            }

            if (ok)
            {
                ++result.removed_components;
                ++result.four_portal_components;
                result.removed_vertices += static_cast<long long>(queue.size());
                result.four_portal_vertices += static_cast<long long>(queue.size());
                for (int i = 0; i < 4; ++i)
                    for (int j = i + 1; j < 4; ++j)
                        four_portal_pair_edges.push_back(
                            {portals_seen[i], portals_seen[j], cost[(1 << i) | (1 << j)]});
                four_portal_triple_hubs.insert(four_portal_triple_hubs.end(),
                                               triple_hubs.begin(), triple_hubs.end());
                four_portal_hubs.push_back(quad_hub);
                for (int v : queue)
                    remove[v] = 1;
            }
        }
    }

    if (!result.removed_vertices)
        return result;

    std::vector<int> old_to_new(n + 1);
    int new_n = 0;
    for (int v = 1; v <= n; ++v)
        if (!remove[v])
            old_to_new[v] = ++new_n;
    const int first_hub = new_n + 1;
    const int total_hubs = static_cast<int>(three_portal_hubs.size() +
                                            four_portal_triple_hubs.size() +
                                            four_portal_hubs.size());
    new_n += total_hubs;

    result.graph.n = new_n;
    result.graph.adj.assign(new_n + 1, {});
    for (const auto& e : graph.edges)
    {
        if (remove[e.u] || remove[e.v])
            continue;
        UndirectedEdge edge;
        edge.id = static_cast<int>(result.graph.edges.size());
        edge.u = old_to_new[e.u];
        edge.v = old_to_new[e.v];
        edge.w = e.w;
        result.graph.edges.push_back(edge);
        result.graph.adj[edge.u].push_back({edge.v, edge.id, edge.w});
        result.graph.adj[edge.v].push_back({edge.u, edge.id, edge.w});
    }
    for (auto [u, v, w] : two_portal_edges)
    {
        AddReducedEdge(result.graph, old_to_new[u], old_to_new[v], w);
        ++result.two_portal_edges_added;
    }
    for (auto [u, v, w] : three_portal_pair_edges)
    {
        AddReducedEdge(result.graph, old_to_new[u], old_to_new[v], w);
        ++result.three_portal_edges_added;
    }
    for (auto [u, v, w] : four_portal_pair_edges)
    {
        AddReducedEdge(result.graph, old_to_new[u], old_to_new[v], w);
        ++result.four_portal_edges_added;
    }
    int next_hub = first_hub;
    for (int i = 0; i < static_cast<int>(three_portal_hubs.size()); ++i)
    {
        const int hub_id = next_hub++;
        const ThreePortalHub& hub = three_portal_hubs[i];
        for (int j = 0; j < 3; ++j)
        {
            AddReducedEdge(result.graph, hub_id, old_to_new[hub.portals[j]], hub.weights[j]);
            ++result.three_portal_edges_added;
        }
        ++result.three_portal_hubs_added;
    }
    for (int i = 0; i < static_cast<int>(four_portal_triple_hubs.size()); ++i)
    {
        const int hub_id = next_hub++;
        const ThreePortalHub& hub = four_portal_triple_hubs[i];
        for (int j = 0; j < 3; ++j)
        {
            AddReducedEdge(result.graph, hub_id, old_to_new[hub.portals[j]], hub.weights[j]);
            ++result.four_portal_edges_added;
        }
        ++result.four_portal_hubs_added;
    }
    for (int i = 0; i < static_cast<int>(four_portal_hubs.size()); ++i)
    {
        const int hub_id = next_hub++;
        const FourPortalHub& hub = four_portal_hubs[i];
        for (int j = 0; j < 4; ++j)
        {
            AddReducedEdge(result.graph, hub_id, old_to_new[hub.portals[j]], hub.weights[j]);
            ++result.four_portal_edges_added;
        }
        ++result.four_portal_hubs_added;
    }
    result.graph.m = static_cast<int>(result.graph.edges.size());
    result.removed_edges = graph.m - result.graph.m;

    result.query.groups.resize(g);
    for (int a = 0; a < g; ++a)
        for (int v : query.groups[a])
            result.query.groups[a].push_back(old_to_new[v]);

    result.gd.assign(g, std::vector<double>(new_n + 1, fp::kInf));
    for (int a = 0; a < g; ++a)
    {
        for (int v = 1; v <= n; ++v)
            if (!remove[v])
                result.gd[a][old_to_new[v]] = gd[a][v];
        int next_gd_hub = first_hub;
        for (int i = 0; i < static_cast<int>(three_portal_hubs.size()); ++i)
        {
            const int hub_id = next_gd_hub++;
            const ThreePortalHub& hub = three_portal_hubs[i];
            for (int j = 0; j < 3; ++j)
                result.gd[a][hub_id] =
                    std::min(result.gd[a][hub_id], gd[a][hub.portals[j]] + hub.weights[j]);
        }
        for (int i = 0; i < static_cast<int>(four_portal_triple_hubs.size()); ++i)
        {
            const int hub_id = next_gd_hub++;
            const ThreePortalHub& hub = four_portal_triple_hubs[i];
            for (int j = 0; j < 3; ++j)
                result.gd[a][hub_id] =
                    std::min(result.gd[a][hub_id], gd[a][hub.portals[j]] + hub.weights[j]);
        }
        for (int i = 0; i < static_cast<int>(four_portal_hubs.size()); ++i)
        {
            const int hub_id = next_gd_hub++;
            const FourPortalHub& hub = four_portal_hubs[i];
            for (int j = 0; j < 4; ++j)
                result.gd[a][hub_id] =
                    std::min(result.gd[a][hub_id], gd[a][hub.portals[j]] + hub.weights[j]);
        }
    }

    return result;
}
}  // namespace

SolveResult SolveOneQuery(const Graph& input_graph, const Query& input_query)
{
    using Clock = std::chrono::steady_clock;
    using P = std::pair<double, int>;
    using Heap = std::priority_queue<P, std::vector<P>, std::greater<P>>;

    SolveResult res;
    auto& st = res.stats;
    const int g = static_cast<int>(input_query.groups.size());
    st.original_n = input_graph.n, st.original_m = input_graph.m, st.g = g;
    if (!g)
        return {0.0, true, {}};
    if (g > 22)
        throw std::runtime_error("Test18 supports group count <= 22.");
    if (!IsQueryFeasible(input_graph, input_query))
        return res;

    const auto solve_start = Clock::now();
    const auto degree_reduce_start = Clock::now();
    DegreeReduction degree_reduction = ReduceSteinerLeavesAndChains(input_graph, input_query);
    st.degree_reduce_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - degree_reduce_start).count();
    st.degree_reduce_removed_vertices = degree_reduction.removed_vertices;
    st.degree_reduce_removed_edges = degree_reduction.removed_edges;
    st.degree_reduce_leaf_vertices = degree_reduction.leaf_vertices;
    st.degree_reduce_dead_vertices = degree_reduction.dead_vertices;
    st.degree_reduce_contracted_vertices = degree_reduction.contracted_vertices;
    const Graph& degree_graph =
        degree_reduction.removed_vertices ? degree_reduction.graph : input_graph;
    const Query& degree_query =
        degree_reduction.removed_vertices ? degree_reduction.query : input_query;

    const auto gd_start = Clock::now();
    std::vector<std::vector<double>> gd = ComputeGroupDistances(degree_graph, degree_query);
    st.group_dist_ms = std::chrono::duration<double, std::milli>(Clock::now() - gd_start).count();

    const auto reduce_start = Clock::now();
    LeafReduction reduction = RemoveVoronoiLeafComponents(degree_graph, degree_query, gd);
    st.leaf_reduce_ms = std::chrono::duration<double, std::milli>(Clock::now() - reduce_start).count();
    st.leaf_reduce_removed_vertices = reduction.removed_vertices;
    st.leaf_reduce_removed_edges = reduction.removed_edges;
    st.leaf_reduce_removed_components = reduction.removed_components;
    st.leaf_reduce_two_portal_components = reduction.two_portal_components;
    st.leaf_reduce_two_portal_vertices = reduction.two_portal_vertices;
    st.leaf_reduce_two_portal_edges_added = reduction.two_portal_edges_added;
    st.leaf_reduce_three_portal_components = reduction.three_portal_components;
    st.leaf_reduce_three_portal_vertices = reduction.three_portal_vertices;
    st.leaf_reduce_three_portal_hubs_added = reduction.three_portal_hubs_added;
    st.leaf_reduce_three_portal_edges_added = reduction.three_portal_edges_added;
    st.leaf_reduce_four_portal_components = reduction.four_portal_components;
    st.leaf_reduce_four_portal_vertices = reduction.four_portal_vertices;
    st.leaf_reduce_four_portal_hubs_added = reduction.four_portal_hubs_added;
    st.leaf_reduce_four_portal_edges_added = reduction.four_portal_edges_added;
    const bool uses_leaf_graph = reduction.removed_vertices;
    const Graph& graph = uses_leaf_graph ? reduction.graph : degree_graph;
    const Query& query = uses_leaf_graph ? reduction.query : degree_query;
    if (reduction.removed_vertices)
    {
        gd = std::move(reduction.gd);
        degree_reduction.graph = Graph{};
        degree_reduction.query = Query{};
    }

    const int n = graph.n;
    st.n = n, st.m = graph.m;
    const int H = g / 2, S = 1 << g, U = S - 1, N = n + 1;
    std::vector<int> pc(S), first_bit(S), order;
    st.total_by_size.assign(H + 1, 0);
    st.active_by_size.assign(H + 1, 0);
    st.inqueue_by_size.assign(H + 1, 0);
    st.merge_by_size.assign(H + 1, 0);
    st.pull_pairs_by_size.assign(H + 1, 0);
    st.pull_scan_by_size.assign(H + 1, 0);
    st.pull_hits_by_size.assign(H + 1, 0);
    st.pull_seed_scan_by_size.assign(H + 1, 0);
    st.pull_seed_hits_by_size.assign(H + 1, 0);
    st.pull_singleton_scan_by_size.assign(H + 1, 0);
    st.pull_singleton_hits_by_size.assign(H + 1, 0);
    st.pull_dense_dense_scan_by_size.assign(H + 1, 0);
    st.pull_dense_dense_hits_by_size.assign(H + 1, 0);
    st.pull_dense_sparse_scan_by_size.assign(H + 1, 0);
    st.pull_dense_sparse_hits_by_size.assign(H + 1, 0);
    st.pull_sparse_sparse_scan_by_size.assign(H + 1, 0);
    st.pull_sparse_sparse_hits_by_size.assign(H + 1, 0);
    st.search_seed_try_by_size.assign(H + 1, 0);
    st.search_seed_push_by_size.assign(H + 1, 0);
    st.search_pq_pop_by_size.assign(H + 1, 0);
    st.search_relax_try_by_size.assign(H + 1, 0);
    st.search_relax_ok_by_size.assign(H + 1, 0);
    st.complement_calls_by_size.assign(H + 1, 0);
    st.complement_scan_by_size.assign(H + 1, 0);
    st.complement_hits_by_size.assign(H + 1, 0);
    st.complement_cache_builds_by_size.assign(H + 1, 0);
    st.complement_cache_queries_by_size.assign(H + 1, 0);
    st.complement_cache_scan_by_size.assign(H + 1, 0);
    st.complement_cache_hits_by_size.assign(H + 1, 0);
    st.best_after_size.assign(H + 1, fp::kInf);
    st.early_cover_by_rem_size.assign(g + 1, 0);
    st.early_cover_ready_by_rem_size.assign(g + 1, 0);
    st.early_cover_better_by_rem_size.assign(g + 1, 0);
    st.pair_saved_by_cover_size.assign(g + 1, 0);
    st.pair_saved_slack_rel_bucket.assign(6, 0);
    st.dense_rows_by_size.assign(H + 1, 0);
    st.dense_states_by_size.assign(H + 1, 0);
    st.pull_ms_by_size.assign(H + 1, 0.0);
    st.search_ms_by_size.assign(H + 1, 0.0);
    st.complement_ms_by_size.assign(H + 1, 0.0);
    st.complement_cache_ms_by_size.assign(H + 1, 0.0);
    for (int s = 1; s < S; ++s)
    {
        pc[s] = pc[s >> 1] + (s & 1);
        first_bit[s] = (s & 1) ? 0 : first_bit[s >> 1] + 1;
        if (pc[s] <= H)
            st.total_by_size[pc[s]] += n, order.push_back(s);
    }
    std::sort(order.begin(), order.end(), [&](int a, int b)
    { return pc[a] != pc[b] ? pc[a] < pc[b] : a < b; });
    std::vector<int> order_index(S, -1);
    for (int i = 0; i < static_cast<int>(order.size()); ++i)
        order_index[order[i]] = i;

    std::vector<char> needed_after_row(S, 1);
    for (int s : order)
    {
        if (pc[s] < H)
            continue;
        needed_after_row[s] = 0;
        const int rem = U ^ s;
        if (pc[rem] == H)
        {
            needed_after_row[s] = order_index[rem] > order_index[s];
            continue;
        }
        for (int t = rem; t; t &= t - 1)
        {
            int future = rem ^ (1 << first_bit[t]);
            if (order_index[future] > order_index[s])
            {
                needed_after_row[s] = 1;
                break;
            }
        }
    }
    std::vector<int> color(N);
    for (int a = 0; a < g; ++a)
    {
        st.total_group_vertices += static_cast<int>(query.groups[a].size());
        st.max_group_size = std::max(st.max_group_size, static_cast<int>(query.groups[a].size()));
        for (int v : query.groups[a])
            color[v] |= 1 << a;
    }

    std::vector<std::vector<double>> gp(g, std::vector<double>(g, fp::kInf));
    for (int a = 0; a < g; ++a)
        for (int b = 0; b < g; ++b)
            for (int v : query.groups[b])
                gp[a][b] = std::min(gp[a][b], gd[a][v]);

    std::vector<double> mst(S, -1);
    auto MstHalf = [&](int s)
    {
        if (!(s & (s - 1)))
            return 0.0;
        if (mst[s] >= 0)
            return mst[s];
        double sum = 0;
        std::vector<double> d(g, fp::kInf);
        std::vector<char> used(g);
        int z = 0;
        while (!(s >> z & 1))
            ++z;
        d[z] = 0;
        for (int it = 0; it < pc[s]; ++it)
        {
            int u = -1;
            for (int a = 0; a < g; ++a)
                if ((s >> a & 1) && !used[a] && (u < 0 || d[a] < d[u]))
                    u = a;
            used[u] = 1, sum += d[u];
            for (int a = 0; a < g; ++a)
                if ((s >> a & 1) && !used[a])
                    d[a] = std::min(d[a], gp[u][a]);
        }
        return mst[s] = sum * .5;
    };
    auto LowerBound = [&](int v, int s)
    {
        if (!s)
            return 0.0;
        double far = 0, x = fp::kInf, y = fp::kInf;
        for (int t = s; t; t &= t - 1)
        {
            double z = gd[first_bit[t]][v];
            far = std::max(far, z);
            if (z < x)
                y = x, x = z;
            else if (z < y)
                y = z;
        }
        return !(s & (s - 1)) ? far : std::max(far, MstHalf(s) + (x + y) * .5);
    };

    double best = fp::kInf;
    int best_root = 1;
    for (int v = 1; v <= n; ++v)
    {
        double cur = 0;
        for (int a = 0; a < g; ++a)
            cur += gd[a][v];
        if (cur < best)
            best = cur, best_root = v;
    }
    st.root_star_upper = best;

    auto GrowGreedy = [&](std::vector<int> src, int covered, double ans, std::vector<int>* chosen)
    {
        std::vector<int> seen(N), parent(N), in_tree(N);
        std::vector<double> d(N);
        if (chosen)
            chosen->clear();
        int w = 0;
        for (int v : src)
            if (!in_tree[v])
            {
                in_tree[v] = 1, src[w++] = v, covered |= color[v];
                if (chosen && color[v])
                    chosen->push_back(v);
            }
        src.resize(w);
        for (int stamp = 1; covered != U && ans < best; ++stamp)
        {
            Heap q;
            for (int v : src)
                seen[v] = stamp, d[v] = 0, parent[v] = 0, q.push({0, v});
            bool ok = false;
            while (!q.empty())
            {
                auto [du, u] = q.top();
                q.pop(), ++st.greedy_pops;
                if (seen[u] != stamp || du != d[u])
                    continue;
                if (color[u] & (U ^ covered))
                {
                    ans += du, ok = true;
                    for (int x = u; !in_tree[x]; x = parent[x])
                    {
                        int add = color[x] & (U ^ covered);
                        in_tree[x] = 1, src.push_back(x), covered |= color[x];
                        if (chosen && add)
                            chosen->push_back(x);
                    }
                    break;
                }
                for (auto e : graph.adj[u])
                    if (seen[e.to] != stamp || du + e.w < d[e.to])
                        seen[e.to] = stamp, d[e.to] = du + e.w, parent[e.to] = u, q.push({d[e.to], e.to});
            }
            if (!ok)
                return fp::kInf;
        }
        return covered == U ? ans : fp::kInf;
    };
    auto Greedy = [&](int root, std::vector<int>* chosen)
    {
        return GrowGreedy(std::vector<int>{root}, color[root], 0.0, chosen);
    };
    const auto greedy_start = Clock::now();
    std::vector<int> greedy_roots;
    st.greedy_upper = Greedy(best_root, &greedy_roots);
    st.greedy_ms = std::chrono::duration<double, std::milli>(Clock::now() - greedy_start).count();
    best = std::min(best, st.greedy_upper);
    std::sort(greedy_roots.begin(), greedy_roots.end());
    greedy_roots.erase(std::unique(greedy_roots.begin(), greedy_roots.end()), greedy_roots.end());
    st.multi_greedy_upper = best;
    for (int root : greedy_roots)
    {
        if (root == best_root)
            continue;
        ++st.multi_greedy_roots;
        double val = Greedy(root, nullptr);
        if (val < best)
            best = st.multi_greedy_upper = val;
    }
    std::vector<int> pair_partition_roots;
    {
        std::vector<char> pair_root_seen(N);
        auto AddPairRoot = [&](int v)
        {
            if (!pair_root_seen[v])
                pair_root_seen[v] = 1, pair_partition_roots.push_back(v);
        };
        AddPairRoot(best_root);
        for (int root : greedy_roots)
            AddPairRoot(root);
        int min_group = 0;
        for (int a = 1; a < g; ++a)
            if (query.groups[a].size() < query.groups[min_group].size())
                min_group = a;
        for (int root : query.groups[min_group])
            AddPairRoot(root);
    }
    st.greedy_ms = std::chrono::duration<double, std::milli>(Clock::now() - greedy_start).count();
    st.preprocess_ms = std::chrono::duration<double, std::milli>(Clock::now() - solve_start).count();

    const auto dp_start = Clock::now();
    const bool verbose_progress = ProgressLoggingEnabled();
    const bool astar_order = AstarOrderingEnabled();
    st.astar_order_enabled = astar_order ? 1 : 0;
    if (verbose_progress)
        std::cerr << "[Test18] preprocess done elapsed="
                  << std::chrono::duration<double>(Clock::now() - solve_start).count()
                  << " best=" << best
                  << " degree_reduce_ms=" << st.degree_reduce_ms
                  << " degree_removed_vertices=" << st.degree_reduce_removed_vertices
                  << " group_dist_ms=" << st.group_dist_ms
                  << " leaf_reduce_ms=" << st.leaf_reduce_ms
                  << " leaf_removed_vertices=" << st.leaf_reduce_removed_vertices
                  << " two_portal_vertices=" << st.leaf_reduce_two_portal_vertices
                  << " three_portal_vertices=" << st.leaf_reduce_three_portal_vertices
                  << " three_portal_hubs=" << st.leaf_reduce_three_portal_hubs_added
                  << " greedy_ms=" << st.greedy_ms
                  << "\n";
    std::vector<double> full_lb(N);
    for (int v = 1; v <= n; ++v)
    {
        full_lb[v] = LowerBound(v, U);
        if (full_lb[v] <= best)
            ++st.global_root_alive;
        else
            ++st.global_root_pruned;
    }

    std::vector<State> state(S);
    auto Available = [&](int s) { return s && pc[s] <= H && (pc[s] == 1 || state[s].ready); };
    auto Lookup = [&](int s, int v) -> double
    {
        if (!s)
            return 0.0;
        if (full_lb[v] > best)
            return fp::kInf;
        if (pc[s] == 1)
            return gd[first_bit[s]][v];
        if (!Available(s))
            return fp::kInf;
        const auto& z = state[s];
        if (z.dense_row)
            return z.dense[v];
        auto it = std::lower_bound(z.v.begin(), z.v.end(), v);
        if (it == z.v.end() || *it != v)
            return fp::kInf;
        size_t i = it - z.v.begin();
        if (!z.light && z.need[i] > best)
        {
            ++st.lookup_need_skips;
            return fp::kInf;
        }
        return z.d[i];
    };
    auto RawLookup = [&](int s, int v) -> double
    {
        if (!s)
            return 0.0;
        if (full_lb[v] > best)
            return fp::kInf;
        if (pc[s] == 1)
            return gd[first_bit[s]][v];
        if (!Available(s))
            return fp::kInf;
        const auto& z = state[s];
        if (z.dense_row)
            return z.dense[v];
        auto it = std::lower_bound(z.v.begin(), z.v.end(), v);
        if (it == z.v.end() || *it != v)
            return fp::kInf;
        return z.d[it - z.v.begin()];
    };
    bool pair_partition_done = false;
    auto PairPartitionUpper = [&]() -> bool
    {
        if (g < 3)
            return false;
        if (pair_partition_done)
            return false;
        pair_partition_done = true;
        const double old_best = best;
        const auto begin = Clock::now();
        std::vector<double> f(S), block_cost(S);
        for (int root : pair_partition_roots)
        {
            if (full_lb[root] > best)
                continue;
            ++st.pair_partition_roots;
            std::fill(block_cost.begin(), block_cost.end(), fp::kInf);
            block_cost[0] = 0.0;
            for (int block = 1; block < S; ++block)
            {
                if (pc[block] > 2)
                    continue;
                if (pc[block] == 1)
                    block_cost[block] = gd[first_bit[block]][root];
                else if (Available(block))
                    block_cost[block] = Lookup(block, root);
            }
            std::fill(f.begin(), f.end(), fp::kInf);
            f[0] = 0.0;
            for (int mask = 1; mask < S; ++mask)
            {
                int a = first_bit[mask], single = 1 << a;
                int rest = mask ^ single;
                double val = f[rest] + block_cost[single];
                for (int t = rest; t; t &= t - 1)
                {
                    int b = first_bit[t];
                    int pair = single | (1 << b);
                    double d = block_cost[pair];
                    if (d < fp::kInf)
                        val = std::min(val, f[mask ^ pair] + d);
                }
                f[mask] = val;
            }
            if (f[U] < best)
            {
                if (!st.best_updates)
                    st.first_best_update_size = 2;
                st.last_best_update_size = 2;
                ++st.best_updates;
                ++st.pair_partition_updates;
                best = f[U];
            }
        }
        st.pair_partition_upper = best;
        st.pair_partition_ms += std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        return best + 1e-12 < old_best;
    };
    std::vector<double> dist(N, fp::kInf), lb(N), far_cache(N), near_cache(N);
    std::vector<int> cov(N), touched, kept, lb_seen(N), far_seen(N), near_seen(N);
    std::vector<std::pair<int, int>> complete_pairs;
    std::vector<double> complete_row(N, fp::kInf);
    int stamp = 0;
    int current_size = 0;
    double last_compact_best = best;
    auto last_progress = Clock::now();

    auto SavedRowLowerBound = [&](int mask, int v)
    {
        const int rem_mask = U ^ mask;
        if (!rem_mask)
            return 0.0;
        double far = 0, x = fp::kInf, y = fp::kInf;
        for (int t = rem_mask; t; t &= t - 1)
        {
            double z = gd[first_bit[t]][v];
            far = std::max(far, z);
            if (z < x)
                y = x, x = z;
            else if (z < y)
                y = z;
        }
        double ans = !(rem_mask & (rem_mask - 1))
                         ? far
                         : std::max(far, MstHalf(rem_mask) + (x + y) * .5);
        if (pc[mask] < H && pc[rem_mask] >= 2)
            ans = std::max(ans, far + x);
        if (pc[mask] == H && (g & 1))
        {
            double forced = fp::kInf;
            for (int t = rem_mask; t; t &= t - 1)
            {
                int bit = first_bit[t];
                int future = rem_mask ^ (1 << bit);
                if (order_index[future] > order_index[mask])
                    forced = std::min(forced, LowerBound(v, future) + gd[bit][v]);
            }
            if (forced < fp::kInf)
                ans = std::max(ans, forced);
        }
        return ans;
    };

    auto CompactRows = [&]()
    {
        const auto begin = Clock::now();
        long long removed = 0;
        for (int t = 1; t < S; ++t)
        {
            if (pc[t] <= 1 || pc[t] > H || !state[t].ready)
                continue;
            auto& z = state[t];
            if (z.dense_row)
            {
                int alive = 0;
                for (int v = 1; v <= n; ++v)
                {
                    double d = z.dense[v];
                    if (d >= fp::kInf)
                        continue;
                    if (full_lb[v] > best || d + SavedRowLowerBound(t, v) > best)
                    {
                        z.dense[v] = fp::kInf;
                        ++removed;
                        ++st.compact_light_removed;
                        ++st.compact_dense_removed;
                        continue;
                    }
                    ++alive;
                }
                if (alive != z.count)
                    z.count = alive;
                continue;
            }

            int w = 0;
            for (int i = 0; i < static_cast<int>(z.v.size()); ++i)
            {
                int v = z.v[i];
                const double need = z.light ? z.d[i] + SavedRowLowerBound(t, v) : z.need[i];
                if (full_lb[v] > best || need > best)
                {
                    ++removed;
                    if (z.light)
                        ++st.compact_light_removed;
                    continue;
                }
                if (w != i)
                {
                    z.v[w] = z.v[i], z.d[w] = z.d[i];
                    if (!z.light)
                        z.cover[w] = z.cover[i], z.need[w] = z.need[i];
                }
                ++w;
            }
            z.v.resize(w), z.d.resize(w);
            if (!z.light)
                z.cover.resize(w), z.need.resize(w);
            z.count = w;
        }
        ++st.compact_calls;
        st.compact_removed += removed;
        st.compact_ms += std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        last_compact_best = best;
    };

    for (int s : order)
    {
        const int k = pc[s], rem = U ^ s;
        if (k == 1)
            continue;
        if (k != current_size)
        {
            if (current_size && best + 1e-12 < last_compact_best)
                CompactRows();
            if (current_size == 2)
            {
                bool updated = PairPartitionUpper();
                if (updated && best + 1e-12 < last_compact_best)
                    CompactRows();
            }
            current_size = k;
            if (verbose_progress)
            {
                const double elapsed = std::chrono::duration<double>(Clock::now() - dp_start).count();
                std::cerr << "[Test18] enter k=" << k
                          << " elapsed=" << elapsed
                          << " best=" << best
                          << " finite=" << st.finite_states
                          << " inqueue=" << st.total_inqueue
                          << " active_seed=" << st.active_seed
                          << "\n";
                last_progress = Clock::now();
            }
        }
        ++stamp, ++st.masks_processed;
        touched.clear();
        kept.clear();

        int rem_bits[22], rem_cnt = 0;
        for (int t = rem; t; t &= t - 1)
            rem_bits[rem_cnt++] = first_bit[t];
        const double rem_mst = rem_cnt <= 1 ? 0.0 : MstHalf(rem);

        auto Lb = [&](int v)
        {
            if (lb_seen[v] == stamp)
                return lb[v];
            double far = 0, x = fp::kInf, y = fp::kInf;
            for (int i = 0; i < rem_cnt; ++i)
            {
                double z = gd[rem_bits[i]][v];
                far = std::max(far, z);
                if (z < x)
                    y = x, x = z;
                else if (z < y)
                    y = z;
            }
            ++st.lb_calls, lb_seen[v] = stamp;
            return lb[v] = rem_cnt <= 1 ? far : std::max(far, rem_mst + (x + y) * .5);
        };
        auto Far = [&](int v)
        {
            if (far_seen[v] == stamp)
                return far_cache[v];
            double ans = 0;
            for (int i = 0; i < rem_cnt; ++i)
                ans = std::max(ans, gd[rem_bits[i]][v]);
            far_seen[v] = stamp;
            return far_cache[v] = ans;
        };
        auto Near = [&](int v)
        {
            if (near_seen[v] == stamp)
                return near_cache[v];
            double ans = fp::kInf;
            for (int i = 0; i < rem_cnt; ++i)
                ans = std::min(ans, gd[rem_bits[i]][v]);
            near_seen[v] = stamp;
            return near_cache[v] = ans;
        };
        auto SaveNeedLowerBound = [&](int v)
        {
            double ans = Lb(v);
            if (k < H && rem_cnt >= 2)
                ans = std::max(ans, Far(v) + Near(v));
            if (k == H && (g & 1))
            {
                double forced = fp::kInf;
                for (int t = rem; t; t &= t - 1)
                {
                    int bit = first_bit[t];
                    int future = rem ^ (1 << bit);
                    if (order_index[future] > order_index[s])
                        forced = std::min(forced, LowerBound(v, future) + gd[bit][v]);
                }
                if (forced < fp::kInf)
                    ans = std::max(ans, forced);
            }
            return ans;
        };

        auto Set = [&](int v, double w, int c)
        {
            if (w >= dist[v])
                return;
            if (dist[v] == fp::kInf)
                touched.push_back(v);
            dist[v] = w, cov[v] = c;
        };
        auto TrySet = [&](int v, double w, int c)
        {
            ++st.tryset_calls;
            if (full_lb[v] > best)
            {
                ++st.tryset_pruned_full;
                return;
            }
            if (w >= best)
            {
                ++st.tryset_pruned_ge_best;
                return;
            }
            if (w + Far(v) > best)
            {
                ++st.tryset_pruned_far;
                return;
            }
            double old_lb = Lb(v);
            if (w + old_lb > best)
            {
                ++st.tryset_pruned_lb;
                return;
            }
            ++st.tryset_keep;
            Set(v, w, c);
        };

        auto RowCover = [&](const State& row, int mask, int i, int v)
        {
            return row.light ? (mask | color[v]) : row.cover[i];
        };
        auto RowAlive = [&](const State& row, int i)
        {
            if (!row.light && row.need[i] > best)
            {
                ++st.stale_need_skips;
                return false;
            }
            return true;
        };
        auto JoinWithSingleton = [&](int single, int row_mask, const State& row)
        {
            int bit = first_bit[single];
            if (row.dense_row)
            {
                st.pull_scan += n;
                st.pull_scan_by_size[k] += n;
                st.pull_singleton_scan_by_size[k] += n;
                for (int v = 1; v <= n; ++v)
                    if (row.dense[v] < fp::kInf)
                    {
                        ++st.pull_hits;
                        ++st.pull_hits_by_size[k];
                        ++st.pull_singleton_hits_by_size[k];
                        TrySet(v, gd[bit][v] + row.dense[v], row_mask | single | color[v]);
                    }
                return;
            }
            st.pull_scan += row.v.size();
            st.pull_scan_by_size[k] += static_cast<long long>(row.v.size());
            st.pull_singleton_scan_by_size[k] += static_cast<long long>(row.v.size());
            for (size_t i = 0; i < row.v.size(); ++i)
            {
                ++st.pull_hits;
                ++st.pull_hits_by_size[k];
                ++st.pull_singleton_hits_by_size[k];
                if (!RowAlive(row, static_cast<int>(i)))
                    continue;
                int v = row.v[i];
                TrySet(v, gd[bit][v] + row.d[i], RowCover(row, row_mask, static_cast<int>(i), v) | single | color[v]);
            }
        };
        auto JoinStates = [&](int am, const State& x, int bm, const State& y)
        {
            if (x.dense_row && y.dense_row)
            {
                st.pull_scan += n;
                st.pull_scan_by_size[k] += n;
                st.pull_dense_dense_scan_by_size[k] += n;
                for (int v = 1; v <= n; ++v)
                    if (x.dense[v] < fp::kInf && y.dense[v] < fp::kInf)
                    {
                        ++st.pull_hits;
                        ++st.pull_hits_by_size[k];
                        ++st.pull_dense_dense_hits_by_size[k];
                        TrySet(v, x.dense[v] + y.dense[v], am | bm | color[v]);
                    }
                return;
            }
            if (x.dense_row || y.dense_row)
            {
                const State& den = x.dense_row ? x : y;
                const State& sp = x.dense_row ? y : x;
                int den_mask = x.dense_row ? am : bm;
                int sp_mask = x.dense_row ? bm : am;
                st.pull_scan += sp.v.size();
                st.pull_scan_by_size[k] += static_cast<long long>(sp.v.size());
                st.pull_dense_sparse_scan_by_size[k] += static_cast<long long>(sp.v.size());
                for (int i = 0; i < static_cast<int>(sp.v.size()); ++i)
                {
                    int v = sp.v[i];
                    double da = den.dense[v];
                    if (da >= fp::kInf)
                        continue;
                    ++st.pull_hits;
                    ++st.pull_hits_by_size[k];
                    ++st.pull_dense_sparse_hits_by_size[k];
                    if (!RowAlive(sp, i))
                        continue;
                    TrySet(v, da + sp.d[i], (den_mask | color[v]) | RowCover(sp, sp_mask, i, v));
                }
                return;
            }
            long long local_scan = 0;
            long long local_hits = 0;
            JoinRows(x.v, x.d, y.v, y.d,
                     [&](int v, double da, double db, size_t ia, size_t ib)
                     {
                         if (!RowAlive(x, static_cast<int>(ia)) || !RowAlive(y, static_cast<int>(ib)))
                             return;
                         double cand = da + db;
                         TrySet(v, cand, RowCover(x, am, static_cast<int>(ia), v) |
                                             RowCover(y, bm, static_cast<int>(ib), v));
                     },
                     local_scan, local_hits);
            st.pull_scan += local_scan;
            st.pull_hits += local_hits;
            st.pull_scan_by_size[k] += local_scan;
            st.pull_hits_by_size[k] += local_hits;
            st.pull_sparse_sparse_scan_by_size[k] += local_scan;
            st.pull_sparse_sparse_hits_by_size[k] += local_hits;
        };
        auto CacheRowAlive = [&](const State& row, int i)
        {
            if (!row.light && row.need[i] > best)
            {
                ++st.complement_cache_need_skips;
                return false;
            }
            return true;
        };
        auto EstimateJoinCost = [&](size_t a, size_t b) -> long long
        {
            const long long linear = static_cast<long long>(a + b);
            const long long abin = static_cast<long long>(a) * LogCost(b + 1);
            const long long bbin = static_cast<long long>(b) * LogCost(a + 1);
            if (a && b && abin * 3 < linear * 2)
                return abin;
            if (a && b && bbin * 3 < linear * 2)
                return bbin;
            return linear;
        };
        auto EstimateMaskLookupCost = [&](int mask) -> long long
        {
            if (!mask)
                return 0;
            if (pc[mask] == 1)
                return 1;
            const State& row = state[mask];
            if (row.dense_row)
                return 1;
            return LogCost(row.v.size() + 1);
        };
        auto EstimateMaskScanCost = [&](int mask) -> long long
        {
            if (!mask || pc[mask] == 1)
                return n;
            const State& row = state[mask];
            return row.dense_row ? n : static_cast<long long>(row.v.size());
        };
        auto EstimateCompletePairBuildCost = [&](int x, int y) -> long long
        {
            if (!x)
                return EstimateMaskScanCost(y);
            if (!y)
                return EstimateMaskScanCost(x);
            if (pc[x] == 1 && pc[y] == 1)
                return n;
            if (pc[x] == 1)
                return EstimateMaskScanCost(y);
            if (pc[y] == 1)
                return EstimateMaskScanCost(x);
            const State& a = state[x];
            const State& b = state[y];
            if (a.dense_row && b.dense_row)
                return n;
            if (a.dense_row)
                return b.dense_row ? n : static_cast<long long>(b.v.size());
            if (b.dense_row)
                return static_cast<long long>(a.v.size());
            return EstimateJoinCost(a.v.size(), b.v.size());
        };
        auto PutCompleteRow = [&](int v, double value, long long& hits)
        {
            if (value >= fp::kInf)
                return;
            ++hits;
            if (value < complete_row[v])
                complete_row[v] = value;
        };
        auto AddMaskToCompleteRow = [&](int mask, long long& scans, long long& hits)
        {
            if (!mask)
            {
                scans += n;
                for (int v = 1; v <= n; ++v)
                    if (full_lb[v] <= best)
                        PutCompleteRow(v, 0.0, hits);
                return;
            }
            if (pc[mask] == 1)
            {
                scans += n;
                const int bit = first_bit[mask];
                for (int v = 1; v <= n; ++v)
                    if (full_lb[v] <= best)
                        PutCompleteRow(v, gd[bit][v], hits);
                return;
            }
            const State& row = state[mask];
            if (row.dense_row)
            {
                scans += n;
                for (int v = 1; v <= n; ++v)
                    if (full_lb[v] <= best)
                        PutCompleteRow(v, row.dense[v], hits);
                return;
            }
            scans += static_cast<long long>(row.v.size());
            for (int i = 0; i < static_cast<int>(row.v.size()); ++i)
            {
                int v = row.v[i];
                if (full_lb[v] > best || !CacheRowAlive(row, i))
                    continue;
                PutCompleteRow(v, row.d[i], hits);
            }
        };
        auto AddCompletePairToRow = [&](int x, int y, long long& scans, long long& hits)
        {
            if (!x)
            {
                AddMaskToCompleteRow(y, scans, hits);
                return;
            }
            if (!y)
            {
                AddMaskToCompleteRow(x, scans, hits);
                return;
            }
            if (pc[x] == 1 && pc[y] == 1)
            {
                scans += n;
                const int xb = first_bit[x], yb = first_bit[y];
                for (int v = 1; v <= n; ++v)
                    if (full_lb[v] <= best)
                        PutCompleteRow(v, gd[xb][v] + gd[yb][v], hits);
                return;
            }
            if (pc[x] == 1 || pc[y] == 1)
            {
                const int single = pc[x] == 1 ? x : y;
                const int row_mask = pc[x] == 1 ? y : x;
                const int bit = first_bit[single];
                const State& row = state[row_mask];
                if (row.dense_row)
                {
                    scans += n;
                    for (int v = 1; v <= n; ++v)
                        if (full_lb[v] <= best && row.dense[v] < fp::kInf)
                            PutCompleteRow(v, gd[bit][v] + row.dense[v], hits);
                    return;
                }
                scans += static_cast<long long>(row.v.size());
                for (int i = 0; i < static_cast<int>(row.v.size()); ++i)
                {
                    int v = row.v[i];
                    if (full_lb[v] > best || !CacheRowAlive(row, i))
                        continue;
                    PutCompleteRow(v, gd[bit][v] + row.d[i], hits);
                }
                return;
            }

            const State& a = state[x];
            const State& b = state[y];
            if (a.dense_row && b.dense_row)
            {
                scans += n;
                for (int v = 1; v <= n; ++v)
                    if (full_lb[v] <= best && a.dense[v] < fp::kInf && b.dense[v] < fp::kInf)
                        PutCompleteRow(v, a.dense[v] + b.dense[v], hits);
                return;
            }
            if (a.dense_row || b.dense_row)
            {
                const State& den = a.dense_row ? a : b;
                const State& sp = a.dense_row ? b : a;
                scans += static_cast<long long>(sp.v.size());
                for (int i = 0; i < static_cast<int>(sp.v.size()); ++i)
                {
                    int v = sp.v[i];
                    double dv = den.dense[v];
                    if (full_lb[v] > best || dv >= fp::kInf || !CacheRowAlive(sp, i))
                        continue;
                    PutCompleteRow(v, dv + sp.d[i], hits);
                }
                return;
            }

            long long local_scan = 0;
            long long ignored_hits = 0;
            JoinRows(a.v, a.d, b.v, b.d,
                     [&](int v, double da, double db, size_t ia, size_t ib)
                     {
                         if (full_lb[v] > best ||
                             !CacheRowAlive(a, static_cast<int>(ia)) ||
                             !CacheRowAlive(b, static_cast<int>(ib)))
                             return;
                         PutCompleteRow(v, da + db, hits);
                     },
                     local_scan, ignored_hits);
            scans += local_scan;
        };
        const auto pull_begin = Clock::now();
        const long long hits_before = st.pull_hits;
        if (k == 2)
        {
            int a = first_bit[s], b = first_bit[s ^ (1 << a)];
            ++st.pull_pairs;
            ++st.pull_pairs_by_size[k];
            st.pull_scan += n;
            st.pull_scan_by_size[k] += n;
            st.pull_seed_scan_by_size[k] += n;
            for (int v = 1; v <= n; ++v)
            {
                ++st.pull_hits;
                ++st.pull_hits_by_size[k];
                ++st.pull_seed_hits_by_size[k];
                TrySet(v, gd[a][v] + gd[b][v], s | color[v]);
            }
        }
        else
        {
            for (int a = (s - 1) & s; a; a = (a - 1) & s)
            {
                int b = s ^ a;
                if (!b || a > b || !Available(a) || !Available(b))
                    continue;
                ++st.pull_pairs;
                ++st.pull_pairs_by_size[k];
                if (pc[a] == 1)
                {
                    JoinWithSingleton(a, b, state[b]);
                    continue;
                }
                if (pc[b] == 1)
                {
                    JoinWithSingleton(b, a, state[a]);
                    continue;
                }
                JoinStates(a, state[a], b, state[b]);
            }
        }
        st.pull_ms += std::chrono::duration<double, std::milli>(Clock::now() - pull_begin).count();
        st.pull_ms_by_size[k] += std::chrono::duration<double, std::milli>(Clock::now() - pull_begin).count();
        st.merge_by_size[k] += st.pull_hits - hits_before;

        st.active_seed += touched.size();
        st.active_by_size[k] += touched.size();

        const bool can_complete = k * 3 >= g;
        complete_pairs.clear();
        if (can_complete)
        {
            for (int x = rem;; x = (x - 1) & rem)
            {
                int y = rem ^ x;
                if (x <= y && pc[x] <= H && pc[y] <= H &&
                    (!x || Available(x)) && (!y || Available(y)))
                    complete_pairs.push_back({x, y});
                if (!x)
                    break;
            }
            st.complement_mask_pairs += complete_pairs.size();
            if (complete_pairs.empty())
                ++st.complement_mask_empty;
        }
        long long complete_row_rent_per_query = 0;
        long long complete_row_buy_cost = 0;
        long long complete_row_paid_rent_cost = 0;
        bool complete_row_ready = false;
        if (can_complete && !complete_pairs.empty())
        {
            for (auto [x, y] : complete_pairs)
            {
                complete_row_rent_per_query += EstimateMaskLookupCost(x) + EstimateMaskLookupCost(y);
                complete_row_buy_cost += EstimateCompletePairBuildCost(x, y);
            }
        }
        auto BuildCompleteRow = [&]()
        {
            const auto begin = Clock::now();
            std::fill(complete_row.begin(), complete_row.end(), fp::kInf);
            long long scans = 0;
            long long hits = 0;
            for (auto [x, y] : complete_pairs)
                AddCompletePairToRow(x, y, scans, hits);
            complete_row_ready = true;
            ++st.complement_cache_builds;
            ++st.complement_cache_builds_by_size[k];
            st.complement_cache_scan += scans;
            st.complement_cache_hits += hits;
            st.complement_cache_scan_by_size[k] += scans;
            st.complement_cache_hits_by_size[k] += hits;
            st.complement_cache_buy_cost += complete_row_buy_cost;
            const double elapsed_ms =
                std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
            st.complement_cache_ms += elapsed_ms;
            st.complement_cache_ms_by_size[k] += elapsed_ms;
        };

        auto Complete = [&](int u, double d)
        {
            ++st.complement_calls;
            ++st.complement_calls_by_size[k];
            int cover_rem = U ^ (cov[u] & U);
            if (!can_complete)
            {
                ++st.complement_skip_early;
                if (cover_rem != rem)
                {
                    ++st.complement_cover_smaller;
                    ++st.early_cover_extra;
                    st.early_cover_extra_groups += pc[rem] - pc[cover_rem];
                    if (!st.early_cover_min_rem || pc[cover_rem] < st.early_cover_min_rem)
                        st.early_cover_min_rem = pc[cover_rem];
                    ++st.early_cover_by_rem_size[pc[cover_rem]];
                    if (pc[cover_rem] <= 2 * k)
                    {
                        ++st.early_cover_rem_le_2k;
                        ++st.early_cover_pair_checks;
                        bool ready = false;
                        double best_cand = fp::kInf;
                        for (int x = cover_rem;; x = (x - 1) & cover_rem)
                        {
                            int y = cover_rem ^ x;
                            if (x <= y && pc[x] <= H && pc[y] <= H &&
                                (!x || Available(x)) && (!y || Available(y)))
                            {
                                ++st.early_cover_pair_splits;
                                double dx = RawLookup(x, u), dy = RawLookup(y, u);
                                if (dx < fp::kInf && dy < fp::kInf)
                                {
                                    ready = true;
                                    double cand = d + dx + dy;
                                    if (cand < best_cand)
                                        best_cand = cand;
                                }
                            }
                            if (!x)
                                break;
                        }
                        if (ready)
                        {
                            ++st.early_cover_pair_ready;
                            ++st.early_cover_ready_by_rem_size[pc[cover_rem]];
                        }
                        if (best_cand < fp::kInf &&
                            (st.early_cover_best_candidate < 0 || best_cand < st.early_cover_best_candidate))
                            st.early_cover_best_candidate = best_cand;
                        if (best_cand < best)
                        {
                            if (!st.best_updates)
                                st.first_best_update_size = k;
                            st.last_best_update_size = k;
                            ++st.best_updates;
                            ++st.early_cover_best_updates;
                            ++st.early_cover_pair_better;
                            ++st.early_cover_better_by_rem_size[pc[cover_rem]];
                            if (!st.early_cover_first_better_size)
                                st.early_cover_first_better_size = k;
                            best = best_cand;
                        }
                    }
                    if (pc[cover_rem] <= 2 * k)
                        ++st.complement_cover_possible;
                }
                return;
            }
            const auto begin = Clock::now();
            double other = fp::kInf;
            auto TryCompleteSplit = [&](int x, int y)
            {
                ++st.complement_pairs;
                double dx = Lookup(x, u), dy = Lookup(y, u);
                ++st.complement_scan;
                ++st.complement_scan_by_size[k];
                if (dx < fp::kInf && dy < fp::kInf)
                {
                    other = std::min(other, dx + dy);
                    ++st.complement_hits;
                    ++st.complement_hits_by_size[k];
                }
            };
            if (cover_rem == rem)
            {
                if (complete_row_ready)
                {
                    other = complete_row[u];
                    ++st.complement_cache_queries;
                    ++st.complement_cache_queries_by_size[k];
                }
                else if (complete_row_buy_cost > 0 && complete_row_rent_per_query > 0 &&
                         complete_row_paid_rent_cost + complete_row_rent_per_query >= complete_row_buy_cost)
                {
                    BuildCompleteRow();
                    other = complete_row[u];
                    ++st.complement_cache_queries;
                    ++st.complement_cache_queries_by_size[k];
                }
                else
                {
                    complete_row_paid_rent_cost += complete_row_rent_per_query;
                    st.complement_cache_rent_cost += complete_row_rent_per_query;
                    for (auto [x, y] : complete_pairs)
                        TryCompleteSplit(x, y);
                }
            }
            else
            {
                ++st.complement_cover_smaller;
                ++st.complement_cover_possible;
                for (int x = cover_rem;; x = (x - 1) & cover_rem)
                {
                    int y = cover_rem ^ x;
                    if (x <= y && pc[x] <= H && pc[y] <= H &&
                        (!x || Available(x)) && (!y || Available(y)))
                        TryCompleteSplit(x, y);
                    if (!x)
                        break;
                }
            }
            if (other < fp::kInf)
            {
                double cand = d + other;
                if (cand < best)
                {
                    if (!st.best_updates)
                        st.first_best_update_size = k;
                    st.last_best_update_size = k;
                    ++st.best_updates;
                    best = cand;
                }
            }
            const double elapsed_ms =
                std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
            st.complement_ms += elapsed_ms;
            st.complement_ms_by_size[k] += elapsed_ms;
        };

        const auto search_begin = Clock::now();
        Heap q;
        auto QueueKey = [&](int v, double d)
        {
            return astar_order ? d + Lb(v) : d;
        };
        for (int v : touched)
        {
            double d = dist[v];
            ++st.seed_try;
            ++st.search_seed_try_by_size[k];
            if (d + Far(v) > best)
            {
                ++st.seed_block_far;
                continue;
            }
            double old_lb = Lb(v);
            if (d + old_lb > best)
            {
                ++st.seed_block_lb;
                continue;
            }
            q.push({astar_order ? d + old_lb : d, v});
            ++st.pq_push, ++st.seed_push, ++st.search_seed_push_by_size[k];
        }

        while (!q.empty())
        {
            auto [key, u] = q.top();
            q.pop(), ++st.pq_pop, ++st.search_pq_pop_by_size[k];
            double d = dist[u];
            if (key != QueueKey(u, d))
                continue;
            if (full_lb[u] > best)
            {
                ++st.pop_pruned_full;
                continue;
            }
            if (d + Far(u) > best)
            {
                ++st.pop_pruned_far;
                continue;
            }
            double old_lb = Lb(u);
            if (d + old_lb > best)
            {
                ++st.pop_pruned_lb;
                continue;
            }
            Complete(u, d);
            kept.push_back(u);
            ++st.total_inqueue, ++st.inqueue_by_size[k];
            for (auto e : graph.adj[u])
            {
                ++st.relax_try;
                ++st.search_relax_try_by_size[k];
                double nd = d + e.w;
                if (nd >= dist[e.to])
                    continue;
                if (full_lb[e.to] > best)
                {
                    ++st.tryset_pruned_full;
                    continue;
                }
                if (nd >= best)
                {
                    ++st.prune_ge_best;
                    continue;
                }
                if (nd + Far(e.to) > best)
                {
                    ++st.prune_far;
                    continue;
                }
                double queue_key = nd;
                if (astar_order)
                {
                    double next_lb = Lb(e.to);
                    if (nd + next_lb > best)
                    {
                        ++st.astar_relax_lb_pruned;
                        continue;
                    }
                    queue_key = nd + next_lb;
                }
                if (dist[e.to] == fp::kInf)
                    touched.push_back(e.to);
                dist[e.to] = nd, cov[e.to] = cov[u] | color[e.to];
                ++st.relax_ok, ++st.search_relax_ok_by_size[k], ++st.pq_push;
                q.push({queue_key, e.to});
            }
        }
        const double search_elapsed_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - search_begin).count();
        st.search_ms += search_elapsed_ms;
        st.search_ms_by_size[k] += search_elapsed_ms;

        std::sort(kept.begin(), kept.end());
        kept.erase(std::unique(kept.begin(), kept.end()), kept.end());
        int keep_w = 0;
        for (int v : kept)
        {
            double base_need = dist[v] + Lb(v);
            double need = dist[v] + SaveNeedLowerBound(v);
            if (full_lb[v] > best)
            {
                ++st.final_pruned_full;
                continue;
            }
            if (need > best)
            {
                ++st.final_pruned_need;
                if (base_need <= best && k == H && (g & 1))
                    ++st.order_lb_pruned_states;
                if (base_need <= best && k < H)
                    ++st.order_split_pruned_states;
                continue;
            }
            kept[keep_w++] = v;
        }
        kept.resize(keep_w);

        auto& z = state[s];
        z.v.clear(), z.d.clear(), z.cover.clear(), z.need.clear(), z.dense.clear();
        if (!needed_after_row[s])
        {
            ++st.order_pruned_rows;
            st.order_pruned_states += static_cast<long long>(kept.size());
            z.count = 0;
            z.ready = false;
            for (int v : touched)
                dist[v] = fp::kInf;
            st.best_after_size[k] = best;
            continue;
        }
        z.count = static_cast<int>(kept.size());
        const bool structural_pair_light = k == 2;
        const bool dense_by_cost = static_cast<long long>(z.count) * 3 > static_cast<long long>(N) * 2;
        z.dense_row = dense_by_cost;
        // Pair rows are structurally light; dense-light is selected only by representation cost.
        z.light = structural_pair_light || z.dense_row;
        if (z.dense_row)
            z.dense.assign(N, fp::kInf);
        else
            z.v.reserve(kept.size()), z.d.reserve(kept.size());
        if (!z.light)
            z.cover.reserve(kept.size()), z.need.reserve(kept.size());
        if (z.dense_row)
        {
            ++st.dense_rows;
            ++st.dense_rows_by_size[k];
        }
        if (z.dense_row && k == 2)
            ++st.pair_dense_rows;
        for (int v : kept)
        {
            double need = dist[v] + SaveNeedLowerBound(v);
            if (z.light)
            {
                if (z.dense_row)
                    z.dense[v] = dist[v];
                else
                    z.v.push_back(v), z.d.push_back(dist[v]);
                if (k == 2)
                {
                    int cover_size = pc[cov[v] & U];
                    ++st.pair_saved_by_cover_size[cover_size];
                    if (cover_size > 2)
                    {
                        ++st.pair_saved_cover_extra;
                        st.pair_saved_cover_extra_groups += cover_size - 2;
                    }
                    double rel = best > 0 ? (best - need) / best : best - need;
                    if (rel < 0)
                        rel = 0;
                    ++st.pair_saved_slack_count;
                    st.pair_saved_slack_rel_sum += rel;
                    st.pair_saved_slack_rel_max = std::max(st.pair_saved_slack_rel_max, rel);
                    int b = rel <= .01 ? 0 : rel <= .05 ? 1 : rel <= .10 ? 2 : rel <= .25 ? 3 : rel <= .50 ? 4 : 5;
                    ++st.pair_saved_slack_rel_bucket[b];
                }
            }
            else
                z.v.push_back(v), z.d.push_back(dist[v]), z.cover.push_back(cov[v]), z.need.push_back(need);
        }
        for (int v : touched)
            dist[v] = fp::kInf;
        z.ready = true;
        if (z.dense_row)
        {
            st.dense_states += z.count;
            st.dense_states_by_size[k] += z.count;
        }
        if (z.dense_row && k == 2)
            st.pair_dense_states += z.count;
        st.finite_states += z.count;
        st.best_after_size[k] = best;
        if (verbose_progress && std::chrono::duration<double>(Clock::now() - last_progress).count() >= 5.0)
        {
            const double elapsed = std::chrono::duration<double>(Clock::now() - dp_start).count();
            const auto& sb = st.pair_saved_slack_rel_bucket;
            long long le1 = sb[0];
            long long le5 = le1 + sb[1];
            long long le10 = le5 + sb[2];
            long long le25 = le10 + sb[3];
            long long le50 = le25 + sb[4];
            double slack_avg = st.pair_saved_slack_count
                                   ? st.pair_saved_slack_rel_sum / st.pair_saved_slack_count
                                   : -1.0;
            const auto mem = gst::GetProcessMemoryUsage();
            std::cerr << "[Test18] k=" << k
                      << " masks=" << st.masks_processed
                      << " elapsed=" << elapsed
                      << " best=" << best
                      << " finite=" << st.finite_states
                      << " inqueue=" << st.total_inqueue
                      << " active_seed=" << st.active_seed
                      << " try_keep=" << st.tryset_keep
                      << " full_prune=" << st.tryset_pruned_full
                      << " far_prune=" << st.tryset_pruned_far
                      << " lb_prune=" << st.tryset_pruned_lb
                      << " seed_push=" << st.seed_push
                      << " seed_block_lb=" << st.seed_block_lb
                      << " early_cover=" << st.early_cover_extra
                      << " early_ready=" << st.early_cover_pair_ready
                      << " early_better=" << st.early_cover_pair_better
                      << " early_updates=" << st.early_cover_best_updates
                      << " early_cand=" << st.early_cover_best_candidate
                      << " pair_cover_extra=" << st.pair_saved_cover_extra
                      << " pair_cover_extra_groups=" << st.pair_saved_cover_extra_groups
                      << " pair_dense_rows=" << st.pair_dense_rows
                      << " dense_rows=" << st.dense_rows
                      << " dense_states=" << st.dense_states
                      << " rss_mb=" << gst::BytesToMiB(mem.current_rss_bytes)
                      << " peak_mb=" << gst::BytesToMiB(mem.peak_rss_bytes)
                      << " pair_slack_avg=" << slack_avg
                      << " pair_slack_le1p=" << le1
                      << " pair_slack_le5p=" << le5
                      << " pair_slack_le10p=" << le10
                      << " pair_slack_le25p=" << le25
                      << " pair_slack_le50p=" << le50
                      << " pair_slack_gt50p=" << sb[5]
                      << " stale_skip=" << st.stale_need_skips
                      << "\n";
            last_progress = Clock::now();
        }
    }
    st.dp_ms = std::chrono::duration<double, std::milli>(Clock::now() - dp_start).count();
    res.best_weight = best;
    res.feasible = true;
    return res;
}

}  // namespace gst::methods::test18
