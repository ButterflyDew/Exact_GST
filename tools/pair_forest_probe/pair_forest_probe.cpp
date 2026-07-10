#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
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
#include "methods/Test/test19_future_bounds.h"
#include "query_io.h"

namespace
{
using Clock = std::chrono::steady_clock;
using HeapItem = std::pair<double, int>;
using Heap = std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>>;

struct QueueNode
{
    double key = 0.0;
    double distance = 0.0;
    int vertex = 0;
};

struct QueueGreater
{
    bool operator()(const QueueNode& left, const QueueNode& right) const
    {
        if (left.key != right.key)
            return left.key > right.key;
        if (left.distance != right.distance)
            return left.distance > right.distance;
        return left.vertex > right.vertex;
    }
};

struct RowStats
{
    int mask = 0;
    long long settled = 0;
    long long roots = 0;
    long long queue_pushes = 0;
    long long closure_errors = 0;
    long long decode_errors = 0;
    long long current_bytes = 0;
    long long certificate_bytes = 0;
    long long depth_sum = 0;
    int max_depth = 0;
    double search_ms = 0.0;
    double decode_ms = 0.0;
    double max_decode_error = 0.0;
};

int FirstBit(int mask)
{
    int bit = 0;
    while (!((mask >> bit) & 1))
        ++bit;
    return bit;
}

double Percent(long long part, long long whole)
{
    return whole ? 100.0 * static_cast<double>(part) / static_cast<double>(whole) : 0.0;
}

std::vector<std::vector<double>> ComputeGroupDistances(const gst::Graph& graph,
                                                       const gst::Query& query)
{
    std::vector<std::vector<double>> distance(
        query.groups.size(), std::vector<double>(graph.n + 1, gst::fp::kInf));
    for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
    {
        Heap heap;
        for (int v : query.groups[group])
        {
            distance[group][v] = 0.0;
            heap.push({0.0, v});
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

std::vector<std::vector<double>> BuildGroupMetric(
    const gst::Query& query,
    const std::vector<std::vector<double>>& group_distance)
{
    const int g = static_cast<int>(query.groups.size());
    std::vector<std::vector<double>> metric(g, std::vector<double>(g, gst::fp::kInf));
    for (int a = 0; a < g; ++a)
        for (int b = 0; b < g; ++b)
            for (int v : query.groups[b])
                metric[a][b] = std::min(metric[a][b], group_distance[a][v]);
    return metric;
}

std::vector<double> SingleSourceDistances(const gst::Graph& graph, int source)
{
    std::vector<double> distance(graph.n + 1, gst::fp::kInf);
    Heap heap;
    distance[source] = 0.0;
    heap.push({0.0, source});
    while (!heap.empty())
    {
        const auto [d, u] = heap.top();
        heap.pop();
        if (d != distance[u])
            continue;
        for (const auto& edge : graph.adj[u])
        {
            const double next = d + edge.w;
            if (next < distance[edge.to])
            {
                distance[edge.to] = next;
                heap.push({next, edge.to});
            }
        }
    }
    return distance;
}

void ProbePairPartitionUpper(const gst::Graph& graph,
                             const gst::Query& query,
                             int query_id)
{
    const int g = static_cast<int>(query.groups.size());
    if (g < 2 || g > 20)
        throw std::runtime_error("pair partition upper probe requires 2 <= g <= 20");

    const auto begin = Clock::now();
    const auto group_distance = ComputeGroupDistances(graph, query);
    int root = 1;
    double star_upper = gst::fp::kInf;
    for (int v = 1; v <= graph.n; ++v)
    {
        double value = 0.0;
        for (int a = 0; a < g; ++a)
            value += group_distance[a][v];
        if (value < star_upper)
        {
            star_upper = value;
            root = v;
        }
    }

    const auto root_distance = SingleSourceDistances(graph, root);
    std::vector<std::vector<double>> pair_cost(g, std::vector<double>(g, gst::fp::kInf));
    for (int v = 1; v <= graph.n; ++v)
    {
        for (int a = 0; a < g; ++a)
        {
            for (int b = a + 1; b < g; ++b)
            {
                pair_cost[a][b] = std::min(
                    pair_cost[a][b],
                    group_distance[a][v] + group_distance[b][v] + root_distance[v]);
            }
        }
    }

    const int subset_count = 1 << g;
    std::vector<double> partition(subset_count, gst::fp::kInf);
    partition[0] = 0.0;
    for (int mask = 1; mask < subset_count; ++mask)
    {
        const int a = FirstBit(mask);
        const int without_a = mask ^ (1 << a);
        partition[mask] = group_distance[a][root] + partition[without_a];
        for (int bits = without_a; bits; bits &= bits - 1)
        {
            const int b = FirstBit(bits & -bits);
            partition[mask] = std::min(
                partition[mask], pair_cost[std::min(a, b)][std::max(a, b)] +
                                     partition[without_a ^ (1 << b)]);
        }
    }

    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    std::cout << std::fixed << std::setprecision(10)
              << "pair_partition_upper query=" << query_id
              << " n=" << graph.n
              << " m=" << graph.m
              << " g=" << g
              << " root=" << root
              << " star_upper=" << star_upper
              << " pair_upper=" << partition.back()
              << " improvement=" << (star_upper - partition.back())
              << " improvement_pct="
              << (star_upper > 0.0 ? 100.0 * (star_upper - partition.back()) / star_upper
                                   : 0.0)
              << " total_ms=" << elapsed_ms << '\n';
}

int ParentVertex(const gst::Graph& graph, int vertex, int parent_code)
{
    const auto& edge = graph.edges[parent_code - 1];
    if (edge.u == vertex)
        return edge.v;
    if (edge.v == vertex)
        return edge.u;
    return -1;
}

RowStats ProbePair(const gst::Graph& graph,
                   int pair_mask,
                   double known_optimum,
                   const std::vector<std::vector<double>>& group_distance,
                   const gst::methods::test19::MetricTspLowerBound& tsp)
{
    constexpr int kAbsent = -1;
    constexpr int kRoot = 0;
    const int a = FirstBit(pair_mask);
    const int b = FirstBit(pair_mask ^ (1 << a));
    const int full_mask = (1 << group_distance.size()) - 1;
    const int remaining = full_mask ^ pair_mask;
    std::vector<double> distance(graph.n + 1, gst::fp::kInf);
    std::vector<double> heuristic(graph.n + 1);
    std::vector<int> parent(graph.n + 1, kAbsent);
    std::vector<char> settled(graph.n + 1);
    std::priority_queue<QueueNode, std::vector<QueueNode>, QueueGreater> queue;
    RowStats stats;
    stats.mask = pair_mask;

    const auto search_begin = Clock::now();
    for (int v = 1; v <= graph.n; ++v)
    {
        heuristic[v] = tsp.TourHalf(remaining, v, group_distance);
        const double seed = group_distance[a][v] + group_distance[b][v];
        if (seed + heuristic[v] <= known_optimum + gst::fp::kEps)
        {
            distance[v] = seed;
            parent[v] = kRoot;
            queue.push({seed + heuristic[v], seed, v});
            ++stats.queue_pushes;
        }
    }

    while (!queue.empty())
    {
        const QueueNode current = queue.top();
        queue.pop();
        if (current.distance != distance[current.vertex] ||
            current.key > known_optimum + gst::fp::kEps)
            continue;
        if (!settled[current.vertex])
        {
            settled[current.vertex] = 1;
            ++stats.settled;
        }
        for (const auto& edge : graph.adj[current.vertex])
        {
            const double next = current.distance + edge.w;
            if (next >= distance[edge.to] ||
                next + heuristic[edge.to] > known_optimum + gst::fp::kEps)
                continue;
            distance[edge.to] = next;
            parent[edge.to] = edge.edge_id + 1;
            queue.push({next + heuristic[edge.to], next, edge.to});
            ++stats.queue_pushes;
        }
    }
    stats.search_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - search_begin).count();

    for (int v = 1; v <= graph.n; ++v)
    {
        if (!settled[v])
        {
            parent[v] = kAbsent;
            continue;
        }
        if (parent[v] == kRoot)
        {
            ++stats.roots;
            continue;
        }
        const int p = parent[v] > 0 ? ParentVertex(graph, v, parent[v]) : -1;
        if (p < 1 || !settled[p] ||
            distance[p] + graph.edges[parent[v] - 1].w != distance[v])
            ++stats.closure_errors;
    }

    const long long current_dense = 2LL * sizeof(double) * (graph.n + 1LL);
    const long long current_sparse =
        (sizeof(int) + 2LL * sizeof(double)) * stats.settled;
    stats.current_bytes = std::min(current_dense, current_sparse);
    const long long certificate_dense = sizeof(std::int32_t) * (graph.n + 1LL);
    const long long certificate_sparse =
        2LL * sizeof(std::int32_t) * stats.settled;
    stats.certificate_bytes = std::min(certificate_dense, certificate_sparse);

    const auto decode_begin = Clock::now();
    std::vector<double> decoded(graph.n + 1, gst::fp::kInf);
    std::vector<int> depth(graph.n + 1);
    std::vector<char> active(graph.n + 1);
    std::vector<int> path;
    for (int start = 1; start <= graph.n; ++start)
    {
        if (!settled[start] || decoded[start] < gst::fp::kInf)
            continue;
        path.clear();
        int current = start;
        while (decoded[current] >= gst::fp::kInf)
        {
            if (!settled[current] || parent[current] == kAbsent || active[current])
            {
                ++stats.decode_errors;
                break;
            }
            if (parent[current] == kRoot)
            {
                decoded[current] = group_distance[a][current] + group_distance[b][current];
                depth[current] = 0;
                break;
            }
            active[current] = 1;
            path.push_back(current);
            current = ParentVertex(graph, current, parent[current]);
            if (current < 1)
            {
                ++stats.decode_errors;
                break;
            }
        }
        while (!path.empty())
        {
            const int child = path.back();
            path.pop_back();
            active[child] = 0;
            const int p = ParentVertex(graph, child, parent[child]);
            if (p < 1 || decoded[p] >= gst::fp::kInf)
            {
                ++stats.decode_errors;
                continue;
            }
            decoded[child] = decoded[p] + graph.edges[parent[child] - 1].w;
            depth[child] = depth[p] + 1;
        }
    }

    for (int v = 1; v <= graph.n; ++v)
    {
        if (!settled[v])
            continue;
        if (decoded[v] >= gst::fp::kInf)
        {
            ++stats.decode_errors;
            continue;
        }
        const double error = std::fabs(decoded[v] - distance[v]);
        stats.max_decode_error = std::max(stats.max_decode_error, error);
        if (error > 1e-9 * std::max(1.0, std::fabs(distance[v])))
            ++stats.decode_errors;
        stats.depth_sum += depth[v];
        stats.max_depth = std::max(stats.max_depth, depth[v]);
    }
    stats.decode_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - decode_begin).count();
    return stats;
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
                                                 int min_group_count,
                                                 int max_group_count,
                                                 int max_vertex_count)
{
    std::uniform_int_distribution<int> n_distribution(4, max_vertex_count);
    std::uniform_int_distribution<int> g_distribution(min_group_count, max_group_count);
    std::uniform_int_distribution<int> weight_distribution(1, 30);
    const int n = n_distribution(random);
    const int g = g_distribution(random);

    gst::Graph graph;
    graph.n = n;
    graph.adj.resize(n + 1);
    for (int v = 2; v <= n; ++v)
    {
        std::uniform_int_distribution<int> parent_distribution(1, v - 1);
        AddEdge(graph, v, parent_distribution(random), weight_distribution(random));
    }
    std::bernoulli_distribution extra_edge(0.32);
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
        const int target_size = size_distribution(random);
        while (static_cast<int>(group.size()) < target_size)
        {
            const int v = vertex_distribution(random);
            if (std::find(group.begin(), group.end(), v) == group.end())
                group.push_back(v);
        }
    }
    return {std::move(graph), std::move(query)};
}

int RunSelfCheck(std::uint64_t seed,
                 int iterations,
                 int min_group_count,
                 int max_group_count,
                 int max_vertex_count)
{
    std::mt19937_64 random(seed);
    for (int iteration = 1; iteration <= iterations; ++iteration)
    {
        auto [graph, query] =
            RandomInstance(random, min_group_count, max_group_count, max_vertex_count);
        const int g = static_cast<int>(query.groups.size());
        const auto group_distance = ComputeGroupDistances(graph, query);
        const auto metric = BuildGroupMetric(query, group_distance);
        gst::methods::test19::MetricTspLowerBound tsp;
        tsp.Build(metric, (1 << g) - 1);
        double upper = gst::fp::kInf;
        for (int v = 1; v <= graph.n; ++v)
        {
            double star = 0.0;
            for (int a = 0; a < g; ++a)
                star += group_distance[a][v];
            upper = std::min(upper, star);
        }
        for (int a = 0; a < g; ++a)
        {
            for (int b = a + 1; b < g; ++b)
            {
                const RowStats row = ProbePair(
                    graph, (1 << a) | (1 << b), upper, group_distance, tsp);
                if (row.closure_errors || row.decode_errors)
                {
                    std::cout << "SELF_CHECK_FAIL seed=" << seed
                              << " iteration=" << iteration
                              << " g=" << g
                              << " mask=" << row.mask
                              << " closure_errors=" << row.closure_errors
                              << " decode_errors=" << row.decode_errors << '\n';
                    return 1;
                }
            }
        }
        if (iteration % 1000 == 0)
            std::cout << "ok " << iteration << '\n';
    }
    std::cout << "ALL_OK seed=" << seed << " iterations=" << iterations << '\n';
    return 0;
}

void ProbeQuery(const gst::Graph& graph,
                const gst::Query& query,
                int query_id,
                double known_optimum)
{
    const int g = static_cast<int>(query.groups.size());
    if (g < 3 || g > 20)
        throw std::runtime_error("pair forest probe requires 3 <= g <= 20");

    const auto begin = Clock::now();
    const auto group_distance = ComputeGroupDistances(graph, query);
    const auto metric = BuildGroupMetric(query, group_distance);
    gst::methods::test19::MetricTspLowerBound tsp;
    tsp.Build(metric, (1 << g) - 1);

    RowStats total;
    int pair_count = 0;
    for (int a = 0; a < g; ++a)
    {
        for (int b = a + 1; b < g; ++b)
        {
            const RowStats row = ProbePair(
                graph, (1 << a) | (1 << b), known_optimum, group_distance, tsp);
            ++pair_count;
            total.settled += row.settled;
            total.roots += row.roots;
            total.queue_pushes += row.queue_pushes;
            total.closure_errors += row.closure_errors;
            total.decode_errors += row.decode_errors;
            total.current_bytes += row.current_bytes;
            total.certificate_bytes += row.certificate_bytes;
            total.depth_sum += row.depth_sum;
            total.max_depth = std::max(total.max_depth, row.max_depth);
            total.search_ms += row.search_ms;
            total.decode_ms += row.decode_ms;
            total.max_decode_error = std::max(total.max_decode_error, row.max_decode_error);
            std::cout << std::fixed << std::setprecision(6)
                      << "pair_progress=" << pair_count
                      << " mask=" << row.mask
                      << " settled=" << row.settled
                      << " roots=" << row.roots
                      << " current_bytes=" << row.current_bytes
                      << " certificate_bytes=" << row.certificate_bytes
                      << " compression="
                      << (row.certificate_bytes
                              ? static_cast<double>(row.current_bytes) /
                                    static_cast<double>(row.certificate_bytes)
                              : 0.0)
                      << " max_depth=" << row.max_depth
                      << " decode_ms=" << row.decode_ms
                      << " errors=" << (row.closure_errors + row.decode_errors)
                      << std::endl;
        }
    }

    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    std::cout << std::fixed << std::setprecision(6)
              << "pair_forest query=" << query_id
              << " n=" << graph.n
              << " m=" << graph.m
              << " g=" << g
              << " known_optimum=" << known_optimum
              << " pairs=" << pair_count
              << " settled=" << total.settled
              << " roots=" << total.roots
              << " root_pct=" << Percent(total.roots, total.settled)
              << " current_bytes=" << total.current_bytes
              << " certificate_bytes=" << total.certificate_bytes
              << " compression="
              << (total.certificate_bytes
                      ? static_cast<double>(total.current_bytes) /
                            static_cast<double>(total.certificate_bytes)
                      : 0.0)
              << " avg_depth="
              << (total.settled
                      ? static_cast<double>(total.depth_sum) /
                            static_cast<double>(total.settled)
                      : 0.0)
              << " max_depth=" << total.max_depth
              << " queue_pushes=" << total.queue_pushes
              << " closure_errors=" << total.closure_errors
              << " decode_errors=" << total.decode_errors
              << " max_decode_error=" << total.max_decode_error
              << " search_ms=" << total.search_ms
              << " decode_ms=" << total.decode_ms
              << " total_ms=" << elapsed_ms << '\n';
}
}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc >= 2 && std::string(argv[1]) == "--self-check")
        {
            const std::uint64_t seed = argc >= 3 ? std::stoull(argv[2]) : 1;
            const int iterations = argc >= 4 ? std::stoi(argv[3]) : 1000;
            const int min_group_count = argc >= 5 ? std::stoi(argv[4]) : 3;
            const int max_group_count = argc >= 6 ? std::stoi(argv[5]) : 8;
            const int max_vertex_count = argc >= 7 ? std::stoi(argv[6]) : 10;
            return RunSelfCheck(
                seed, iterations, min_group_count, max_group_count, max_vertex_count);
        }
        if (argc >= 2 && std::string(argv[1]) == "--partition-upper")
        {
            if (argc < 5 || argc > 6)
                throw std::runtime_error(
                    "partition upper usage: --partition-upper <data_root> <graph> <query>"
                    " [query_begin_1based=1]");
            const std::string graph_folder = gst::ResolveGraphFolder(argv[2], argv[3]);
            const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
            const std::vector<gst::Query> queries =
                gst::LoadQueriesFromFolder(graph_folder, argv[4]);
            const int query_begin = argc >= 6 ? std::stoi(argv[5]) : 1;
            if (query_begin < 1 || query_begin > static_cast<int>(queries.size()))
                throw std::runtime_error("query_begin out of range");
            ProbePairPartitionUpper(graph, queries[query_begin - 1], query_begin);
            return 0;
        }
        if (argc < 5 || argc > 6)
        {
            std::cerr << "usage: " << argv[0]
                      << " <data_root> <graph_selector> <query_selector>"
                      << " <known_optimum> [query_begin_1based=1]\n"
                      << "   or: " << argv[0]
                      << " --self-check [seed=1] [iterations=1000]"
                      << " [min_g=3] [max_g=8] [max_n=10]\n";
            return 2;
        }
        const std::string data_root = argv[1];
        const std::string graph_selector = argv[2];
        const std::string query_selector = argv[3];
        const double known_optimum = std::stod(argv[4]);
        const int query_begin = argc >= 6 ? std::stoi(argv[5]) : 1;
        const std::string graph_folder = gst::ResolveGraphFolder(data_root, graph_selector);
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const std::vector<gst::Query> queries =
            gst::LoadQueriesFromFolder(graph_folder, query_selector);
        if (query_begin < 1 || query_begin > static_cast<int>(queries.size()))
            throw std::runtime_error("query_begin out of range");
        ProbeQuery(graph, queries[query_begin - 1], query_begin, known_optimum);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
