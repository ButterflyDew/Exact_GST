#include "anchor_junction_upper.h"

#include <algorithm>
#include <queue>
#include <stdexcept>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "../../float_compare.h"

namespace gst::methods::anchor_junction
{
namespace
{
using HeapItem = std::pair<double, int>;
using Heap = std::priority_queue<HeapItem,
                                 std::vector<HeapItem>,
                                 std::greater<HeapItem>>;

int FirstBit(int mask)
{
#if defined(_MSC_VER)
    unsigned long index = 0;
    _BitScanForward(&index, static_cast<unsigned long>(mask));
    return static_cast<int>(index);
#else
    return __builtin_ctz(static_cast<unsigned int>(mask));
#endif
}

}  // namespace

Result BuildUpper(
    const Graph& graph,
    const Query& query,
    const std::vector<std::vector<double>>& group_distance,
    int root,
    int anchor)
{
    Result result;
    result.upper = fp::kInf;
    const int g = static_cast<int>(query.groups.size());

    std::vector<char> anchor_terminal(graph.n + 1);
    for (int vertex : query.groups[anchor])
        anchor_terminal[vertex] = 1;
    std::vector<int> path;
    int vertex = root;
    for (int steps = 0; steps <= graph.n; ++steps)
    {
        path.push_back(vertex);
        if (anchor_terminal[vertex])
            break;
        int next_vertex = 0;
        double next_value = fp::kInf;
        for (const AdjEdge& edge : graph.adj[vertex])
        {
            ++result.work;
            const double candidate = edge.w + group_distance[anchor][edge.to];
            if (candidate < next_value ||
                (candidate == next_value && edge.to < next_vertex))
            {
                next_value = candidate;
                next_vertex = edge.to;
            }
        }
        if (!next_vertex || next_value > group_distance[anchor][vertex] + fp::kEps)
            throw std::runtime_error("Failed to recover the paid anchor path.");
        vertex = next_vertex;
    }
    if (!anchor_terminal[path.back()])
        throw std::runtime_error("Paid anchor path did not reach its group.");
    result.anchor_path = path;

    std::vector<double> path_distance(graph.n + 1, fp::kInf);
    std::vector<int> parent(graph.n + 1);
    std::vector<double> parent_edge(graph.n + 1);
    Heap heap;
    for (int path_vertex : path)
    {
        path_distance[path_vertex] = 0.0;
        heap.push({0.0, path_vertex});
    }
    long long heap_pushes = static_cast<long long>(path.size());
    long long heap_pops = 0;
    while (!heap.empty())
    {
        const auto [value, current] = heap.top();
        heap.pop();
        ++heap_pops;
        if (value != path_distance[current])
            continue;
        for (const AdjEdge& edge : graph.adj[current])
        {
            ++result.work;
            const double next = value + edge.w;
            if (next < path_distance[edge.to] ||
                (next == path_distance[edge.to] &&
                 current < parent[edge.to]))
            {
                path_distance[edge.to] = next;
                parent[edge.to] = current;
                parent_edge[edge.to] = edge.w;
                heap.push({next, edge.to});
                ++heap_pushes;
            }
        }
    }
    int heap_cost = 0;
    for (long long span = 1; span < graph.n; span *= 2)
        ++heap_cost;
    result.work += (heap_pushes + heap_pops) * std::max(1, heap_cost);
    for (int path_vertex : path)
    {
        parent[path_vertex] = 0;
        parent_edge[path_vertex] = 0.0;
    }

    std::vector<int> groups;
    for (int group = 0; group < g; ++group)
        if (group != anchor)
            groups.push_back(group);
    const int k = static_cast<int>(groups.size());
    const int subset_count = 1 << k;
    const int full = subset_count - 1;

    std::vector<char> candidate(graph.n + 1);
    for (int path_vertex : path)
        candidate[path_vertex] = 1;
    for (int first = 0; first < k; ++first)
        for (int second = first + 1; second < k; ++second)
            for (int third = second + 1; third < k; ++third)
            {
                int best_root = 0;
                double best_value = fp::kInf;
                for (int current = 1; current <= graph.n; ++current)
                {
                    ++result.triple_scans;
                    ++result.work;
                    const double value = path_distance[current] +
                        group_distance[groups[first]][current] +
                        group_distance[groups[second]][current] +
                        group_distance[groups[third]][current];
                    if (value < best_value ||
                        (value == best_value && current < best_root))
                    {
                        best_value = value;
                        best_root = current;
                    }
                }
                if (best_root)
                    candidate[best_root] = 1;
            }

    std::vector<char> active(graph.n + 1);
    for (int current = 1; current <= graph.n; ++current)
        if (candidate[current])
            for (int node = current; node && !active[node]; node = parent[node])
            {
                ++result.work;
                active[node] = 1;
            }
    std::vector<int> active_children(graph.n + 1);
    for (int current = 1; current <= graph.n; ++current)
    {
        ++result.work;
        if (active[current] && parent[current])
            ++active_children[parent[current]];
    }

    std::vector<char> keep(graph.n + 1);
    std::vector<int> kept_vertices;
    for (int current = 1; current <= graph.n; ++current)
    {
        ++result.work;
        if (active[current] &&
            (candidate[current] || active_children[current] >= 2))
        {
            keep[current] = 1;
            kept_vertices.push_back(current);
        }
    }
    result.candidate_roots = static_cast<int>(std::count(
        candidate.begin(), candidate.end(), static_cast<char>(1)));
    result.tree_vertices = static_cast<int>(kept_vertices.size());

    struct Child
    {
        int id = 0;
        double length = 0.0;
    };
    const int super_root = static_cast<int>(kept_vertices.size());
    std::vector<int> id(graph.n + 1, -1);
    for (int index = 0; index < super_root; ++index)
        id[kept_vertices[index]] = index;
    std::vector<std::vector<Child>> children(super_root + 1);
    for (int index = 0; index < super_root; ++index)
    {
        int ancestor = parent[kept_vertices[index]];
        double length = parent_edge[kept_vertices[index]];
        while (ancestor && !keep[ancestor])
        {
            ++result.work;
            length += parent_edge[ancestor];
            ancestor = parent[ancestor];
        }
        const int parent_id = ancestor ? id[ancestor] : super_root;
        children[parent_id].push_back({index, ancestor ? length : 0.0});
    }

    std::vector<int> order{super_root};
    for (size_t index = 0; index < order.size(); ++index)
        for (const Child& child : children[order[index]])
            order.push_back(child.id);
    std::vector<std::vector<double>> dp(
        super_root + 1, std::vector<double>(subset_count, fp::kInf));
    std::vector<double> merged(subset_count, fp::kInf);
    for (auto it = order.rbegin(); it != order.rend(); ++it)
    {
        const int node = *it;
        dp[node][0] = 0.0;
        if (node != super_root)
        {
            const int graph_vertex = kept_vertices[node];
            for (int mask = 1; mask < subset_count; ++mask)
            {
                ++result.work;
                const int bit = mask & -mask;
                dp[node][mask] = dp[node][mask ^ bit] +
                    group_distance[groups[FirstBit(bit)]][graph_vertex];
            }
        }
        for (const Child& child : children[node])
        {
            std::fill(merged.begin(), merged.end(), fp::kInf);
            for (int mask = 0; mask < subset_count; ++mask)
                for (int below = mask;; below = (below - 1) & mask)
                {
                    ++result.convolutions;
                    ++result.work;
                    const double edge_cost = below ? child.length : 0.0;
                    merged[mask] = std::min(
                        merged[mask],
                        dp[node][mask ^ below] + dp[child.id][below] + edge_cost);
                    if (!below)
                        break;
                }
            dp[node].swap(merged);
        }
    }

    result.upper = group_distance[anchor][root] + dp[super_root][full];
    return result;
}

}  // namespace gst::methods::anchor_junction
