#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "graph_io.h"
#include "methods/DPBF/dpbf_solver.h"
#include "methods/Release/release_v1.h"
#include "memory_usage.h"
#include "query_io.h"
#include "distance_solver.h"

namespace
{
std::string LayerShape(const gst::tools::distance_epoch::ReleaseStats& stats)
{
    std::ostringstream out;
    bool first = true;
    for (int size = 2; size < static_cast<int>(stats.kept_by_size.size()); ++size)
    {
        if (!stats.kept_by_size[size])
            continue;
        if (!first)
            out << ',';
        first = false;
        out << size << ':' << stats.active_roots_by_size[size]
            << '/' << stats.kept_by_size[size]
            << '/' << std::fixed << std::setprecision(3)
            << static_cast<double>(stats.depth_sum_by_size[size]) /
                   static_cast<double>(stats.kept_by_size[size])
            << '/' << stats.max_depth_by_size[size];
    }
    return out.str();
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
        const auto distance = gst::tools::distance_epoch::SolveOneQuery(graph, query);
        if (exact.feasible != distance.feasible ||
            (exact.feasible && std::fabs(exact.best_weight - distance.best_weight) > 1e-6))
        {
            std::cout << std::setprecision(17)
                      << "MISMATCH seed=" << seed
                      << " iteration=" << iteration
                      << " n=" << graph.n
                      << " g=" << query.groups.size()
                      << " exact=" << exact.best_weight
                      << " distance=" << distance.best_weight << '\n';
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
        const auto distance = gst::tools::distance_epoch::SolveOneQuery(graph, queries[index]);
        const bool equal = release.feasible == distance.feasible &&
                           (!release.feasible ||
                            std::fabs(release.best_weight - distance.best_weight) <= 1e-6);
        std::cout << std::fixed << std::setprecision(6)
                  << "query=" << index + 1
                  << " equal=" << equal
                  << " release_weight=" << release.best_weight
                  << " distance_weight=" << distance.best_weight
                  << " release_ms=" << release.stats.total_ms
                  << " distance_ms=" << distance.stats.total_ms
                  << " ratio="
                  << (release.stats.total_ms > 0.0
                          ? distance.stats.total_ms / release.stats.total_ms
                          : 0.0)
                  << " release_saved_states=" << release.stats.saved_states
                  << " distance_states=" << distance.stats.distance_states
                  << " distance_bytes=" << distance.stats.distance_bytes
                  << " baseline_row_bytes=" << distance.stats.baseline_row_bytes
                  << " compact_recomputed_states="
                  << distance.stats.compact_recomputed_states
                  << " compact_recompute_ms=" << distance.stats.compact_recompute_ms
                  << " dual_cut_ms=" << distance.stats.dual_cut_ms
                  << " dual_cut_objective=" << distance.stats.dual_cut_objective
                  << " dual_cut_primal_upper=" << distance.stats.dual_cut_primal_upper
                  << " dual_cut_enabled=" << distance.stats.dual_cut_enabled
                  << " dense_join_work=" << distance.stats.dense_join_work
                  << " dual_cut_build_work=" << distance.stats.dual_cut_build_work
                  << " byte_compression="
                  << (distance.stats.distance_bytes
                          ? static_cast<double>(distance.stats.baseline_row_bytes) /
                                static_cast<double>(distance.stats.distance_bytes)
                          : 0.0)
                  << " layer_shape=" << LayerShape(distance.stats)
                  << '\n';
        if (!equal)
            throw std::runtime_error("distance solver differs from ReleaseV1");
    }
}

void DistanceOnly(const gst::Graph& graph,
                  const std::vector<gst::Query>& queries,
                  int query_begin,
                  int query_limit)
{
    const int end = query_limit < 0
                        ? static_cast<int>(queries.size())
                        : std::min<int>(queries.size(), query_begin - 1 + query_limit);
    for (int index = query_begin - 1; index < end; ++index)
    {
        const auto before = gst::GetProcessMemoryUsage();
        const auto result =
            gst::tools::distance_epoch::SolveOneQuery(graph, queries[index], true);
        const auto after = gst::GetProcessMemoryUsage();
        std::cout << std::fixed << std::setprecision(6)
                  << "distance_only query=" << index + 1
                  << " feasible=" << result.feasible
                  << " weight=" << result.best_weight
                  << " total_ms=" << result.stats.total_ms
                  << " distance_states=" << result.stats.distance_states
                  << " distance_bytes=" << result.stats.distance_bytes
                  << " baseline_row_bytes=" << result.stats.baseline_row_bytes
                  << " compact_recomputed_states="
                  << result.stats.compact_recomputed_states
                  << " compact_recompute_ms=" << result.stats.compact_recompute_ms
                  << " pair_partition_roots=" << result.stats.pair_partition_roots
                  << " pair_partition_updates=" << result.stats.pair_partition_updates
                  << " pair_partition_upper=" << result.stats.pair_partition_upper
                  << " pair_partition_ms=" << result.stats.pair_partition_ms
                  << " dual_cut_ms=" << result.stats.dual_cut_ms
                  << " dual_cut_objective=" << result.stats.dual_cut_objective
                  << " dual_cut_primal_upper=" << result.stats.dual_cut_primal_upper
                  << " dual_cut_enabled=" << result.stats.dual_cut_enabled
                  << " dense_join_work=" << result.stats.dense_join_work
                  << " dual_cut_build_work=" << result.stats.dual_cut_build_work
                  << " layer_shape=" << LayerShape(result.stats)
                  << " rss_before_mb=" << gst::BytesToMiB(before.current_rss_bytes)
                  << " rss_after_mb=" << gst::BytesToMiB(after.current_rss_bytes)
                  << " peak_rss_mb=" << gst::BytesToMiB(after.peak_rss_bytes) << '\n';
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
        const bool distance_only = argc >= 2 && std::string(argv[1]) == "--distance";
        const int offset = distance_only ? 1 : 0;
        if (argc < 4 + offset || argc > 6 + offset)
        {
            std::cerr << "usage: " << argv[0]
                      << " <data_root> <graph_selector> <query_selector>"
                      << " [query_begin_1based=1] [query_limit=1]\n"
                      << "   or: " << argv[0]
                      << " --self-check [seed=1] [iterations=1000]"
                      << " [min_g=2] [max_g=8] [max_n=10]\n";
            return 2;
        }
        const std::string graph_folder =
            gst::ResolveGraphFolder(argv[1 + offset], argv[2 + offset]);
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const std::vector<gst::Query> queries =
            gst::LoadQueriesFromFolder(graph_folder, argv[3 + offset]);
        const int query_begin = argc >= 5 + offset ? std::stoi(argv[4 + offset]) : 1;
        const int query_limit = argc >= 6 + offset ? std::stoi(argv[5 + offset]) : 1;
        if (query_begin < 1 || query_begin > static_cast<int>(queries.size()))
            throw std::runtime_error("query_begin out of range");
        if (distance_only)
            DistanceOnly(graph, queries, query_begin, query_limit);
        else
            DatasetProbe(graph, queries, query_begin, query_limit);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
