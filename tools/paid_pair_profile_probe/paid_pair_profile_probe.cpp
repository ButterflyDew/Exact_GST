#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
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
using Item = std::pair<double, int>;
using Heap = std::priority_queue<Item, std::vector<Item>, std::greater<Item>>;

int FirstBit(int mask)
{
    int bit = 0;
    while ((mask & 1) == 0)
    {
        mask >>= 1;
        ++bit;
    }
    return bit;
}

using ParentForest = std::vector<std::vector<int>>;
using SettleOrder = std::vector<std::vector<int>>;

std::vector<std::vector<double>> GroupDistances(const gst::Graph& graph,
                                                const gst::Query& query,
                                                ParentForest* parent,
                                                SettleOrder* order)
{
    std::vector<std::vector<double>> result(
        query.groups.size(), std::vector<double>(graph.n + 1, gst::fp::kInf));
    for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
    {
        Heap heap;
        std::vector<char> finalized(graph.n + 1);
        for (int vertex : query.groups[group])
        {
            result[group][vertex] = 0.0;
            if (parent)
                (*parent)[group][vertex] = 0;
            heap.push({0.0, vertex});
        }
        while (!heap.empty())
        {
            const auto [distance, vertex] = heap.top();
            heap.pop();
            if (distance != result[group][vertex] || finalized[vertex])
                continue;
            finalized[vertex] = 1;
            if (order)
                (*order)[group].push_back(vertex);
            for (const auto& edge : graph.adj[vertex])
            {
                const double next = distance + edge.w;
                if (next < result[group][edge.to])
                {
                    result[group][edge.to] = next;
                    if (parent)
                        (*parent)[group][edge.to] = vertex;
                    heap.push({next, edge.to});
                }
            }
        }
    }
    return result;
}

std::vector<std::vector<double>> GroupMetric(
    const gst::Query& query,
    const std::vector<std::vector<double>>& group_distance)
{
    const int g = static_cast<int>(query.groups.size());
    std::vector<std::vector<double>> metric(g, std::vector<double>(g, gst::fp::kInf));
    for (int first = 0; first < g; ++first)
        for (int second = 0; second < g; ++second)
            for (int vertex : query.groups[second])
                metric[first][second] =
                    std::min(metric[first][second], group_distance[first][vertex]);
    return metric;
}

struct PairStats
{
    long long settled = 0;
    long long skyline = 0;
    long long root_vector_skyline = 0;
    long long witness_vector_skyline = 0;
    long long witness_from_anchor_skyline = 0;
    long long tree_count = 0;
    long long tree_vertex_incidence = 0;
    long long max_tree_vertices = 0;
    long long pushes = 0;
    double paid_singleton_upper = gst::fp::kInf;
    double milliseconds = 0.0;
};


using PathMin = std::vector<std::vector<std::vector<double>>>;

PathMin BuildPathMin(const gst::Graph& graph,
                     const ParentForest& parent,
                     const SettleOrder& order,
                     const std::vector<std::vector<double>>& group_distance)
{
    const int g = static_cast<int>(group_distance.size());
    PathMin path_min(
        g, std::vector<std::vector<double>>(g, std::vector<double>(graph.n + 1)));
    for (int source = 0; source < g; ++source)
    {
        for (int profile = 0; profile < g; ++profile)
            for (int vertex : order[source])
            {
                path_min[source][profile][vertex] = group_distance[profile][vertex];
                const int previous = parent[source][vertex];
                if (previous > 0)
                    path_min[source][profile][vertex] =
                        std::min(path_min[source][profile][vertex],
                                 path_min[source][profile][previous]);
            }
    }
    return path_min;
}

PairStats ProbePair(const gst::Graph& graph,
                    int first,
                    int second,
                    int full_mask,
                    int anchor_group,
                    double known_optimum,
                    const std::vector<std::vector<double>>& group_distance,
                    const gst::methods::test19::MetricTspLowerBound& lower,
                    const PathMin* path_min,
                    bool upper_only,
                    const ParentForest* group_parent,
                    std::vector<int>* tree_stamp,
                    int* stamp,
                    std::vector<char>* global_tree_vertex)
{
    const int pair_mask = (1 << first) | (1 << second);
    const int remaining = full_mask ^ pair_mask;
    std::vector<double> distance(graph.n + 1, gst::fp::kInf);
    std::vector<double> heuristic(graph.n + 1);
    std::vector<char> settled(graph.n + 1);
    std::vector<int> parent(graph.n + 1, -1);
    Heap queue;
    PairStats stats;

    const auto begin = Clock::now();
    for (int vertex = 1; vertex <= graph.n; ++vertex)
    {
        heuristic[vertex] = lower.TourHalf(remaining, vertex, group_distance);
        const double seed =
            group_distance[first][vertex] + group_distance[second][vertex];
        if (seed + heuristic[vertex] <= known_optimum + gst::fp::kEps)
        {
            distance[vertex] = seed;
            parent[vertex] = 0;
            queue.push({seed, vertex});
            ++stats.pushes;
        }
    }
    while (!queue.empty())
    {
        const auto [value, vertex] = queue.top();
        queue.pop();
        if (value != distance[vertex] || settled[vertex] ||
            value + heuristic[vertex] > known_optimum + gst::fp::kEps)
            continue;
        settled[vertex] = 1;
        ++stats.settled;
        for (const auto& edge : graph.adj[vertex])
        {
            const double next = value + edge.w;
            if (next >= distance[edge.to] ||
                next + heuristic[edge.to] > known_optimum + gst::fp::kEps)
                continue;
            distance[edge.to] = next;
            parent[edge.to] = vertex;
            queue.push({next, edge.to});
            ++stats.pushes;
        }
    }

    std::vector<int> roots;
    roots.reserve(static_cast<size_t>(stats.settled));
    for (int vertex = 1; vertex <= graph.n; ++vertex)
        if (settled[vertex])
            roots.push_back(vertex);
    std::sort(roots.begin(), roots.end(), [&](int a, int b)
    {
        if (distance[a] != distance[b])
            return distance[a] < distance[b];
        if (group_distance[anchor_group][a] != group_distance[anchor_group][b])
            return group_distance[anchor_group][a] < group_distance[anchor_group][b];
        return a < b;
    });
    double best_attachment = gst::fp::kInf;
    std::vector<int> anchor_front;
    for (int vertex : roots)
    {
        const double attachment = group_distance[anchor_group][vertex];
        if (attachment + gst::fp::kEps < best_attachment)
        {
            best_attachment = attachment;
            ++stats.skyline;
            anchor_front.push_back(vertex);
        }
    }
    if (group_parent)
    {
        for (int root : anchor_front)
        {
            ++*stamp;
            long long tree_vertices = 0;
            auto AddPath = [&](int vertex, const std::vector<int>& path_parent)
            {
                while (vertex > 0)
                {
                    if ((*tree_stamp)[vertex] != *stamp)
                    {
                        (*tree_stamp)[vertex] = *stamp;
                        (*global_tree_vertex)[vertex] = 1;
                        ++tree_vertices;
                    }
                    vertex = path_parent[vertex];
                }
            };
            int split_root = root;
            while (parent[split_root] > 0)
                split_root = parent[split_root];
            AddPath(root, parent);
            AddPath(split_root, (*group_parent)[first]);
            AddPath(split_root, (*group_parent)[second]);
            ++stats.tree_count;
            stats.tree_vertex_incidence += tree_vertices;
            stats.max_tree_vertices =
                std::max(stats.max_tree_vertices, tree_vertices);
        }
    }

    std::vector<int> profile_groups;
    for (int group = 0; group < static_cast<int>(group_distance.size()); ++group)
        if (group != first && group != second)
            profile_groups.push_back(group);
    if (!upper_only)
    {
        std::sort(roots.begin(), roots.end(), [&](int a, int b)
        {
            if (distance[a] != distance[b])
                return distance[a] < distance[b];
            for (int group : profile_groups)
                if (group_distance[group][a] != group_distance[group][b])
                    return group_distance[group][a] < group_distance[group][b];
            return a < b;
        });
        std::vector<int> vector_front;
        for (int vertex : roots)
        {
            bool dominated = false;
            for (int other : vector_front)
            {
                bool no_worse = true;
                for (int group : profile_groups)
                    if (group_distance[group][other] >
                        group_distance[group][vertex] + gst::fp::kEps)
                    {
                        no_worse = false;
                        break;
                    }
                if (no_worse)
                {
                    dominated = true;
                    break;
                }
            }
            if (!dominated)
                vector_front.push_back(vertex);
        }
        stats.root_vector_skyline = vector_front.size();
    }

    if (path_min)
    {
        std::vector<std::vector<double>> pair_profile(
            group_distance.size(), std::vector<double>(graph.n + 1, gst::fp::kInf));
        std::sort(roots.begin(), roots.end(), [&](int a, int b)
        {
            if (distance[a] != distance[b])
                return distance[a] < distance[b];
            return a < b;
        });
        for (int vertex : roots)
        {
            if (parent[vertex] == 0)
            {
                for (int group : profile_groups)
                    pair_profile[group][vertex] =
                        std::min((*path_min)[first][group][vertex],
                                 (*path_min)[second][group][vertex]);
            }
            else
            {
                for (int group : profile_groups)
                    pair_profile[group][vertex] =
                        std::min(pair_profile[group][parent[vertex]],
                                 group_distance[group][vertex]);
            }
        }
        if (!upper_only)
        {
            std::sort(roots.begin(), roots.end(), [&](int a, int b)
            {
                const double cost_a = distance[a] + group_distance[anchor_group][a];
                const double cost_b = distance[b] + group_distance[anchor_group][b];
                if (cost_a != cost_b)
                    return cost_a < cost_b;
                for (int group : profile_groups)
                {
                    const double value_a = std::min(
                        pair_profile[group][a], (*path_min)[anchor_group][group][a]);
                    const double value_b = std::min(
                        pair_profile[group][b], (*path_min)[anchor_group][group][b]);
                    if (value_a != value_b)
                        return value_a < value_b;
                }
                return a < b;
            });
            std::vector<int> witness_front;
            for (int vertex : roots)
            {
                bool dominated = false;
                for (int other : witness_front)
                {
                    bool no_worse = true;
                    for (int group : profile_groups)
                    {
                        const double other_value = std::min(
                            pair_profile[group][other],
                            (*path_min)[anchor_group][group][other]);
                        const double value = std::min(
                            pair_profile[group][vertex],
                            (*path_min)[anchor_group][group][vertex]);
                        if (other_value > value + gst::fp::kEps)
                        {
                            no_worse = false;
                            break;
                        }
                    }
                    if (no_worse)
                    {
                        dominated = true;
                        break;
                    }
                }
                if (!dominated)
                    witness_front.push_back(vertex);
            }
            stats.witness_vector_skyline = witness_front.size();
            std::sort(anchor_front.begin(), anchor_front.end());
            for (int vertex : witness_front)
                if (std::binary_search(anchor_front.begin(), anchor_front.end(), vertex))
                    ++stats.witness_from_anchor_skyline;
        }
        for (int vertex : roots)
        {
            double upper = distance[vertex] + group_distance[anchor_group][vertex];
            for (int group : profile_groups)
            {
                if (group == anchor_group)
                    continue;
                upper += std::min(pair_profile[group][vertex],
                                  (*path_min)[anchor_group][group][vertex]);
            }
            stats.paid_singleton_upper =
                std::min(stats.paid_singleton_upper, upper);
        }
    }
    stats.milliseconds =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return stats;
}

void Probe(const gst::Graph& graph,
           const gst::Query& query,
           int query_id,
           double known_optimum,
           bool verbose,
           bool witness_vector,
           bool upper_only,
           bool tree_incidence)
{
    const auto begin = Clock::now();
    const int g = static_cast<int>(query.groups.size());
    if (g < 3 || g > 20)
        throw std::runtime_error("paid pair profile requires 3 <= g <= 20");
    const int full_mask = (1 << g) - 1;
    ParentForest parent;
    SettleOrder order;
    if (witness_vector || tree_incidence)
    {
        parent.assign(g, std::vector<int>(graph.n + 1, -1));
    }
    if (witness_vector)
    {
        order.resize(g);
    }
    const auto group_distance = GroupDistances(
        graph, query,
        (witness_vector || tree_incidence) ? &parent : nullptr,
        witness_vector ? &order : nullptr);
    const auto metric = GroupMetric(query, group_distance);
    const PathMin path_min =
        witness_vector ? BuildPathMin(graph, parent, order, group_distance) : PathMin{};
    gst::methods::test19::MetricTspLowerBound lower;
    lower.Build(metric, full_mask);

    int root = 1;
    double root_star = gst::fp::kInf;
    for (int vertex = 1; vertex <= graph.n; ++vertex)
    {
        double value = 0.0;
        for (int group = 0; group < g; ++group)
            value += group_distance[group][vertex];
        if (value < root_star)
        {
            root_star = value;
            root = vertex;
        }
    }
    int anchor_group = 0;
    for (int group = 1; group < g; ++group)
        if (group_distance[group][root] > group_distance[anchor_group][root])
            anchor_group = group;

    PairStats total;
    int pair_count = 0;
    long long maximum_skyline = 0;
    long long maximum_vector_skyline = 0;
    long long maximum_witness_skyline = 0;
    std::vector<int> tree_stamp(graph.n + 1);
    std::vector<char> global_tree_vertex(graph.n + 1);
    int stamp = 0;
    for (int first = 0; first < g; ++first)
    {
        if (first == anchor_group)
            continue;
        for (int second = first + 1; second < g; ++second)
        {
            if (second == anchor_group)
                continue;
            const PairStats current = ProbePair(
                graph, first, second, full_mask, anchor_group, known_optimum,
                group_distance, lower, witness_vector ? &path_min : nullptr,
                upper_only || tree_incidence,
                tree_incidence ? &parent : nullptr, tree_incidence ? &tree_stamp : nullptr,
                tree_incidence ? &stamp : nullptr,
                tree_incidence ? &global_tree_vertex : nullptr);
            ++pair_count;
            total.settled += current.settled;
            total.skyline += current.skyline;
            total.root_vector_skyline += current.root_vector_skyline;
            total.witness_vector_skyline += current.witness_vector_skyline;
            total.witness_from_anchor_skyline += current.witness_from_anchor_skyline;
            total.tree_count += current.tree_count;
            total.tree_vertex_incidence += current.tree_vertex_incidence;
            total.max_tree_vertices =
                std::max(total.max_tree_vertices, current.max_tree_vertices);
            total.pushes += current.pushes;
            total.milliseconds += current.milliseconds;
            total.paid_singleton_upper =
                std::min(total.paid_singleton_upper, current.paid_singleton_upper);
            maximum_skyline = std::max(maximum_skyline, current.skyline);
            maximum_vector_skyline =
                std::max(maximum_vector_skyline, current.root_vector_skyline);
            maximum_witness_skyline =
                std::max(maximum_witness_skyline, current.witness_vector_skyline);
            if (verbose)
                std::cout << "pair=" << pair_count
                          << " groups=" << (first + 1) << ',' << (second + 1)
                          << " settled=" << current.settled
                          << " skyline=" << current.skyline
                          << " vector_skyline=" << current.root_vector_skyline
                          << " witness_skyline=" << current.witness_vector_skyline
                          << " ratio=" << std::fixed << std::setprecision(6)
                          << (current.settled
                                  ? static_cast<double>(current.skyline) / current.settled
                                  : 0.0)
                          << " ms=" << current.milliseconds << '\n';
        }
    }
    const double elapsed =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    const long long unique_tree_vertices =
        std::accumulate(global_tree_vertex.begin(), global_tree_vertex.end(), 0LL);
    std::cout << std::fixed << std::setprecision(10)
              << "paid_pair_profile query=" << query_id
              << " n=" << graph.n
              << " m=" << graph.m
              << " g=" << g
              << " anchor_group=" << (anchor_group + 1)
              << " root=" << root
              << " known_optimum=" << known_optimum
              << " pairs=" << pair_count
              << " settled=" << total.settled
              << " skyline=" << total.skyline
              << " skyline_ratio="
              << (total.settled ? static_cast<double>(total.skyline) / total.settled : 0.0)
              << " max_skyline=" << maximum_skyline
              << " vector_skyline=" << total.root_vector_skyline
              << " vector_skyline_ratio="
              << (total.settled
                      ? static_cast<double>(total.root_vector_skyline) / total.settled
                      : 0.0)
              << " max_vector_skyline=" << maximum_vector_skyline
              << " witness_skyline=" << total.witness_vector_skyline
              << " witness_skyline_ratio="
              << (total.settled
                      ? static_cast<double>(total.witness_vector_skyline) / total.settled
                      : 0.0)
              << " witness_from_anchor=" << total.witness_from_anchor_skyline
              << " max_witness_skyline=" << maximum_witness_skyline
              << " tree_count=" << total.tree_count
              << " tree_vertex_incidence=" << total.tree_vertex_incidence
              << " unique_tree_vertices=" << unique_tree_vertices
              << " max_tree_vertices=" << total.max_tree_vertices
              << " paid_singleton_upper="
              << (total.paid_singleton_upper < gst::fp::kInf / 2
                      ? total.paid_singleton_upper
                      : -1.0)
              << " paid_singleton_gap="
              << (total.paid_singleton_upper < gst::fp::kInf / 2
                      ? total.paid_singleton_upper - known_optimum
                      : -1.0)
              << " paid_singleton_gap_pct="
              << (known_optimum > 0.0 &&
                          total.paid_singleton_upper < gst::fp::kInf / 2
                      ? 100.0 * (total.paid_singleton_upper - known_optimum) /
                            known_optimum
                      : -1.0)
              << " pushes=" << total.pushes
              << " pair_ms=" << total.milliseconds
              << " total_ms=" << elapsed << '\n';
}
}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 5 || argc > 10)
        {
            std::cerr << "usage: " << argv[0]
                      << " <data_root> <graph_selector> <query_selector>"
                      << " <known_optimum> [query_begin_1based=1]"
                      << " [--summary] [--witness-vector] [--upper-only]"
                      << " [--tree-incidence]\n";
            return 2;
        }
        const std::string graph_folder = gst::ResolveGraphFolder(argv[1], argv[2]);
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const std::vector<gst::Query> queries =
            gst::LoadQueriesFromFolder(graph_folder, argv[3]);
        const double known_optimum = std::stod(argv[4]);
        const int query_id = argc >= 6 ? std::stoi(argv[5]) : 1;
        bool verbose = true;
        bool witness_vector = false;
        bool upper_only = false;
        bool tree_incidence = false;
        for (int index = 6; index < argc; ++index)
        {
            const std::string option = argv[index];
            if (option == "--summary")
                verbose = false;
            else if (option == "--witness-vector")
                witness_vector = true;
            else if (option == "--upper-only")
            {
                witness_vector = true;
                upper_only = true;
            }
            else if (option == "--tree-incidence")
                tree_incidence = true;
            else
                throw std::runtime_error("unknown option: " + option);
        }
        if (query_id < 1 || query_id > static_cast<int>(queries.size()))
            throw std::runtime_error("query index out of range");
        Probe(graph, queries[query_id - 1], query_id, known_optimum,
              verbose, witness_vector, upper_only, tree_incidence);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
