#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "graph_io.h"
#include "query_io.h"

namespace
{
constexpr double kInf = 1e100;

struct HeapItem
{
    double dist = 0.0;
    int vertex = 0;
    int group = 0;
};

struct HeapGreater
{
    bool operator()(const HeapItem& a, const HeapItem& b) const
    {
        if (a.dist != b.dist)
            return a.dist > b.dist;
        if (a.group != b.group)
            return a.group > b.group;
        return a.vertex > b.vertex;
    }
};

struct BoundaryVertex
{
    int vertex = 0;
    int cross_edges = 0;
    int degree = 0;
    int group_hits = 0;
};

struct ComponentInfo
{
    long long vertices = 0;
    std::uint64_t groups = 0;
};

int Popcount64(std::uint64_t x)
{
    int count = 0;
    while (x)
    {
        x &= x - 1;
        ++count;
    }
    return count;
}

double Percent(long long part, long long total)
{
    return total ? 100.0 * static_cast<double>(part) / static_cast<double>(total) : 0.0;
}

void PrintUsage(const char* argv0)
{
    std::cerr << "usage: " << argv0
              << " <data_root> <graph_selector> <query_selector> <query_begin_1based>"
              << " [query_limit=1] [max_remove_multiplier=256]\n";
}

std::vector<std::uint64_t> BuildVertexGroupMasks(const gst::Graph& graph, const gst::Query& query)
{
    if (query.groups.size() > 62)
        throw std::runtime_error("group_separator_probe supports at most 62 groups");

    std::vector<std::uint64_t> masks(graph.n + 1, 0);
    for (int gi = 0; gi < static_cast<int>(query.groups.size()); ++gi)
    {
        for (int v : query.groups[gi])
        {
            if (v < 1 || v > graph.n)
                throw std::runtime_error("query contains vertex outside graph range");
            masks[v] |= (std::uint64_t{1} << gi);
        }
    }
    return masks;
}

void ComputeNearestGroup(const gst::Graph& graph,
                         const gst::Query& query,
                         std::vector<double>& dist,
                         std::vector<int>& label)
{
    dist.assign(graph.n + 1, kInf);
    label.assign(graph.n + 1, -1);
    std::priority_queue<HeapItem, std::vector<HeapItem>, HeapGreater> heap;

    for (int gi = 0; gi < static_cast<int>(query.groups.size()); ++gi)
    {
        for (int v : query.groups[gi])
        {
            if (dist[v] > 0.0 || (dist[v] == 0.0 && (label[v] < 0 || gi < label[v])))
            {
                dist[v] = 0.0;
                label[v] = gi;
                heap.push({0.0, v, gi});
            }
        }
    }

    while (!heap.empty())
    {
        const HeapItem cur = heap.top();
        heap.pop();
        if (cur.dist != dist[cur.vertex] || cur.group != label[cur.vertex])
            continue;
        for (const auto& edge : graph.adj[cur.vertex])
        {
            const double nd = cur.dist + edge.w;
            if (nd < dist[edge.to] || (nd == dist[edge.to] && cur.group < label[edge.to]))
            {
                dist[edge.to] = nd;
                label[edge.to] = cur.group;
                heap.push({nd, edge.to, cur.group});
            }
        }
    }
}

std::vector<BoundaryVertex> BuildBoundaryOrder(const gst::Graph& graph,
                                               const std::vector<int>& label,
                                               const std::vector<std::uint64_t>& vertex_groups,
                                               long long& cross_edges)
{
    std::vector<int> cross(graph.n + 1, 0);
    cross_edges = 0;
    for (const auto& edge : graph.edges)
    {
        if (label[edge.u] >= 0 && label[edge.v] >= 0 && label[edge.u] != label[edge.v])
        {
            ++cross[edge.u];
            ++cross[edge.v];
            ++cross_edges;
        }
    }

    std::vector<BoundaryVertex> order;
    order.reserve(graph.n);
    for (int v = 1; v <= graph.n; ++v)
    {
        const int hits = Popcount64(vertex_groups[v]);
        if (cross[v] == 0 && hits <= 1)
            continue;
        order.push_back({v, cross[v], static_cast<int>(graph.adj[v].size()), hits});
    }
    std::sort(order.begin(), order.end(), [](const BoundaryVertex& a, const BoundaryVertex& b)
    {
        if (a.cross_edges != b.cross_edges)
            return a.cross_edges > b.cross_edges;
        if (a.group_hits != b.group_hits)
            return a.group_hits > b.group_hits;
        if (a.degree != b.degree)
            return a.degree > b.degree;
        return a.vertex < b.vertex;
    });
    return order;
}

std::vector<int> BuildBudgets(int g, int n, int candidate_count, int max_multiplier)
{
    std::vector<int> budgets;
    budgets.push_back(0);
    for (long long mul = 1; mul <= max_multiplier; mul <<= 1)
    {
        const long long budget = mul * static_cast<long long>(g);
        if (budget > n)
            break;
        budgets.push_back(static_cast<int>(budget));
    }
    if (candidate_count > 0 && candidate_count <= n)
        budgets.push_back(candidate_count);
    std::sort(budgets.begin(), budgets.end());
    budgets.erase(std::unique(budgets.begin(), budgets.end()), budgets.end());
    return budgets;
}

std::vector<ComponentInfo> FindComponents(const gst::Graph& graph,
                                          const std::vector<char>& removed,
                                          const std::vector<std::uint64_t>& vertex_groups)
{
    std::vector<ComponentInfo> comps;
    std::vector<char> seen(graph.n + 1, 0);
    std::vector<int> stack;
    stack.reserve(1024);

    for (int start = 1; start <= graph.n; ++start)
    {
        if (removed[start] || seen[start])
            continue;
        ComponentInfo comp;
        seen[start] = 1;
        stack.clear();
        stack.push_back(start);
        while (!stack.empty())
        {
            const int u = stack.back();
            stack.pop_back();
            ++comp.vertices;
            comp.groups |= vertex_groups[u];
            for (const auto& edge : graph.adj[u])
            {
                if (!removed[edge.to] && !seen[edge.to])
                {
                    seen[edge.to] = 1;
                    stack.push_back(edge.to);
                }
            }
        }
        comps.push_back(comp);
    }
    return comps;
}

void PrintComponentSummary(const gst::Graph& graph,
                           const gst::Query& query,
                           const std::vector<std::uint64_t>& vertex_groups,
                           const std::vector<BoundaryVertex>& order,
                           int budget)
{
    std::vector<char> removed(graph.n + 1, 0);
    const int remove_count = std::min<int>(budget, static_cast<int>(order.size()));
    long long removed_group_vertices = 0;
    std::uint64_t removed_groups = 0;
    for (int i = 0; i < remove_count; ++i)
    {
        const int v = order[i].vertex;
        removed[v] = 1;
        if (vertex_groups[v])
        {
            ++removed_group_vertices;
            removed_groups |= vertex_groups[v];
        }
    }

    const auto comps = FindComponents(graph, removed, vertex_groups);
    std::vector<long long> hist(query.groups.size() + 1, 0);
    long long mixed_comps = 0;
    long long mixed_vertices = 0;
    long long largest = 0;
    long long largest_mixed = 0;
    int max_touch = 0;
    std::vector<ComponentInfo> mixed_top;

    for (const auto& comp : comps)
    {
        const int touch = Popcount64(comp.groups);
        if (touch >= static_cast<int>(hist.size()))
            throw std::runtime_error("component touch count outside histogram");
        ++hist[touch];
        largest = std::max(largest, comp.vertices);
        max_touch = std::max(max_touch, touch);
        if (touch >= 2)
        {
            ++mixed_comps;
            mixed_vertices += comp.vertices;
            largest_mixed = std::max(largest_mixed, comp.vertices);
            mixed_top.push_back(comp);
        }
    }
    std::sort(mixed_top.begin(), mixed_top.end(), [](const ComponentInfo& a, const ComponentInfo& b)
    {
        if (a.vertices != b.vertices)
            return a.vertices > b.vertices;
        return Popcount64(a.groups) > Popcount64(b.groups);
    });

    std::cout << "  budget=" << budget
              << " removed=" << remove_count
              << " removed_group_vertices=" << removed_group_vertices
              << " removed_group_count=" << Popcount64(removed_groups)
              << " comps=" << comps.size()
              << " mixed_comps=" << mixed_comps
              << " max_touch=" << max_touch
              << " largest_comp=" << largest
              << " largest_mixed=" << largest_mixed
              << " mixed_vertices=" << mixed_vertices
              << " mixed_pct=" << std::fixed << std::setprecision(3) << Percent(mixed_vertices, graph.n);
    for (int touch = 0; touch < static_cast<int>(hist.size()); ++touch)
    {
        if (hist[touch])
            std::cout << " touch" << touch << "=" << hist[touch];
    }
    for (int i = 0; i < static_cast<int>(mixed_top.size()) && i < 5; ++i)
    {
        std::cout << " top_mixed" << i + 1 << "="
                  << mixed_top[i].vertices << ":" << Popcount64(mixed_top[i].groups);
    }
    std::cout << "\n";
}

void ProbeQuery(const gst::Graph& graph, const gst::Query& query, int query_id, int max_multiplier)
{
    const int g = static_cast<int>(query.groups.size());
    const auto vertex_groups = BuildVertexGroupMasks(graph, query);
    long long group_vertices = 0;
    long long shared_group_vertices = 0;
    for (int v = 1; v <= graph.n; ++v)
    {
        if (vertex_groups[v])
        {
            ++group_vertices;
            if (Popcount64(vertex_groups[v]) > 1)
                ++shared_group_vertices;
        }
    }

    std::vector<double> dist;
    std::vector<int> label;
    ComputeNearestGroup(graph, query, dist, label);

    long long cross_edges = 0;
    const auto boundary_order = BuildBoundaryOrder(graph, label, vertex_groups, cross_edges);
    std::vector<long long> region_size(g, 0);
    for (int v = 1; v <= graph.n; ++v)
    {
        if (label[v] >= 0)
            ++region_size[label[v]];
    }

    std::cout << "query=" << query_id
              << " g=" << g
              << " n=" << graph.n
              << " m=" << graph.m
              << " group_vertices=" << group_vertices
              << " shared_group_vertices=" << shared_group_vertices
              << " boundary_candidates=" << boundary_order.size()
              << " boundary_pct=" << std::fixed << std::setprecision(3)
              << Percent(static_cast<long long>(boundary_order.size()), graph.n)
              << " cross_edges=" << cross_edges << "\n";

    std::vector<long long> sorted_regions = region_size;
    std::sort(sorted_regions.begin(), sorted_regions.end(), std::greater<long long>());
    std::cout << "  voronoi_regions";
    for (int i = 0; i < static_cast<int>(sorted_regions.size()); ++i)
        std::cout << " r" << i + 1 << "=" << sorted_regions[i];
    std::cout << "\n";

    const auto budgets = BuildBudgets(g, graph.n, static_cast<int>(boundary_order.size()), max_multiplier);
    for (int budget : budgets)
        PrintComponentSummary(graph, query, vertex_groups, boundary_order, budget);
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 5)
        {
            PrintUsage(argv[0]);
            return 2;
        }

        const std::string data_root = argv[1];
        const std::string graph_selector = argv[2];
        const std::string query_selector = argv[3];
        const int query_begin = std::atoi(argv[4]);
        const int query_limit = (argc >= 6) ? std::atoi(argv[5]) : 1;
        const int max_multiplier = (argc >= 7) ? std::atoi(argv[6]) : 256;
        if (query_begin < 1 || query_limit < 0 || max_multiplier < 1)
        {
            PrintUsage(argv[0]);
            return 2;
        }

        const std::string graph_folder = gst::ResolveGraphFolder(data_root, graph_selector);
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const std::vector<gst::Query> queries = gst::LoadQueriesFromFolder(graph_folder, query_selector);
        const int begin = query_begin - 1;
        if (begin >= static_cast<int>(queries.size()))
            throw std::runtime_error("query_begin out of range");
        const int end = query_limit == 0
                            ? static_cast<int>(queries.size())
                            : std::min<int>(static_cast<int>(queries.size()), begin + query_limit);

        std::cout << "graph=" << graph_selector
                  << " graph_folder=" << graph_folder
                  << " query_selector=" << query_selector
                  << " query_begin=" << query_begin
                  << " query_limit=" << query_limit
                  << " max_remove_multiplier=" << max_multiplier << "\n";
        for (int qi = begin; qi < end; ++qi)
            ProbeQuery(graph, queries[qi], qi + 1, max_multiplier);
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}
