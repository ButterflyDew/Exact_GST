#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "graph_io.h"
#include "methods/DPBF/dpbf_solver.h"
#include "methods/Release/release_v1.h"
#include "query_io.h"
#include "witness_solver.h"

namespace
{
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

int SelfCheck(std::uint64_t seed,
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
        const auto exact = gst::methods::dpbf::SolveOneQuery(graph, query);
        const auto witness = gst::tools::tsp_witness::SolveOneQuery(graph, query);
        if (exact.feasible != witness.feasible ||
            (exact.feasible && std::fabs(exact.best_weight - witness.best_weight) > 1e-6))
        {
            std::cout << std::setprecision(17)
                      << "MISMATCH seed=" << seed
                      << " iteration=" << iteration
                      << " n=" << graph.n
                      << " g=" << query.groups.size()
                      << " exact=" << exact.best_weight
                      << " witness=" << witness.best_weight << '\n';
            return 1;
        }
        if (iteration % 1000 == 0)
            std::cout << "ok " << iteration << '\n';
    }
    std::cout << "ALL_OK seed=" << seed << " iterations=" << iterations << '\n';
    return 0;
}

void DatasetProbe(const gst::Graph& graph,
                  const std::vector<gst::Query>& queries,
                  int query_begin,
                  int query_limit)
{
    const int end = query_limit < 0
                        ? static_cast<int>(queries.size())
                        : std::min<int>(queries.size(), query_begin - 1 + query_limit);
    for (int index = query_begin - 1; index < end; ++index)
    {
        const auto release = gst::methods::release_v1::SolveOneQuery(graph, queries[index]);
        const auto witness = gst::tools::tsp_witness::SolveOneQuery(graph, queries[index]);
        const bool equal = release.feasible == witness.feasible &&
                           (!release.feasible ||
                            std::fabs(release.best_weight - witness.best_weight) <= 1e-6);
        std::cout << std::fixed << std::setprecision(6)
                  << "query=" << index + 1
                  << " equal=" << equal
                  << " release_weight=" << release.best_weight
                  << " witness_weight=" << witness.best_weight
                  << " release_ms=" << release.stats.total_ms
                  << " witness_ms=" << witness.stats.total_ms
                  << " ratio="
                  << (release.stats.total_ms > 0.0
                          ? witness.stats.total_ms / release.stats.total_ms
                          : 0.0)
                  << " release_saved_states=" << release.stats.saved_states
                  << " witness_states=" << witness.stats.witness_states
                  << " witness_bytes=" << witness.stats.witness_bytes
                  << " baseline_row_bytes=" << witness.stats.baseline_row_bytes
                  << " byte_compression="
                  << (witness.stats.witness_bytes
                          ? static_cast<double>(witness.stats.baseline_row_bytes) /
                                static_cast<double>(witness.stats.witness_bytes)
                          : 0.0)
                  << '\n';
        if (!equal)
            throw std::runtime_error("witness solver differs from ReleaseV1");
    }
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
            const int min_group_count = argc >= 5 ? std::stoi(argv[4]) : 2;
            const int max_group_count = argc >= 6 ? std::stoi(argv[5]) : 8;
            const int max_vertex_count = argc >= 7 ? std::stoi(argv[6]) : 10;
            return SelfCheck(
                seed, iterations, min_group_count, max_group_count, max_vertex_count);
        }
        if (argc < 4 || argc > 6)
        {
            std::cerr << "usage: " << argv[0]
                      << " <data_root> <graph_selector> <query_selector>"
                      << " [query_begin_1based=1] [query_limit=1]\n"
                      << "   or: " << argv[0]
                      << " --self-check [seed=1] [iterations=1000]"
                      << " [min_g=2] [max_g=8] [max_n=10]\n";
            return 2;
        }
        const std::string graph_folder = gst::ResolveGraphFolder(argv[1], argv[2]);
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const std::vector<gst::Query> queries =
            gst::LoadQueriesFromFolder(graph_folder, argv[3]);
        const int query_begin = argc >= 5 ? std::stoi(argv[4]) : 1;
        const int query_limit = argc >= 6 ? std::stoi(argv[5]) : 1;
        if (query_begin < 1 || query_begin > static_cast<int>(queries.size()))
            throw std::runtime_error("query_begin out of range");
        DatasetProbe(graph, queries, query_begin, query_limit);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
