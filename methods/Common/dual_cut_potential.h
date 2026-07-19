#ifndef GST_METHODS_COMMON_DUAL_CUT_POTENTIAL_H
#define GST_METHODS_COMMON_DUAL_CUT_POTENTIAL_H

#include <algorithm>
#include <cstdint>
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
        BuildInternal<false>(graph, query, group_distance, std::vector<int>{root}, false);
        primal_upper_ = RecoverPrimal(graph, query, root, residual_);
        residual_.clear();
        residual_.shrink_to_fit();
    }

    void BuildDynamic(const Graph& graph,
                      const Query& query,
                      const std::vector<std::vector<double>>& group_distance,
                      int root)
    {
        BuildInternal<false>(graph, query, group_distance, std::vector<int>{root}, true);
        primal_upper_ = RecoverPrimal(graph, query, root, residual_);
        residual_.clear();
        residual_.shrink_to_fit();
    }

    void BuildKeepingResidual(const Graph& graph,
                              const Query& query,
                              const std::vector<std::vector<double>>& group_distance,
                              int root)
    {
        BuildInternal<false>(graph, query, group_distance, std::vector<int>{root}, false);
        primal_upper_ = RecoverPrimal(graph, query, root, residual_);
    }

    void BuildKeepingResidualChangedArcs(
        const Graph& graph,
        const Query& query,
        const std::vector<std::vector<double>>& group_distance,
        int root)
    {
        BuildInternal<true>(graph, query, group_distance, std::vector<int>{root}, false);
        primal_upper_ = RecoverPrimal(graph, query, root, residual_);
    }

    void BeginProgressiveChangedArcs(
        const Graph& graph,
        const Query& query,
        const std::vector<std::vector<double>>& group_distance,
        int root)
    {
        const int g = static_cast<int>(query.groups.size());
        objective_ = 0.0;
        primal_upper_ = fp::kInf;
        seed_arc_scans_ = 0;
        full_seed_arc_scans_ = 0;
        changed_arcs_ = 0;
        potential_.assign(g, {});
        order_.resize(g);
        std::iota(order_.begin(), order_.end(), 0);
        std::sort(order_.begin(), order_.end(), [&](int left, int right)
        {
            if (group_distance[left][root] != group_distance[right][root])
                return group_distance[left][root] > group_distance[right][root];
            return left < right;
        });

        residual_.assign(static_cast<size_t>(2) * graph.m, 0.0);
        for (const auto& edge : graph.edges)
        {
            residual_[2 * edge.id] = edge.w;
            residual_[2 * edge.id + 1] = edge.w;
        }
        progressive_changed_arc_words_.assign(
            (static_cast<size_t>(2) * graph.m + 63) / 64, 0);
        progressive_group_distance_ = &group_distance;
        progressive_root_ = root;
        progressive_index_ = 0;
    }

    bool AdvanceProgressiveChangedArcs(const Graph& graph)
    {
        if (!progressive_group_distance_ || progressive_index_ >= order_.size())
            return false;

        using HeapItem = std::pair<double, int>;
        const int group = order_[progressive_index_];
        std::vector<double> distance = (*progressive_group_distance_)[group];
        std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
        if (progressive_index_ > 0)
        {
            full_seed_arc_scans_ += static_cast<long long>(2) * graph.m;
            for (size_t word_index = 0;
                 word_index < progressive_changed_arc_words_.size();
                 ++word_index)
            {
                std::uint64_t bits = progressive_changed_arc_words_[word_index];
                while (bits)
                {
#if defined(_MSC_VER)
                    unsigned long offset = 0;
                    _BitScanForward64(&offset, bits);
#else
                    const int offset = __builtin_ctzll(bits);
#endif
                    const int arc = static_cast<int>(word_index * 64 + offset);
                    bits &= bits - 1;
                    if (arc >= 2 * graph.m)
                        continue;
                    ++seed_arc_scans_;
                    const UndirectedEdge& edge = graph.edges[arc / 2];
                    const bool forward = ArcIndex(edge.id, edge.u, edge.v) == arc;
                    const int target = forward ? edge.u : edge.v;
                    const int source = forward ? edge.v : edge.u;
                    const double next = residual_[arc] + distance[source];
                    if (next < distance[target])
                    {
                        distance[target] = next;
                        heap.push({next, target});
                    }
                }
            }
        }
        while (!heap.empty())
        {
            const auto [value, vertex] = heap.top();
            heap.pop();
            if (value != distance[vertex])
                continue;
            for (const AdjEdge& edge : graph.adj[vertex])
            {
                const int arc = ArcIndex(edge.edge_id, edge.to, vertex);
                const double next = value + residual_[arc];
                if (next < distance[edge.to])
                {
                    distance[edge.to] = next;
                    heap.push({next, edge.to});
                }
            }
        }

        const double root_distance = distance[progressive_root_];
        objective_ += root_distance;
        auto& group_potential = potential_[group];
        group_potential.assign(graph.n + 1, 0.0);
        auto MarkChangedArc = [&](int arc)
        {
            std::uint64_t& word =
                progressive_changed_arc_words_[static_cast<size_t>(arc) >> 6];
            const std::uint64_t bit = std::uint64_t{1} << (arc & 63);
            if (!(word & bit))
            {
                word |= bit;
                ++changed_arcs_;
            }
        };
        for (int vertex = 1; vertex <= graph.n; ++vertex)
            group_potential[vertex] = std::min(distance[vertex], root_distance);
        for (const auto& edge : graph.edges)
        {
            const double forward = std::max(
                0.0, group_potential[edge.u] - group_potential[edge.v]);
            const double backward = std::max(
                0.0, group_potential[edge.v] - group_potential[edge.u]);
            const int forward_arc = ArcIndex(edge.id, edge.u, edge.v);
            const int backward_arc = ArcIndex(edge.id, edge.v, edge.u);
            if (forward > 0.0)
                MarkChangedArc(forward_arc);
            if (backward > 0.0)
                MarkChangedArc(backward_arc);
            residual_[forward_arc] =
                std::max(0.0, residual_[forward_arc] - forward);
            residual_[backward_arc] =
                std::max(0.0, residual_[backward_arc] - backward);
        }
        ++progressive_index_;
        return true;
    }

    bool ProgressiveComplete() const
    {
        return progressive_group_distance_ && progressive_index_ == order_.size();
    }

    int ProgressiveGroupsBuilt() const
    {
        return static_cast<int>(progressive_index_);
    }

    void RecoverProgressivePrimal(const Graph& graph,
                                  const Query& query,
                                  int root)
    {
        if (!ProgressiveComplete())
            throw std::runtime_error("Cannot recover primal from an incomplete dual.");
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
        BuildInternal<false>(
            graph, query, group_distance, query.groups[anchor_group], false);
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
            const auto& group_potential = potential_[FirstBit(bit)];
            if (!group_potential.empty())
                value += group_potential[vertex];
            mask ^= bit;
        }
        return value;
    }

    double GroupAt(int vertex, int group) const
    {
        return potential_[group].empty() ? 0.0 : potential_[group][vertex];
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

    long long SeedArcScans() const { return seed_arc_scans_; }
    long long FullSeedArcScans() const { return full_seed_arc_scans_; }
    long long ChangedArcs() const { return changed_arcs_; }

private:
    template <bool changed_arc_seeds>
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
        seed_arc_scans_ = 0;
        full_seed_arc_scans_ = 0;
        changed_arcs_ = 0;
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
        // Original group distances satisfy every untouched arc.  Only arcs
        // affected by an earlier potential can seed a residual-distance repair.
        std::vector<std::uint64_t> changed_arc_words;
        if constexpr (changed_arc_seeds)
            changed_arc_words.resize((static_cast<size_t>(2) * graph.m + 63) / 64);
        auto MarkChangedArc = [&](int arc)
        {
            std::uint64_t& word = changed_arc_words[static_cast<size_t>(arc) >> 6];
            const std::uint64_t bit = std::uint64_t{1} << (arc & 63);
            if (!(word & bit))
            {
                word |= bit;
                ++changed_arcs_;
            }
        };
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
                if constexpr (changed_arc_seeds)
                {
                    full_seed_arc_scans_ += static_cast<long long>(2) * graph.m;
                    for (size_t word_index = 0; word_index < changed_arc_words.size();
                         ++word_index)
                    {
                        std::uint64_t bits = changed_arc_words[word_index];
                        while (bits)
                        {
#if defined(_MSC_VER)
                            unsigned long offset = 0;
                            _BitScanForward64(&offset, bits);
#else
                            const int offset = __builtin_ctzll(bits);
#endif
                            const int arc = static_cast<int>(word_index * 64 + offset);
                            bits &= bits - 1;
                            if (arc >= 2 * graph.m)
                                continue;
                            ++seed_arc_scans_;
                            const UndirectedEdge& edge = graph.edges[arc / 2];
                            const bool forward = ArcIndex(edge.id, edge.u, edge.v) == arc;
                            const int target = forward ? edge.u : edge.v;
                            const int source = forward ? edge.v : edge.u;
                            const double next = residual_[arc] + distance[source];
                            if (next < distance[target])
                            {
                                distance[target] = next;
                                heap.push({next, target});
                            }
                        }
                    }
                }
                else
                {
                    full_seed_arc_scans_ += static_cast<long long>(2) * graph.m;
                    seed_arc_scans_ += static_cast<long long>(2) * graph.m;
                    for (const auto& edge : graph.edges)
                    {
                        const double to_u =
                            residual_[ArcIndex(edge.id, edge.u, edge.v)] +
                            distance[edge.v];
                        if (to_u < distance[edge.u])
                        {
                            distance[edge.u] = to_u;
                            heap.push({to_u, edge.u});
                        }
                        const double to_v =
                            residual_[ArcIndex(edge.id, edge.v, edge.u)] +
                            distance[edge.u];
                        if (to_v < distance[edge.v])
                        {
                            distance[edge.v] = to_v;
                            heap.push({to_v, edge.v});
                        }
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
                if constexpr (changed_arc_seeds)
                {
                    if (forward > 0.0)
                        MarkChangedArc(forward_arc);
                    if (backward > 0.0)
                        MarkChangedArc(backward_arc);
                }
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
    long long seed_arc_scans_ = 0;
    long long full_seed_arc_scans_ = 0;
    long long changed_arcs_ = 0;
    const std::vector<std::vector<double>>* progressive_group_distance_ = nullptr;
    std::vector<std::uint64_t> progressive_changed_arc_words_;
    size_t progressive_index_ = 0;
    int progressive_root_ = 0;
};
}  // namespace gst::methods::dual_cut

#endif
