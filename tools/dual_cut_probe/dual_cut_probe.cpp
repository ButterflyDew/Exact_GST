#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "float_compare.h"
#include "graph_io.h"
#include "methods/Test/test19_future_bounds.h"
#include "methods/Common/dual_cut_potential.h"
#include "query_io.h"

namespace
{
using Clock = std::chrono::steady_clock;
using HeapItem = std::pair<double, int>;
using gst::methods::dual_cut::DualCutPotential;

int FirstBit(int mask)
{
    int bit = 0;
    while (!(mask & 1))
    {
        mask >>= 1;
        ++bit;
    }
    return bit;
}

std::vector<std::vector<double>> GroupDistances(const gst::Graph& graph,
                                                const gst::Query& query)
{
    const int g = static_cast<int>(query.groups.size());
    std::vector<std::vector<double>> distance(
        g, std::vector<double>(graph.n + 1, gst::fp::kInf));
    std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
    for (int group = 0; group < g; ++group)
    {
        for (int source : query.groups[group])
        {
            distance[group][source] = 0.0;
            heap.push({0.0, source});
        }
        while (!heap.empty())
        {
            const auto [d, u] = heap.top();
            heap.pop();
            if (d != distance[group][u])
                continue;
            for (const auto& edge : graph.adj[u])
            {
                const double next = d + edge.w;
                if (next < distance[group][edge.to])
                {
                    distance[group][edge.to] = next;
                    heap.push({next, edge.to});
                }
            }
        }
    }
    return distance;
}

std::vector<std::vector<double>> GroupMetric(
    const gst::Query& query, const std::vector<std::vector<double>>& group_distance)
{
    const int g = static_cast<int>(query.groups.size());
    std::vector<std::vector<double>> metric(g, std::vector<double>(g, gst::fp::kInf));
    for (int a = 0; a < g; ++a)
        for (int b = 0; b < g; ++b)
            for (int vertex : query.groups[b])
                metric[a][b] = std::min(metric[a][b], group_distance[a][vertex]);
    return metric;
}

double RootStar(const std::vector<std::vector<double>>& group_distance,
                int n,
                int& root)
{
    double best = gst::fp::kInf;
    for (int v = 1; v <= n; ++v)
    {
        double value = 0.0;
        for (const auto& distance : group_distance)
            value += distance[v];
        if (value < best)
        {
            best = value;
            root = v;
        }
    }
    return best;
}

double GreedyUpper(const gst::Graph& graph,
                   const std::vector<int>& color,
                   int full_mask,
                   int root,
                   double limit)
{
    const int n = graph.n;
    std::vector<char> in_tree(n + 1);
    std::vector<int> parent(n + 1);
    std::vector<double> distance(n + 1, gst::fp::kInf);
    in_tree[root] = 1;
    int covered = color[root];
    double cost = 0.0;

    while (covered != full_mask && cost < limit)
    {
        std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
        std::fill(distance.begin(), distance.end(), gst::fp::kInf);
        for (int v = 1; v <= n; ++v)
        {
            if (!in_tree[v])
                continue;
            distance[v] = 0.0;
            parent[v] = 0;
            heap.push({0.0, v});
        }

        int found = 0;
        while (!heap.empty())
        {
            const auto [d, u] = heap.top();
            heap.pop();
            if (d != distance[u])
                continue;
            if (color[u] & (full_mask ^ covered))
            {
                found = u;
                cost += d;
                break;
            }
            for (const auto& edge : graph.adj[u])
            {
                const double next = d + edge.w;
                if (next < distance[edge.to])
                {
                    distance[edge.to] = next;
                    parent[edge.to] = u;
                    heap.push({next, edge.to});
                }
            }
        }
        if (!found)
            return gst::fp::kInf;
        for (int v = found; v && !in_tree[v]; v = parent[v])
        {
            in_tree[v] = 1;
            covered |= color[v];
        }
    }
    return cost;
}

std::vector<std::vector<double>> ExactRootedDp(
    const gst::Graph& graph, const std::vector<std::vector<double>>& group_distance)
{
    const int g = static_cast<int>(group_distance.size());
    const int subset_count = 1 << g;
    std::vector<std::vector<double>> dp(
        subset_count, std::vector<double>(graph.n + 1, gst::fp::kInf));
    for (int group = 0; group < g; ++group)
        dp[1 << group] = group_distance[group];

    for (int mask = 1; mask < subset_count; ++mask)
    {
        if (!(mask & (mask - 1)))
            continue;
        for (int left = (mask - 1) & mask; left; left = (left - 1) & mask)
        {
            const int right = mask ^ left;
            if (!right || left > right)
                continue;
            for (int v = 1; v <= graph.n; ++v)
                dp[mask][v] = std::min(dp[mask][v], dp[left][v] + dp[right][v]);
        }
        std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
        for (int v = 1; v <= graph.n; ++v)
            heap.push({dp[mask][v], v});
        while (!heap.empty())
        {
            const auto [d, u] = heap.top();
            heap.pop();
            if (d != dp[mask][u])
                continue;
            for (const auto& edge : graph.adj[u])
            {
                const double next = d + edge.w;
                if (next < dp[mask][edge.to])
                {
                    dp[mask][edge.to] = next;
                    heap.push({next, edge.to});
                }
            }
        }
    }
    return dp;
}

void AddEdge(gst::Graph& graph, int u, int v, double weight)
{
    const int id = static_cast<int>(graph.edges.size());
    graph.edges.push_back({id, u, v, weight});
    graph.adj[u].push_back({v, id, weight});
    graph.adj[v].push_back({u, id, weight});
    graph.m = static_cast<int>(graph.edges.size());
}

std::pair<gst::Graph, gst::Query> RandomInstance(std::mt19937_64& random,
                                                 int max_g,
                                                 int max_n)
{
    std::uniform_int_distribution<int> n_distribution(4, max_n);
    const int n = n_distribution(random);
    std::uniform_int_distribution<int> g_distribution(2, std::min(max_g, 7));
    const int g = g_distribution(random);
    std::uniform_int_distribution<int> weight_distribution(1, 30);

    gst::Graph graph;
    graph.n = n;
    graph.adj.resize(n + 1);
    for (int v = 2; v <= n; ++v)
    {
        std::uniform_int_distribution<int> parent_distribution(1, v - 1);
        AddEdge(graph, v, parent_distribution(random), weight_distribution(random));
    }
    std::bernoulli_distribution extra_edge(0.35);
    for (int u = 1; u <= n; ++u)
    {
        for (int v = u + 1; v <= n; ++v)
        {
            bool exists = false;
            for (const auto& edge : graph.adj[u])
                exists = exists || edge.to == v;
            if (!exists && extra_edge(random))
                AddEdge(graph, u, v, weight_distribution(random));
        }
    }

    gst::Query query;
    query.groups.resize(g);
    std::uniform_int_distribution<int> vertex_distribution(1, n);
    std::uniform_int_distribution<int> size_distribution(1, std::min(3, n));
    for (auto& group : query.groups)
    {
        const int size = size_distribution(random);
        while (static_cast<int>(group.size()) < size)
        {
            const int v = vertex_distribution(random);
            if (std::find(group.begin(), group.end(), v) == group.end())
                group.push_back(v);
        }
    }
    return {std::move(graph), std::move(query)};
}

int SelfCheck(std::uint64_t seed, int iterations, int max_g, int max_n)
{
    std::mt19937_64 random(seed);
    long long admissibility_checks = 0;
    long long consistency_checks = 0;
    long long splice_checks = 0;
    long long objective_checks = 0;
    for (int iteration = 1; iteration <= iterations; ++iteration)
    {
        auto [graph, query] = RandomInstance(random, max_g, max_n);
        const auto group_distance = GroupDistances(graph, query);
        int root = 1;
        RootStar(group_distance, graph.n, root);
        DualCutPotential dual;
        dual.Build(graph, query, group_distance, root);
        int anchor_group = 0;
        for (int group = 1; group < static_cast<int>(query.groups.size()); ++group)
            if (group_distance[group][root] > group_distance[anchor_group][root])
                anchor_group = group;
        DualCutPotential anchor_dual;
        anchor_dual.BuildFromGroup(graph, query, group_distance, anchor_group);
        DualCutPotential dynamic_dual;
        dynamic_dual.BuildDynamic(graph, query, group_distance, root);
        const auto exact = ExactRootedDp(graph, group_distance);
        const int subset_count = 1 << query.groups.size();
        const int full_mask = subset_count - 1;
        double exact_unrooted = gst::fp::kInf;
        for (int v = 1; v <= graph.n; ++v)
            exact_unrooted = std::min(exact_unrooted, exact[full_mask][v]);
        ++objective_checks;
        if (anchor_dual.Objective() > exact_unrooted + 1e-8)
        {
            std::cout << std::setprecision(17)
                      << "OBJECTIVE_FAIL seed=" << seed
                      << " iteration=" << iteration
                      << " objective=" << anchor_dual.Objective()
                      << " exact=" << exact_unrooted << '\n';
            return 1;
        }
        for (int mask = 1; mask < subset_count; ++mask)
        {
            for (int v = 1; v <= graph.n; ++v)
            {
                ++admissibility_checks;
                const double candidate = std::max(
                    std::max(dual.At(v, mask), anchor_dual.At(v, mask)),
                    dynamic_dual.At(v, mask));
                if (candidate > exact[mask][v] + 1e-8)
                {
                    std::cout << std::setprecision(17)
                              << "ADMISSIBILITY_FAIL seed=" << seed
                              << " iteration=" << iteration
                              << " mask=" << mask
                              << " vertex=" << v
                              << " dual=" << candidate
                              << " exact=" << exact[mask][v] << '\n';
                    return 1;
                }
            }
            for (const auto& edge : graph.edges)
            {
                ++consistency_checks;
                const double from = std::max(
                    std::max(dual.At(edge.u, mask), anchor_dual.At(edge.u, mask)),
                    dynamic_dual.At(edge.u, mask));
                const double to = std::max(
                    std::max(dual.At(edge.v, mask), anchor_dual.At(edge.v, mask)),
                    dynamic_dual.At(edge.v, mask));
                if (from > edge.w + to + 1e-8 || to > edge.w + from + 1e-8)
                {
                    std::cout << "CONSISTENCY_FAIL seed=" << seed
                              << " iteration=" << iteration
                              << " mask=" << mask
                              << " edge=" << edge.id << '\n';
                    return 1;
                }
            }
        }

        const int g = static_cast<int>(query.groups.size());
        for (int to = 1; to <= graph.n; ++to)
        {
            std::vector<std::vector<double>> bridge_distance = group_distance;
            bridge_distance.push_back(std::vector<double>(graph.n + 1, gst::fp::kInf));
            std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
            bridge_distance.back()[to] = 0.0;
            heap.push({0.0, to});
            while (!heap.empty())
            {
                const auto [d, u] = heap.top();
                heap.pop();
                if (d != bridge_distance.back()[u])
                    continue;
                for (const auto& edge : graph.adj[u])
                {
                    const double next = d + edge.w;
                    if (next < bridge_distance.back()[edge.to])
                    {
                        bridge_distance.back()[edge.to] = next;
                        heap.push({next, edge.to});
                    }
                }
            }
            const auto exact_bridge = ExactRootedDp(graph, bridge_distance);
            const int to_bit = 1 << g;
            for (int mask = 1; mask < subset_count; ++mask)
            {
                for (int submask = mask;; submask = (submask - 1) & mask)
                {
                    const int added = mask ^ submask;
                    for (int from = 1; from <= graph.n; ++from)
                    {
                        ++splice_checks;
                        const double larger = std::max(
                            std::max(dual.At(from, mask), anchor_dual.At(from, mask)),
                            dynamic_dual.At(from, mask));
                        const double smaller = std::max(
                            std::max(dual.At(to, submask), anchor_dual.At(to, submask)),
                            dynamic_dual.At(to, submask));
                        const double bridge = exact_bridge[to_bit | added][from];
                        if (larger > smaller + bridge + 1e-8)
                        {
                            std::cout << std::setprecision(17)
                                      << "SPLICE_FAIL seed=" << seed
                                      << " iteration=" << iteration
                                      << " mask=" << mask
                                      << " submask=" << submask
                                      << " from=" << from
                                      << " to=" << to
                                      << " larger=" << larger
                                      << " smaller=" << smaller
                                      << " bridge=" << bridge << '\n';
                            return 1;
                        }
                    }
                    if (!submask)
                        break;
                }
            }
        }
    }
    std::cout << "ALL_OK seed=" << seed
              << " iterations=" << iterations
              << " objective_checks=" << objective_checks
              << " admissibility_checks=" << admissibility_checks
              << " consistency_checks=" << consistency_checks
              << " splice_checks=" << splice_checks << '\n';
    return 0;
}

struct PairRun
{
    long long settled = 0;
    long long pushes = 0;
    double ms = 0.0;
};

PairRun ReplayPairs(const gst::Graph& graph,
                    const std::vector<std::vector<double>>& group_distance,
                    const gst::methods::test19::MetricTspLowerBound& tsp,
                    const DualCutPotential* dual,
                    const DualCutPotential* second_dual,
                    double best)
{
    const auto begin = Clock::now();
    const int g = static_cast<int>(group_distance.size());
    const int full_mask = (1 << g) - 1;
    std::vector<double> distance(graph.n + 1, gst::fp::kInf);
    std::vector<double> heuristic(graph.n + 1);
    std::vector<int> touched;

    PairRun result;
    for (int a = 0; a < g; ++a)
    {
        for (int b = a + 1; b < g; ++b)
        {
            const int remaining = full_mask ^ (1 << a) ^ (1 << b);
            for (int v = 1; v <= graph.n; ++v)
            {
                heuristic[v] = tsp.TourHalf(remaining, v, group_distance);
                if (dual)
                    heuristic[v] = std::max(heuristic[v], dual->At(v, remaining));
                if (second_dual)
                    heuristic[v] = std::max(heuristic[v], second_dual->At(v, remaining));
            }

            std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
            touched.clear();
            for (int v = 1; v <= graph.n; ++v)
            {
                const double seed = group_distance[a][v] + group_distance[b][v];
                if (seed + heuristic[v] > best)
                    continue;
                distance[v] = seed;
                touched.push_back(v);
                heap.push({seed + heuristic[v], v});
                ++result.pushes;
            }
            while (!heap.empty())
            {
                const auto [key, u] = heap.top();
                heap.pop();
                if (key != distance[u] + heuristic[u] || key > best)
                    continue;
                ++result.settled;
                for (const auto& edge : graph.adj[u])
                {
                    const double next = distance[u] + edge.w;
                    if (next >= distance[edge.to] || next + heuristic[edge.to] > best)
                        continue;
                    if (distance[edge.to] == gst::fp::kInf)
                        touched.push_back(edge.to);
                    distance[edge.to] = next;
                    heap.push({next + heuristic[edge.to], edge.to});
                    ++result.pushes;
                }
            }
            for (int v : touched)
                distance[v] = gst::fp::kInf;
        }
    }
    result.ms = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return result;
}

void DatasetProbe(const gst::Graph& graph,
                  const gst::Query& query,
                  int query_index,
                  bool run_current,
                  bool use_anchor,
                  bool use_dynamic,
                  bool objective_only)
{
    const auto distance_begin = Clock::now();
    const auto group_distance = GroupDistances(graph, query);
    const double group_distance_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - distance_begin).count();
    const int g = static_cast<int>(query.groups.size());
    const int full_mask = (1 << g) - 1;
    int root = 1;
    double best = RootStar(group_distance, graph.n, root);
    std::vector<int> color(graph.n + 1);
    for (int group = 0; group < g; ++group)
        for (int v : query.groups[group])
            color[v] |= 1 << group;
    best = std::min(best, GreedyUpper(graph, color, full_mask, root, best));

    gst::methods::test19::MetricTspLowerBound tsp;
    const auto tsp_begin = Clock::now();
    tsp.Build(GroupMetric(query, group_distance), full_mask);
    const double tsp_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - tsp_begin).count();

    const auto dual_begin = Clock::now();
    DualCutPotential dual;
    dual.Build(graph, query, group_distance, root);
    const double dual_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - dual_begin).count();

    if (objective_only)
    {
        std::cout << std::fixed << std::setprecision(10)
                  << "dual_objective query=" << query_index
                  << " n=" << graph.n
                  << " m=" << graph.m
                  << " g=" << g
                  << " root=" << root
                  << " best=" << best
                  << " dual_full_root=" << dual.Objective()
                  << " dual_primal_upper=" << dual.PrimalUpper()
                  << " group_distance_ms=" << group_distance_ms
                  << " tsp_ms=" << tsp_ms
                  << " dual_ms=" << dual_ms << '\n';
        return;
    }

    int anchor_group = 0;
    for (int group = 1; group < g; ++group)
        if (query.groups[group].size() < query.groups[anchor_group].size())
            anchor_group = group;
    const auto anchor_begin = Clock::now();
    DualCutPotential second_dual;
    if (use_anchor)
        second_dual.BuildFromGroup(graph, query, group_distance, anchor_group);
    else if (use_dynamic)
        second_dual.BuildDynamic(graph, query, group_distance, root);
    const bool second_enabled = use_anchor || use_dynamic;
    const double second_ms = second_enabled
                                 ? std::chrono::duration<double, std::milli>(
                                       Clock::now() - anchor_begin).count()
                                 : 0.0;

    long long stronger_vertices = 0;
    long long compared_vertices = 0;
    double gain_sum = 0.0;
    for (int a = 0; a < g; ++a)
    {
        for (int b = a + 1; b < g; ++b)
        {
            const int remaining = full_mask ^ (1 << a) ^ (1 << b);
            for (int v = 1; v <= graph.n; ++v)
            {
                const double current = tsp.TourHalf(remaining, v, group_distance);
                double candidate = dual.At(v, remaining);
                if (second_enabled)
                    candidate = std::max(candidate, second_dual.At(v, remaining));
                ++compared_vertices;
                if (candidate > current + 1e-12)
                {
                    ++stronger_vertices;
                    gain_sum += candidate - current;
                }
            }
        }
    }

    PairRun current;
    if (run_current)
        current = ReplayPairs(graph, group_distance, tsp, nullptr, nullptr, best);
    const PairRun hybrid =
        ReplayPairs(graph,
                    group_distance,
                    tsp,
                    &dual,
                    second_enabled ? &second_dual : nullptr,
                    best);
    std::cout << std::fixed << std::setprecision(10)
              << "dual_cut query=" << query_index
              << " n=" << graph.n
              << " m=" << graph.m
              << " g=" << g
              << " root=" << root
              << " best=" << best
              << " tsp_full_root=" << tsp.TourHalf(full_mask, root, group_distance)
              << " dual_full_root=" << dual.Objective()
              << " dual_primal_upper=" << dual.PrimalUpper()
              << " anchor_enabled=" << use_anchor
              << " anchor_group=" << (use_anchor ? anchor_group : -1)
              << " anchor_dual_objective="
              << (use_anchor ? second_dual.Objective() : -1.0)
              << " dynamic_enabled=" << use_dynamic
              << " dynamic_dual_objective="
              << (use_dynamic ? second_dual.Objective() : -1.0)
              << " dynamic_primal_upper="
              << (use_dynamic ? second_dual.PrimalUpper() : -1.0)
              << " group_distance_ms=" << group_distance_ms
              << " tsp_ms=" << tsp_ms
              << " dual_ms=" << dual_ms
              << " second_dual_ms=" << second_ms
              << " stronger_vertices=" << stronger_vertices
              << " compared_vertices=" << compared_vertices
              << " stronger_pct="
              << (compared_vertices ? 100.0 * stronger_vertices / compared_vertices : 0.0)
              << " avg_positive_gain="
              << (stronger_vertices ? gain_sum / stronger_vertices : 0.0)
              << " current_settled=" << (run_current ? current.settled : -1)
              << " hybrid_settled=" << hybrid.settled
              << " pruned=" << (run_current ? current.settled - hybrid.settled : -1)
              << " pruned_pct="
              << (run_current && current.settled
                      ? 100.0 * (current.settled - hybrid.settled) / current.settled
                      : 0.0)
              << " current_pushes=" << (run_current ? current.pushes : -1)
              << " hybrid_pushes=" << hybrid.pushes
              << " current_ms=" << current.ms
              << " hybrid_ms=" << hybrid.ms
              << " order=";
    for (size_t i = 0; i < dual.Order().size(); ++i)
        std::cout << (i ? "," : "") << dual.Order()[i];
    std::cout << '\n';
}
}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc >= 2 && std::string(argv[1]) == "--self-check")
        {
            const std::uint64_t seed = argc >= 3 ? std::stoull(argv[2]) : 1;
            const int iterations = argc >= 4 ? std::stoi(argv[3]) : 100;
            const int max_g = argc >= 5 ? std::stoi(argv[4]) : 7;
            const int max_n = argc >= 6 ? std::stoi(argv[5]) : 10;
            return SelfCheck(seed, iterations, max_g, max_n);
        }
        const bool hybrid_only = argc >= 2 && std::string(argv[1]) == "--hybrid-only";
        const bool use_anchor = argc >= 2 && std::string(argv[1]) == "--anchor";
        const bool use_dynamic = argc >= 2 && std::string(argv[1]) == "--dynamic";
        const bool objective_only =
            argc >= 2 && std::string(argv[1]) == "--objective-only";
        const int offset = hybrid_only || use_anchor || use_dynamic || objective_only ? 1 : 0;
        if (argc < 4 + offset || argc > 5 + offset)
        {
            std::cerr << "usage: " << argv[0]
                      << " <data_root> <graph_selector> <query_selector>"
                      << " [query_index_1based=1]\n"
                      << "   or: " << argv[0]
                      << " --hybrid-only <data_root> <graph_selector> <query_selector>"
                      << " [query_index_1based=1]\n"
                      << "   or: " << argv[0]
                      << " --anchor <data_root> <graph_selector> <query_selector>"
                      << " [query_index_1based=1]\n"
                      << "   or: " << argv[0]
                      << " --dynamic <data_root> <graph_selector> <query_selector>"
                      << " [query_index_1based=1]\n"
                      << "   or: " << argv[0]
                      << " --objective-only <data_root> <graph_selector> <query_selector>"
                      << " [query_index_1based=1]\n"
                      << "   or: " << argv[0]
                      << " --self-check [seed=1] [iterations=100] [max_g=7] [max_n=10]\n";
            return 2;
        }
        const std::string graph_folder =
            gst::ResolveGraphFolder(argv[1 + offset], argv[2 + offset]);
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const auto queries = gst::LoadQueriesFromFolder(graph_folder, argv[3 + offset]);
        const int query_index = argc >= 5 + offset ? std::stoi(argv[4 + offset]) : 1;
        if (query_index < 1 || query_index > static_cast<int>(queries.size()))
            throw std::runtime_error("query index out of range");
        DatasetProbe(
            graph,
            queries[query_index - 1],
            query_index,
            !hybrid_only,
            use_anchor,
            use_dynamic,
            objective_only);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
