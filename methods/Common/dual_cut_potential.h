#ifndef GST_METHODS_COMMON_DUAL_CUT_POTENTIAL_H
#define GST_METHODS_COMMON_DUAL_CUT_POTENTIAL_H

#include <algorithm>
#include <functional>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "../../float_compare.h"
#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::dual_cut
{
struct ProgressivePackingResult
{
    int rounds = 0;
    double min_scale = 0.0;
    double average_scale = 0.0;
    double max_scale = 0.0;
};

class DualCutPotential
{
public:
    void Build(const Graph& graph,
               const Query& query,
               const std::vector<std::vector<double>>& group_distance,
               int root)
    {
        BuildInternal(graph, query, group_distance, std::vector<int>{root}, false);
        primal_upper_ = RecoverPrimal(graph, query, root, residual_);
        residual_.clear();
        residual_.shrink_to_fit();
    }

    void BuildDynamic(const Graph& graph,
                      const Query& query,
                      const std::vector<std::vector<double>>& group_distance,
                      int root)
    {
        BuildInternal(graph, query, group_distance, std::vector<int>{root}, true);
        primal_upper_ = RecoverPrimal(graph, query, root, residual_);
        residual_.clear();
        residual_.shrink_to_fit();
    }

    void BuildKeepingResidual(const Graph& graph,
                              const Query& query,
                              const std::vector<std::vector<double>>& group_distance,
                              int root)
    {
        BuildInternal(graph, query, group_distance, std::vector<int>{root}, false);
        primal_upper_ = RecoverPrimal(graph, query, root, residual_);
    }

    ProgressivePackingResult StrengthenAlongPath(
        const Graph& graph,
        const Query& query,
        const std::vector<int>& paid_path)
    {
        ProgressivePackingResult result;
        const int g = static_cast<int>(query.groups.size());
        if (residual_.empty() || paid_path.empty() || !g)
            return result;

        using HeapItem = std::pair<double, int>;
        // Each capped residual distance is a feasible direction whose group
        // terminals remain at zero and whose paid-path values are bounded.
        std::vector<std::vector<double>> direction(
            g, std::vector<double>(graph.n + 1, fp::kInf));
        for (int group = 0; group < g; ++group)
        {
            auto& distance = direction[group];
            std::priority_queue<HeapItem,
                                std::vector<HeapItem>,
                                std::greater<HeapItem>> heap;
            for (int terminal : query.groups[group])
            {
                distance[terminal] = 0.0;
                heap.push({0.0, terminal});
            }
            while (!heap.empty())
            {
                const auto [value, vertex] = heap.top();
                heap.pop();
                if (value != distance[vertex])
                    continue;
                for (const AdjEdge& edge : graph.adj[vertex])
                {
                    const int reverse_arc = ArcIndex(edge.edge_id, edge.to, vertex);
                    const double next = value + residual_[reverse_arc];
                    if (next < distance[edge.to])
                    {
                        distance[edge.to] = next;
                        heap.push({next, edge.to});
                    }
                }
            }
            double cap = 0.0;
            for (int vertex : paid_path)
                cap = std::max(cap, distance[vertex]);
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                distance[vertex] = std::min(distance[vertex], cap);
        }

        std::vector<double> scale(g);
        std::vector<char> active(g, 1);
        std::vector<double> load(residual_.size());
        // Max-min filling freezes exactly the groups using a saturated arc.
        for (int round = 0; round < g; ++round)
        {
            std::fill(load.begin(), load.end(), 0.0);
            double step = 1.0;
            for (int group = 0; group < g; ++group)
                if (active[group])
                    step = std::min(step, 1.0 - scale[group]);

            for (const UndirectedEdge& edge : graph.edges)
            {
                const int forward = ArcIndex(edge.id, edge.u, edge.v);
                const int backward = ArcIndex(edge.id, edge.v, edge.u);
                for (int group = 0; group < g; ++group)
                {
                    if (!active[group])
                        continue;
                    load[forward] += std::max(
                        0.0, direction[group][edge.u] - direction[group][edge.v]);
                    load[backward] += std::max(
                        0.0, direction[group][edge.v] - direction[group][edge.u]);
                }
                if (load[forward] > 0.0)
                    step = std::min(step, residual_[forward] / load[forward]);
                if (load[backward] > 0.0)
                    step = std::min(step, residual_[backward] / load[backward]);
            }
            step = std::max(0.0, step);
            ++result.rounds;
            for (int group = 0; group < g; ++group)
                if (active[group])
                    scale[group] = std::min(1.0, scale[group] + step);
            for (size_t arc = 0; arc < residual_.size(); ++arc)
                residual_[arc] = std::max(0.0, residual_[arc] - step * load[arc]);

            std::vector<char> freeze(g);
            for (int group = 0; group < g; ++group)
                if (active[group] && scale[group] >= 1.0 - 1e-12)
                    freeze[group] = 1;
            for (const UndirectedEdge& edge : graph.edges)
            {
                const int arcs[2] = {
                    ArcIndex(edge.id, edge.u, edge.v),
                    ArcIndex(edge.id, edge.v, edge.u)};
                const int from[2] = {edge.u, edge.v};
                const int to[2] = {edge.v, edge.u};
                const double tolerance = 1e-10 * std::max(1.0, edge.w);
                for (int direction_index = 0; direction_index < 2; ++direction_index)
                {
                    const int arc = arcs[direction_index];
                    if (residual_[arc] > tolerance || load[arc] <= 0.0)
                        continue;
                    for (int group = 0; group < g; ++group)
                        if (active[group] &&
                            direction[group][from[direction_index]] >
                                direction[group][to[direction_index]])
                            freeze[group] = 1;
                }
            }

            bool any_active = false;
            bool any_frozen = false;
            for (int group = 0; group < g; ++group)
            {
                if (freeze[group])
                {
                    active[group] = 0;
                    any_frozen = true;
                }
                any_active = any_active || active[group];
            }
            if (!any_active)
                break;
            if (!any_frozen)
                throw std::runtime_error(
                    "Progressive packing found no saturated constraint.");
        }

        for (int group = 0; group < g; ++group)
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                potential_[group][vertex] += scale[group] * direction[group][vertex];
        result.min_scale = *std::min_element(scale.begin(), scale.end());
        result.max_scale = *std::max_element(scale.begin(), scale.end());
        result.average_scale =
            std::accumulate(scale.begin(), scale.end(), 0.0) / g;
        ReleaseResidual();
        return result;
    }

    void ReleaseResidual()
    {
        residual_.clear();
        residual_.shrink_to_fit();
    }

    void BuildFromGroup(const Graph& graph,
                        const Query& query,
                        const std::vector<std::vector<double>>& group_distance,
                        int anchor_group)
    {
        BuildInternal(graph, query, group_distance, query.groups[anchor_group], false);
        primal_upper_ = fp::kInf;
        residual_.clear();
        residual_.shrink_to_fit();
    }

    double At(int vertex, int mask) const
    {
        double value = 0.0;
        while (mask)
        {
            const int bit = mask & -mask;
            value += potential_[FirstBit(bit)][vertex];
            mask ^= bit;
        }
        return value;
    }

    double Objective() const
    {
        return objective_;
    }

    double PrimalUpper() const
    {
        return primal_upper_;
    }

    const std::vector<int>& Order() const
    {
        return order_;
    }

private:
    void BuildInternal(const Graph& graph,
                       const Query& query,
                       const std::vector<std::vector<double>>& group_distance,
                       const std::vector<int>& roots,
                       bool dynamic_order)
    {
        using HeapItem = std::pair<double, int>;

        const int n = graph.n;
        const int g = static_cast<int>(query.groups.size());
        objective_ = 0.0;
        potential_.assign(g, std::vector<double>(n + 1));
        order_.clear();
        std::vector<int> static_order(g);
        std::iota(static_order.begin(), static_order.end(), 0);
        if (!dynamic_order)
        {
            std::vector<double> root_group_distance(g, fp::kInf);
            for (int group = 0; group < g; ++group)
                for (int root : roots)
                    root_group_distance[group] =
                        std::min(root_group_distance[group], group_distance[group][root]);
            std::sort(static_order.begin(), static_order.end(), [&](int a, int b)
            {
                if (root_group_distance[a] != root_group_distance[b])
                    return root_group_distance[a] > root_group_distance[b];
                return a < b;
            });
            order_ = static_order;
        }

        residual_.assign(static_cast<size_t>(2) * graph.m, 0.0);
        for (const auto& edge : graph.edges)
        {
            residual_[2 * edge.id] = edge.w;
            residual_[2 * edge.id + 1] = edge.w;
        }

        std::vector<double> distance(n + 1);
        std::vector<double> capped(n + 1);
        std::vector<char> processed(g);
        for (int order_index = 0; order_index < g; ++order_index)
        {
            const int group = dynamic_order
                                  ? SelectFarthestGroup(graph, query, roots, processed)
                                  : order_[order_index];
            if (dynamic_order)
                order_.push_back(group);
            processed[group] = 1;
            distance = group_distance[group];
            std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
            if (order_index > 0)
            {
                for (const auto& edge : graph.edges)
                {
                    const double to_u =
                        residual_[ArcIndex(edge.id, edge.u, edge.v)] + distance[edge.v];
                    if (to_u < distance[edge.u])
                    {
                        distance[edge.u] = to_u;
                        heap.push({to_u, edge.u});
                    }
                    const double to_v =
                        residual_[ArcIndex(edge.id, edge.v, edge.u)] + distance[edge.u];
                    if (to_v < distance[edge.v])
                    {
                        distance[edge.v] = to_v;
                        heap.push({to_v, edge.v});
                    }
                }
            }
            while (!heap.empty())
            {
                const auto [d, v] = heap.top();
                heap.pop();
                if (d != distance[v])
                    continue;
                for (const auto& edge : graph.adj[v])
                {
                    const int u = edge.to;
                    const int arc = ArcIndex(edge.edge_id, u, v);
                    const double next = d + residual_[arc];
                    if (next < distance[u])
                    {
                        distance[u] = next;
                        heap.push({next, u});
                    }
                }
            }

            double root_distance = fp::kInf;
            for (int root : roots)
                root_distance = std::min(root_distance, distance[root]);
            objective_ += root_distance;
            for (int v = 1; v <= n; ++v)
            {
                capped[v] = std::min(distance[v], root_distance);
                potential_[group][v] = capped[v];
            }
            for (const auto& edge : graph.edges)
            {
                const double forward = std::max(0.0, capped[edge.u] - capped[edge.v]);
                const double backward = std::max(0.0, capped[edge.v] - capped[edge.u]);
                const int forward_arc = ArcIndex(edge.id, edge.u, edge.v);
                const int backward_arc = ArcIndex(edge.id, edge.v, edge.u);
                residual_[forward_arc] =
                    std::max(0.0, residual_[forward_arc] - forward);
                residual_[backward_arc] =
                    std::max(0.0, residual_[backward_arc] - backward);
            }
        }
    }

    int SelectFarthestGroup(const Graph& graph,
                            const Query& query,
                            const std::vector<int>& roots,
                            const std::vector<char>& processed) const
    {
        using HeapItem = std::pair<double, int>;

        std::vector<double> distance(graph.n + 1, fp::kInf);
        std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
        for (int root : roots)
        {
            distance[root] = 0.0;
            heap.push({0.0, root});
        }
        while (!heap.empty())
        {
            const auto [d, u] = heap.top();
            heap.pop();
            if (d != distance[u])
                continue;
            for (const auto& edge : graph.adj[u])
            {
                const int arc = ArcIndex(edge.edge_id, u, edge.to);
                const double next = d + residual_[arc];
                if (next < distance[edge.to])
                {
                    distance[edge.to] = next;
                    heap.push({next, edge.to});
                }
            }
        }

        int farthest_group = -1;
        double farthest_distance = -1.0;
        for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
        {
            if (processed[group])
                continue;
            double value = fp::kInf;
            for (int v : query.groups[group])
                value = std::min(value, distance[v]);
            if (value > farthest_distance ||
                (value == farthest_distance && group < farthest_group))
            {
                farthest_distance = value;
                farthest_group = group;
            }
        }
        return farthest_group;
    }
    static double RecoverPrimal(const Graph& graph,
                                const Query& query,
                                int root,
                                const std::vector<double>& residual)
    {
        using HeapItem = std::pair<double, int>;

        const int g = static_cast<int>(query.groups.size());
        const int full_mask = (1 << g) - 1;
        std::vector<int> color(graph.n + 1);
        for (int group = 0; group < g; ++group)
            for (int v : query.groups[group])
                color[v] |= 1 << group;

        std::vector<char> in_tree(graph.n + 1);
        std::vector<int> parent(graph.n + 1);
        std::vector<double> distance(graph.n + 1, fp::kInf);
        in_tree[root] = 1;
        int covered = color[root];
        double cost = 0.0;
        while (covered != full_mask)
        {
            std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
            std::fill(distance.begin(), distance.end(), fp::kInf);
            for (int v = 1; v <= graph.n; ++v)
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
                    break;
                }
                for (const auto& edge : graph.adj[u])
                {
                    const int arc = ArcIndex(edge.edge_id, u, edge.to);
                    const double zero_tolerance = 1e-10 * std::max(1.0, edge.w);
                    if (residual[arc] > zero_tolerance)
                        continue;
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
                return fp::kInf;
            cost += distance[found];
            for (int v = found; v && !in_tree[v]; v = parent[v])
            {
                in_tree[v] = 1;
                covered |= color[v];
            }
        }
        return cost;
    }

    static int FirstBit(int mask)
    {
#if defined(_MSC_VER)
        unsigned long index = 0;
        _BitScanForward(&index, static_cast<unsigned long>(mask));
        return static_cast<int>(index);
#else
        return __builtin_ctz(static_cast<unsigned int>(mask));
#endif
    }

    static int ArcIndex(int edge_id, int from, int to)
    {
        return 2 * edge_id + (from < to ? 0 : 1);
    }

    std::vector<std::vector<double>> potential_;
    std::vector<int> order_;
    std::vector<double> residual_;
    double objective_ = 0.0;
    double primal_upper_ = fp::kInf;
};
}  // namespace gst::methods::dual_cut

#endif
