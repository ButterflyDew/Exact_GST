#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "float_compare.h"
#include "graph_io.h"
#include "methods/DPBF/dpbf_solver.h"
#include "query_io.h"

namespace
{
using gst::Graph;
using gst::Query;
using Item = std::pair<double, int>;
using MinHeap = std::priority_queue<Item, std::vector<Item>, std::greater<Item>>;

struct Result
{
    double best = gst::fp::kInf;
    long long settled = 0;
    long long edge_relaxations = 0;
};

int BitCount(int mask)
{
    int count = 0;
    for (; mask; mask &= mask - 1)
        ++count;
    return count;
}

Result Solve(const Graph& graph, const Query& query)
{
    const int g = static_cast<int>(query.groups.size());
    if (g < 1 || g > 20)
        throw std::runtime_error("goal-root-star supports 1 to 20 groups");
    if (g == 1)
        return {0.0, 0, 0};

    const int full = (1 << g) - 1;
    std::vector<std::vector<double>> distance(
        g, std::vector<double>(graph.n + 1, gst::fp::kInf));
    std::vector<MinHeap> frontier(g);
    for (int group = 0; group < g; ++group)
    {
        for (int vertex : query.groups[group])
        {
            if (distance[group][vertex] == 0.0)
                continue;
            distance[group][vertex] = 0.0;
            frontier[group].push({0.0, vertex});
        }
    }

    std::vector<int> settled_mask(graph.n + 1);
    std::vector<double> settled_sum(graph.n + 1);
    std::vector<int> mask_count(1 << g);
    mask_count[0] = graph.n;
    std::vector<MinHeap> base_by_mask(1 << g);
    Result result;
    std::vector<int> terminal_mask(graph.n + 1);
    for (int group = 0; group < g; ++group)
        for (int vertex : query.groups[group])
            terminal_mask[vertex] |= 1 << group;
    for (const auto& group : query.groups)
        for (int vertex : group)
            if (terminal_mask[vertex] == full)
                result.best = 0.0;
    std::vector<std::vector<double>> group_metric(
        g, std::vector<double>(g, gst::fp::kInf));
    for (auto& row : group_metric)
        std::fill(row.begin(), row.end(), gst::fp::kInf);
    for (int group = 0; group < g; ++group)
        group_metric[group][group] = 0.0;

    auto MetricLower = [&]()
    {
        if (g == 2)
            return group_metric[0][1] < gst::fp::kInf / 4 ? group_metric[0][1] : 0.0;
        if (g != 3)
            return 0.0;
        if (group_metric[0][1] >= gst::fp::kInf / 4 ||
            group_metric[0][2] >= gst::fp::kInf / 4 ||
            group_metric[1][2] >= gst::fp::kInf / 4)
            return 0.0;
        return 0.5 * (group_metric[0][1] + group_metric[0][2] +
                      group_metric[1][2]);
    };

    auto CleanFrontier = [&](int group)
    {
        auto& heap = frontier[group];
        while (!heap.empty())
        {
            const auto [value, vertex] = heap.top();
            if (value == distance[group][vertex] && !(settled_mask[vertex] & (1 << group)))
                break;
            heap.pop();
        }
    };

    auto MinBase = [&](int mask)
    {
        if (!mask_count[mask])
            return gst::fp::kInf;
        if (!mask)
            return 0.0;
        auto& heap = base_by_mask[mask];
        while (!heap.empty())
        {
            const auto [value, vertex] = heap.top();
            if (settled_mask[vertex] == mask && value == settled_sum[vertex])
                return value;
            heap.pop();
        }
        return gst::fp::kInf;
    };

    while (true)
    {
        std::vector<double> minimum(g, gst::fp::kInf);
        int next_group = -1;
        for (int group = 0; group < g; ++group)
        {
            CleanFrontier(group);
            if (frontier[group].empty())
                continue;
            minimum[group] = frontier[group].top().first;
            if (next_group < 0 || minimum[group] < minimum[next_group])
                next_group = group;
        }

        double global_lower = 0.0;
        if (result.best < gst::fp::kInf / 4)
        {
            global_lower = gst::fp::kInf;
            for (int mask = 0; mask <= full; ++mask)
            {
                double lower = MinBase(mask);
                if (lower >= gst::fp::kInf / 4)
                    continue;
                for (int group = 0; group < g; ++group)
                    if (!(mask & (1 << group)))
                        lower += minimum[group];
                global_lower = std::min(global_lower, lower);
            }
            global_lower = std::max(global_lower, MetricLower());
        }
        if (global_lower + gst::fp::kEps >= result.best || next_group < 0)
            break;

        const auto [value, vertex] = frontier[next_group].top();
        frontier[next_group].pop();
        if (value != distance[next_group][vertex] ||
            (settled_mask[vertex] & (1 << next_group)))
            continue;

        const int old_mask = settled_mask[vertex];
        --mask_count[old_mask];
        settled_mask[vertex] |= 1 << next_group;
        settled_sum[vertex] += value;
        const int new_mask = settled_mask[vertex];
        ++mask_count[new_mask];
        base_by_mask[new_mask].push({settled_sum[vertex], vertex});
        for (int bits = terminal_mask[vertex]; bits; bits &= bits - 1)
        {
            int target = 0;
            int bit = bits & -bits;
            while ((1 << target) != bit)
                ++target;
            group_metric[next_group][target] =
                std::min(group_metric[next_group][target], value);
            group_metric[target][next_group] = group_metric[next_group][target];
        }
        ++result.settled;
        if (new_mask == full)
            result.best = std::min(result.best, settled_sum[vertex]);

        for (const auto& edge : graph.adj[vertex])
        {
            ++result.edge_relaxations;
            const double next = value + edge.w;
            if (next < distance[next_group][edge.to])
            {
                distance[next_group][edge.to] = next;
                frontier[next_group].push({next, edge.to});
            }
        }
    }
    return result;
}

double GreedyUpper(const Graph& graph, const Query& query)
{
    const int g = static_cast<int>(query.groups.size());
    const int full = (1 << g) - 1;
    std::vector<int> color(graph.n + 1);
    int start = 0;
    for (int group = 0; group < g; ++group)
    {
        for (int vertex : query.groups[group])
        {
            color[vertex] |= 1 << group;
            if (!start || BitCount(color[vertex]) > BitCount(color[start]) ||
                (BitCount(color[vertex]) == BitCount(color[start]) && vertex < start))
                start = vertex;
        }
    }

    std::vector<char> in_tree(graph.n + 1);
    std::vector<int> parent(graph.n + 1);
    std::vector<double> distance(graph.n + 1, gst::fp::kInf);
    std::vector<int> sources{start};
    in_tree[start] = 1;
    int covered = color[start];
    double answer = 0.0;
    while (covered != full)
    {
        std::fill(distance.begin(), distance.end(), gst::fp::kInf);
        std::fill(parent.begin(), parent.end(), 0);
        MinHeap heap;
        for (int source : sources)
        {
            distance[source] = 0.0;
            heap.push({0.0, source});
        }
        int hit = 0;
        while (!heap.empty())
        {
            const auto [value, vertex] = heap.top();
            heap.pop();
            if (value != distance[vertex])
                continue;
            if (color[vertex] & (full ^ covered))
            {
                hit = vertex;
                answer += value;
                break;
            }
            for (const auto& edge : graph.adj[vertex])
            {
                const double next = value + edge.w;
                if (next < distance[edge.to])
                {
                    distance[edge.to] = next;
                    parent[edge.to] = vertex;
                    heap.push({next, edge.to});
                }
            }
        }
        if (!hit)
            return gst::fp::kInf;
        for (int vertex = hit; !in_tree[vertex]; vertex = parent[vertex])
        {
            in_tree[vertex] = 1;
            sources.push_back(vertex);
            covered |= color[vertex];
        }
    }
    return answer;
}

void ProbeTruncatedDistances(const Graph& graph, const Query& query)
{
    const auto begin = std::chrono::steady_clock::now();
    const double upper = GreedyUpper(graph, query);
    long long settled = 0;
    long long edge_relaxations = 0;
    for (const auto& group : query.groups)
    {
        std::vector<double> distance(graph.n + 1, gst::fp::kInf);
        MinHeap heap;
        for (int vertex : group)
        {
            distance[vertex] = 0.0;
            heap.push({0.0, vertex});
        }
        while (!heap.empty())
        {
            const auto [value, vertex] = heap.top();
            heap.pop();
            if (value != distance[vertex])
                continue;
            if (value + gst::fp::kEps >= upper)
                break;
            ++settled;
            for (const auto& edge : graph.adj[vertex])
            {
                ++edge_relaxations;
                const double next = value + edge.w;
                if (next + gst::fp::kEps < upper && next < distance[edge.to])
                {
                    distance[edge.to] = next;
                    heap.push({next, edge.to});
                }
            }
        }
    }
    const double wall = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - begin).count();
    const long long full_scan =
        2LL * static_cast<long long>(query.groups.size()) * graph.m;
    std::cout << std::fixed << std::setprecision(10)
              << "upper=" << upper
              << " wall_sec=" << wall
              << " settled=" << settled
              << " edge_relaxations=" << edge_relaxations
              << " full_scan=" << full_scan
              << " scan_ratio="
              << (full_scan ? static_cast<double>(edge_relaxations) / full_scan : 0.0)
              << '\n';
}

struct PairDistance
{
    double value = gst::fp::kInf;
    long long settled = 0;
    long long edge_relaxations = 0;
};

PairDistance BidirectionalGroupDistance(const Graph& graph,
                                        const std::vector<int>& first,
                                        const std::vector<int>& second)
{
    std::array<std::vector<double>, 2> distance{
        std::vector<double>(graph.n + 1, gst::fp::kInf),
        std::vector<double>(graph.n + 1, gst::fp::kInf)};
    std::array<std::vector<char>, 2> settled{
        std::vector<char>(graph.n + 1), std::vector<char>(graph.n + 1)};
    std::array<MinHeap, 2> frontier;
    const std::array<const std::vector<int>*, 2> sources{&first, &second};
    for (int side = 0; side < 2; ++side)
        for (int vertex : *sources[side])
            if (distance[side][vertex] != 0.0)
            {
                distance[side][vertex] = 0.0;
                frontier[side].push({0.0, vertex});
            }

    PairDistance result;
    auto Clean = [&](int side)
    {
        while (!frontier[side].empty())
        {
            const auto [value, vertex] = frontier[side].top();
            if (value == distance[side][vertex] && !settled[side][vertex])
                break;
            frontier[side].pop();
        }
    };

    while (true)
    {
        Clean(0);
        Clean(1);
        if (frontier[0].empty() || frontier[1].empty() ||
            frontier[0].top().first + frontier[1].top().first + gst::fp::kEps >=
                result.value)
            break;
        const int side = frontier[1].top().first < frontier[0].top().first ? 1 : 0;
        const auto [value, vertex] = frontier[side].top();
        frontier[side].pop();
        settled[side][vertex] = 1;
        ++result.settled;
        result.value = std::min(result.value, value + distance[side ^ 1][vertex]);
        for (const auto& edge : graph.adj[vertex])
        {
            ++result.edge_relaxations;
            const double next = value + edge.w;
            if (next < distance[side][edge.to])
            {
                distance[side][edge.to] = next;
                frontier[side].push({next, edge.to});
            }
            result.value =
                std::min(result.value, next + distance[side ^ 1][edge.to]);
        }
    }
    return result;
}

void ProbePairMetric(const Graph& graph, const Query& query)
{
    const auto begin = std::chrono::steady_clock::now();
    long long settled = 0;
    long long edge_relaxations = 0;
    double checksum = 0.0;
    for (int first = 0; first < static_cast<int>(query.groups.size()); ++first)
        for (int second = first + 1; second < static_cast<int>(query.groups.size()); ++second)
        {
            const PairDistance pair =
                BidirectionalGroupDistance(graph, query.groups[first], query.groups[second]);
            checksum += pair.value;
            settled += pair.settled;
            edge_relaxations += pair.edge_relaxations;
        }
    const double wall = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - begin).count();
    std::cout << std::fixed << std::setprecision(10)
              << "pairs=" << query.groups.size() * (query.groups.size() - 1) / 2
              << " checksum=" << checksum
              << " settled=" << settled
              << " edge_relaxations=" << edge_relaxations
              << " wall_sec=" << wall << '\n';
}

void AddEdge(Graph& graph, int u, int v, double weight)
{
    const int id = static_cast<int>(graph.edges.size());
    graph.edges.push_back({id, u, v, weight});
    graph.adj[u].push_back({v, id, weight});
    graph.adj[v].push_back({u, id, weight});
    graph.m = static_cast<int>(graph.edges.size());
}

Graph RandomGraph(std::mt19937_64& rng, int n)
{
    Graph graph;
    graph.n = n;
    graph.adj.assign(n + 1, {});
    std::uniform_int_distribution<int> weight(1, 30);
    for (int vertex = 2; vertex <= n; ++vertex)
    {
        std::uniform_int_distribution<int> parent(1, vertex - 1);
        AddEdge(graph, vertex, parent(rng), weight(rng));
    }
    std::bernoulli_distribution extra(0.3);
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
    std::uniform_int_distribution<int> size(1, std::min(3, n));
    for (auto& group : query.groups)
    {
        const int target = size(rng);
        while (static_cast<int>(group.size()) < target)
        {
            const int candidate = vertex(rng);
            if (std::find(group.begin(), group.end(), candidate) == group.end())
                group.push_back(candidate);
        }
    }
    return query;
}

int Usage()
{
    std::cerr << "Usage:\n"
              << "  gst_goal_root_star_probe <graph_folder> <query_selector> [query_index=1]\n"
              << "  gst_goal_root_star_probe --truncated <graph_folder> <query_selector>"
              << " [query_index=1]\n"
              << "  gst_goal_root_star_probe --pair-metric <graph_folder> <query_selector>"
              << " [query_index=1]\n"
              << "  gst_goal_root_star_probe --self-check [seed=1] [iterations=1000]"
              << " [min_n=4] [max_n=14]\n";
    return 2;
}
}  // namespace

int main(int argc, char** argv)
{
    if (argc >= 2 && std::string(argv[1]) == "--pair-metric")
    {
        if (argc < 4 || argc > 5)
            return Usage();
        const Graph graph = gst::LoadGraphFromFolder(argv[2]);
        const auto queries = gst::LoadQueriesFromFolder(argv[2], argv[3]);
        const int index = argc > 4 ? std::atoi(argv[4]) : 1;
        if (index < 1 || index > static_cast<int>(queries.size()))
            return Usage();
        ProbePairMetric(graph, queries[index - 1]);
        return 0;
    }

    if (argc >= 2 && std::string(argv[1]) == "--truncated")
    {
        if (argc < 4 || argc > 5)
            return Usage();
        const Graph graph = gst::LoadGraphFromFolder(argv[2]);
        const auto queries = gst::LoadQueriesFromFolder(argv[2], argv[3]);
        const int index = argc > 4 ? std::atoi(argv[4]) : 1;
        if (index < 1 || index > static_cast<int>(queries.size()))
            return Usage();
        ProbeTruncatedDistances(graph, queries[index - 1]);
        return 0;
    }

    if (argc >= 2 && std::string(argv[1]) == "--self-check")
    {
        const std::uint64_t seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1;
        const int iterations = argc > 3 ? std::atoi(argv[3]) : 1000;
        const int min_n = argc > 4 ? std::atoi(argv[4]) : 4;
        const int max_n = argc > 5 ? std::atoi(argv[5]) : 14;
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<int> n_dist(min_n, max_n);
        std::uniform_int_distribution<int> g_dist(2, 3);
        for (int iteration = 1; iteration <= iterations; ++iteration)
        {
            Graph graph = RandomGraph(rng, n_dist(rng));
            Query query = RandomQuery(rng, graph.n, g_dist(rng));
            const auto exact = gst::methods::dpbf::SolveOneQuery(graph, query);
            const Result result = Solve(graph, query);
            if (!exact.feasible || std::abs(result.best - exact.best_weight) > 1e-6)
            {
                std::cout << "MISMATCH seed=" << seed << " iteration=" << iteration
                          << " exact=" << exact.best_weight << " got=" << result.best << '\n';
                return 1;
            }
            if (iteration % 100 == 0)
                std::cout << "ok " << iteration << '\n';
        }
        std::cout << "ALL_OK seed=" << seed << " iterations=" << iterations << '\n';
        return 0;
    }

    if (argc < 3 || argc > 4)
        return Usage();
    const Graph graph = gst::LoadGraphFromFolder(argv[1]);
    const auto queries = gst::LoadQueriesFromFolder(argv[1], argv[2]);
    const int index = argc > 3 ? std::atoi(argv[3]) : 1;
    if (index < 1 || index > static_cast<int>(queries.size()))
        return Usage();

    const auto begin = std::chrono::steady_clock::now();
    const Result result = Solve(graph, queries[index - 1]);
    const double wall = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - begin).count();
    std::cout << std::fixed << std::setprecision(10)
              << "best=" << result.best
              << " wall_sec=" << wall
              << " settled=" << result.settled
              << " edge_relaxations=" << result.edge_relaxations << '\n';
    return 0;
}
