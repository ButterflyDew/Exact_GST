#include "release_v2.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "../../float_compare.h"

namespace gst::methods::release_v2
{
namespace
{
using Clock = std::chrono::steady_clock;
using DistanceItem = std::pair<double, int>;
using DistanceHeap = std::priority_queue<DistanceItem,
                                         std::vector<DistanceItem>,
                                         std::greater<DistanceItem>>;

int FirstBit(std::uint64_t bits)
{
#if defined(_MSC_VER)
    unsigned long index = 0;
    _BitScanForward64(&index, bits);
    return static_cast<int>(index);
#else
    return __builtin_ctzll(bits);
#endif
}

int BitCount(int mask)
{
    int count = 0;
    for (; mask; mask &= mask - 1)
        ++count;
    return count;
}

struct HeapNode
{
    double key = 0.0;
    int root = 0;
    int mask = 0;

    bool operator>(const HeapNode& other) const
    {
        if (key != other.key)
            return key > other.key;
        if (mask != other.mask)
            return mask > other.mask;
        return root > other.root;
    }
};

struct Label
{
    double cost = fp::kInf;
    double lower = 0.0;
    int mask = 0;
    bool settled = false;
};

// Zero is the empty-slot sentinel; anchored search stores only nonempty masks.
class LabelMap
{
public:
    Label* Find(int mask)
    {
        if (table_.empty())
            return nullptr;
        std::size_t slot = Hash(mask) & (table_.size() - 1);
        while (table_[slot].mask)
        {
            if (table_[slot].mask == mask)
                return &table_[slot];
            slot = (slot + 1) & (table_.size() - 1);
        }
        return nullptr;
    }

    Label* Insert(int mask)
    {
        if (table_.empty())
            Rehash(8);
        else if ((size_ + 1) * 2 > table_.size())
            Rehash(table_.size() * 2);

        std::size_t slot = Hash(mask) & (table_.size() - 1);
        while (table_[slot].mask)
            slot = (slot + 1) & (table_.size() - 1);
        table_[slot].mask = mask;
        ++size_;
        return &table_[slot];
    }

private:
    static std::uint32_t Hash(int mask)
    {
        std::uint32_t value = static_cast<std::uint32_t>(mask);
        value ^= value >> 16;
        value *= 0x7feb352dU;
        value ^= value >> 15;
        value *= 0x846ca68bU;
        return value ^ (value >> 16);
    }

    void Rehash(std::size_t capacity)
    {
        std::vector<Label> old = std::move(table_);
        table_.assign(capacity, Label{});
        for (const Label& label : old)
        {
            if (!label.mask)
                continue;
            std::size_t slot = Hash(label.mask) & (table_.size() - 1);
            while (table_[slot].mask)
                slot = (slot + 1) & (table_.size() - 1);
            table_[slot] = label;
        }
    }

    std::vector<Label> table_;
    std::size_t size_ = 0;
};

struct DisjointIndex
{
    std::vector<int> masks;
    std::vector<double> costs;
    std::vector<std::uint64_t> contains;

    void Insert(int mask, double cost, int group_count)
    {
        const std::size_t id = masks.size();
        const std::size_t word = id / 64;
        const std::size_t required = (word + 1) * static_cast<std::size_t>(group_count);
        if (contains.size() < required)
            contains.resize(required, 0);

        masks.push_back(mask);
        costs.push_back(cost);
        const std::uint64_t id_bit = std::uint64_t{1} << (id % 64);
        for (int bits = mask; bits; bits &= bits - 1)
        {
            const int bit = FirstBit(static_cast<std::uint32_t>(bits));
            contains[word * static_cast<std::size_t>(group_count) + bit] |= id_bit;
        }
    }
};

std::vector<std::vector<double>> GroupDistances(const Graph& graph, const Query& query)
{
    const int group_count = static_cast<int>(query.groups.size());
    std::vector<std::vector<double>> result(
        group_count, std::vector<double>(graph.n + 1, fp::kInf));

    for (int group = 0; group < group_count; ++group)
    {
        DistanceHeap heap;
        for (int vertex : query.groups[group])
        {
            if (result[group][vertex] == 0.0)
                continue;
            result[group][vertex] = 0.0;
            heap.push({0.0, vertex});
        }
        while (!heap.empty())
        {
            const auto [distance, vertex] = heap.top();
            heap.pop();
            if (distance != result[group][vertex])
                continue;
            for (const AdjEdge& edge : graph.adj[vertex])
            {
                const double next = distance + edge.w;
                if (next < result[group][edge.to])
                {
                    result[group][edge.to] = next;
                    heap.push({next, edge.to});
                }
            }
        }
    }
    return result;
}

std::vector<std::vector<double>> GroupMetric(
    const Query& query,
    const std::vector<std::vector<double>>& group_distance)
{
    const int group_count = static_cast<int>(query.groups.size());
    std::vector<std::vector<double>> metric(
        group_count, std::vector<double>(group_count, fp::kInf));
    for (int left = 0; left < group_count; ++left)
        for (int right = 0; right < group_count; ++right)
            for (int vertex : query.groups[right])
                metric[left][right] = std::min(metric[left][right],
                                               group_distance[left][vertex]);
    return metric;
}

class TourLowerBound
{
public:
    void Build(const std::vector<std::vector<double>>& metric)
    {
        const int group_count = static_cast<int>(metric.size());
        const int subset_count = 1 << group_count;
        const int full_mask = subset_count - 1;
        endpoints_.assign(subset_count, {});
        std::vector<double> path(
            static_cast<std::size_t>(subset_count) * group_count * group_count, fp::kInf);
        auto Path = [&](int mask, int start, int last) -> double&
        {
            return path[(static_cast<std::size_t>(mask) * group_count + start) *
                            group_count +
                        last];
        };

        for (int start = 0; start < group_count; ++start)
        {
            Path(1 << start, start, start) = 0.0;
            for (int mask = 1; mask < subset_count; ++mask)
            {
                if (!(mask & (1 << start)))
                    continue;
                for (int last = 0; last < group_count; ++last)
                {
                    if (!(mask & (1 << last)))
                        continue;
                    const double current = Path(mask, start, last);
                    if (current >= fp::kInf / 4)
                        continue;
                    for (int bits = full_mask ^ mask; bits; bits &= bits - 1)
                    {
                        const int bit = bits & -bits;
                        const int next = FirstBit(static_cast<std::uint32_t>(bit));
                        double& destination = Path(mask | bit, start, next);
                        destination = std::min(destination, current + metric[last][next]);
                    }
                }
            }
        }

        for (int mask = 1; mask < subset_count; ++mask)
        {
            const int size = BitCount(mask);
            if (size <= 1)
                continue;
            endpoints_[mask].reserve(size * (size - 1) / 2);
            for (int left = 0; left < group_count; ++left)
            {
                if (!(mask & (1 << left)))
                    continue;
                for (int right = left + 1; right < group_count; ++right)
                {
                    if (!(mask & (1 << right)))
                        continue;
                    const double value = std::min(Path(mask, left, right),
                                                  Path(mask, right, left));
                    if (value < fp::kInf / 4)
                        endpoints_[mask].push_back({left, right, value});
                }
            }
        }
    }

    double At(int vertex,
              int mask,
              const std::vector<std::vector<double>>& group_distance) const
    {
        if (!mask)
            return 0.0;
        if (!(mask & (mask - 1)))
            return group_distance[FirstBit(static_cast<std::uint32_t>(mask))][vertex];

        double answer = fp::kInf;
        for (const Endpoint& endpoint : endpoints_[mask])
        {
            answer = std::min(answer,
                              group_distance[endpoint.left][vertex] + endpoint.path +
                                  group_distance[endpoint.right][vertex]);
        }
        return answer * 0.5;
    }

private:
    struct Endpoint
    {
        int left = 0;
        int right = 0;
        double path = 0.0;
    };

    std::vector<std::vector<Endpoint>> endpoints_;
};

int ArcIndex(int edge_id, int from, int to)
{
    return 2 * edge_id + (from < to ? 0 : 1);
}

double GrowTree(const Graph& graph,
                const std::vector<int>& color,
                int full_mask,
                int start,
                const std::vector<double>* residual,
                std::vector<int>* covering_vertices)
{
    std::vector<char> in_tree(graph.n + 1);
    std::vector<int> parent(graph.n + 1);
    std::vector<double> distance(graph.n + 1, fp::kInf);
    std::vector<int> sources{start};
    in_tree[start] = 1;
    int covered = color[start];
    double cost = 0.0;

    if (covering_vertices)
    {
        covering_vertices->clear();
        if (covered)
            covering_vertices->push_back(start);
    }

    while (covered != full_mask)
    {
        std::fill(distance.begin(), distance.end(), fp::kInf);
        std::fill(parent.begin(), parent.end(), 0);
        DistanceHeap heap;
        for (int source : sources)
        {
            distance[source] = 0.0;
            heap.push({0.0, source});
        }

        int found = 0;
        while (!heap.empty())
        {
            const auto [current, vertex] = heap.top();
            heap.pop();
            if (current != distance[vertex])
                continue;
            if (color[vertex] & (full_mask ^ covered))
            {
                found = vertex;
                cost += current;
                break;
            }
            for (const AdjEdge& edge : graph.adj[vertex])
            {
                if (residual)
                {
                    const double tolerance = 1e-10 * std::max(1.0, edge.w);
                    if ((*residual)[ArcIndex(edge.edge_id, vertex, edge.to)] > tolerance)
                        continue;
                }
                const double next = current + edge.w;
                if (next < distance[edge.to])
                {
                    distance[edge.to] = next;
                    parent[edge.to] = vertex;
                    heap.push({next, edge.to});
                }
            }
        }
        if (!found)
            return fp::kInf;

        for (int vertex = found; !in_tree[vertex]; vertex = parent[vertex])
        {
            const int newly_covered = color[vertex] & (full_mask ^ covered);
            in_tree[vertex] = 1;
            sources.push_back(vertex);
            covered |= color[vertex];
            if (covering_vertices && newly_covered)
                covering_vertices->push_back(vertex);
        }
    }
    return cost;
}

class DualPotential
{
public:
    double Build(const Graph& graph,
                 const Query& query,
                 const std::vector<std::vector<double>>& group_distance,
                 const std::vector<int>& color,
                 int root)
    {
        const int group_count = static_cast<int>(query.groups.size());
        potential_.assign(group_count, std::vector<double>(graph.n + 1));
        std::vector<int> order(group_count);
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int left, int right)
        {
            if (group_distance[left][root] != group_distance[right][root])
                return group_distance[left][root] > group_distance[right][root];
            return left < right;
        });

        std::vector<double> residual(static_cast<std::size_t>(2) * graph.m);
        for (const UndirectedEdge& edge : graph.edges)
        {
            residual[2 * edge.id] = edge.w;
            residual[2 * edge.id + 1] = edge.w;
        }

        std::vector<double> distance(graph.n + 1);
        std::vector<double> capped(graph.n + 1);
        // Each group consumes part of the same directed residual capacities.
        for (int order_index = 0; order_index < group_count; ++order_index)
        {
            const int group = order[order_index];
            distance = group_distance[group];
            DistanceHeap heap;
            if (order_index > 0)
            {
                for (const UndirectedEdge& edge : graph.edges)
                {
                    const double to_u = residual[ArcIndex(edge.id, edge.u, edge.v)] +
                                        distance[edge.v];
                    if (to_u < distance[edge.u])
                    {
                        distance[edge.u] = to_u;
                        heap.push({to_u, edge.u});
                    }
                    const double to_v = residual[ArcIndex(edge.id, edge.v, edge.u)] +
                                        distance[edge.u];
                    if (to_v < distance[edge.v])
                    {
                        distance[edge.v] = to_v;
                        heap.push({to_v, edge.v});
                    }
                }
            }
            while (!heap.empty())
            {
                const auto [current, vertex] = heap.top();
                heap.pop();
                if (current != distance[vertex])
                    continue;
                for (const AdjEdge& edge : graph.adj[vertex])
                {
                    const int predecessor = edge.to;
                    const double next = current +
                                        residual[ArcIndex(edge.edge_id, predecessor, vertex)];
                    if (next < distance[predecessor])
                    {
                        distance[predecessor] = next;
                        heap.push({next, predecessor});
                    }
                }
            }

            const double root_distance = distance[root];
            for (int vertex = 1; vertex <= graph.n; ++vertex)
            {
                capped[vertex] = std::min(distance[vertex], root_distance);
                potential_[group][vertex] = capped[vertex];
            }
            for (const UndirectedEdge& edge : graph.edges)
            {
                const double forward = std::max(0.0, capped[edge.u] - capped[edge.v]);
                const double backward = std::max(0.0, capped[edge.v] - capped[edge.u]);
                const int forward_arc = ArcIndex(edge.id, edge.u, edge.v);
                const int backward_arc = ArcIndex(edge.id, edge.v, edge.u);
                residual[forward_arc] = std::max(0.0, residual[forward_arc] - forward);
                residual[backward_arc] = std::max(0.0, residual[backward_arc] - backward);
            }
        }

        return GrowTree(graph, color, (1 << group_count) - 1, root, &residual, nullptr);
    }

    double At(int vertex, int mask) const
    {
        double answer = 0.0;
        for (int bits = mask; bits; bits &= bits - 1)
        {
            const int group = FirstBit(static_cast<std::uint32_t>(bits));
            answer += potential_[group][vertex];
        }
        return answer;
    }

private:
    std::vector<std::vector<double>> potential_;
};

double RootStarUpper(const std::vector<std::vector<double>>& group_distance,
                     int vertex_count,
                     int& root)
{
    double best = fp::kInf;
    for (int vertex = 1; vertex <= vertex_count; ++vertex)
    {
        double candidate = 0.0;
        for (const auto& distance : group_distance)
            candidate += distance[vertex];
        if (candidate < best)
        {
            best = candidate;
            root = vertex;
        }
    }
    return best;
}

std::vector<double> GroupMstHalf(const std::vector<std::vector<double>>& metric,
                                 const std::vector<int>& popcount)
{
    const int group_count = static_cast<int>(metric.size());
    const int subset_count = 1 << group_count;
    std::vector<double> result(subset_count);
    for (int mask = 1; mask < subset_count; ++mask)
    {
        if (popcount[mask] <= 1)
            continue;
        std::vector<double> distance(group_count, fp::kInf);
        std::vector<char> used(group_count);
        distance[FirstBit(static_cast<std::uint32_t>(mask))] = 0.0;
        double sum = 0.0;
        for (int iteration = 0; iteration < popcount[mask]; ++iteration)
        {
            int next = -1;
            for (int group = 0; group < group_count; ++group)
            {
                if ((mask & (1 << group)) && !used[group] &&
                    (next < 0 || distance[group] < distance[next]))
                    next = group;
            }
            used[next] = 1;
            sum += distance[next];
            for (int group = 0; group < group_count; ++group)
            {
                if ((mask & (1 << group)) && !used[group])
                    distance[group] = std::min(distance[group], metric[next][group]);
            }
        }
        result[mask] = sum * 0.5;
    }
    return result;
}

}  // namespace

SolveResult SolveOneQuery(const Graph& graph, const Query& query)
{
    const auto total_begin = Clock::now();
    SolveResult result;
    ReleaseStats& stats = result.stats;
    stats.n = graph.n;
    stats.m = graph.m;
    stats.g = static_cast<int>(query.groups.size());

    const int group_count = stats.g;
    if (group_count < 1 || group_count > 20)
        throw std::runtime_error("ReleaseV2 supports 1 to 20 groups.");
    for (const auto& group : query.groups)
        if (group.empty())
        {
            stats.total_ms =
                std::chrono::duration<double, std::milli>(Clock::now() - total_begin).count();
            return result;
        }
    if (group_count == 1)
    {
        result.feasible = true;
        result.best_weight = 0.0;
        stats.total_ms = std::chrono::duration<double, std::milli>(Clock::now() - total_begin).count();
        return result;
    }

    const int full_mask = (1 << group_count) - 1;
    const int anchor_bit = 1;
    const int label_full = full_mask ^ anchor_bit;
    std::vector<int> popcount(1 << group_count);
    for (int mask = 1; mask <= full_mask; ++mask)
        popcount[mask] = popcount[mask >> 1] + (mask & 1);

    auto phase_begin = Clock::now();
    const auto group_distance = GroupDistances(graph, query);
    stats.group_distance_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - phase_begin).count();

    phase_begin = Clock::now();
    const auto metric = GroupMetric(query, group_distance);
    TourLowerBound tour;
    tour.Build(metric);
    const auto mst_half = GroupMstHalf(metric, popcount);
    stats.lower_bound_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - phase_begin).count();

    std::vector<int> color(graph.n + 1);
    for (int group = 0; group < group_count; ++group)
        for (int vertex : query.groups[group])
            color[vertex] |= 1 << group;

    phase_begin = Clock::now();
    int dual_root = 1;
    double best = RootStarUpper(group_distance, graph.n, dual_root);
    if (best >= fp::kInf / 4)
    {
        stats.total_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - total_begin).count();
        return result;
    }
    std::vector<int> greedy_roots;
    best = std::min(best,
                    GrowTree(graph, color, full_mask, dual_root, nullptr, &greedy_roots));
    std::sort(greedy_roots.begin(), greedy_roots.end());
    greedy_roots.erase(std::unique(greedy_roots.begin(), greedy_roots.end()),
                       greedy_roots.end());
    for (int root : greedy_roots)
        best = std::min(best, GrowTree(graph, color, full_mask, root, nullptr, nullptr));
    stats.upper_bound_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - phase_begin).count();

    phase_begin = Clock::now();
    DualPotential dual;
    best = std::min(best, dual.Build(graph, query, group_distance, color, dual_root));
    stats.dual_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - phase_begin).count();

    phase_begin = Clock::now();
    std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> heap;
    std::vector<LabelMap> labels_at_root(graph.n + 1);
    std::vector<std::unique_ptr<DisjointIndex>> disjoint_at_root(graph.n + 1);
    long long open_labels = 0;

    auto StarExtension = [&](int root, int remaining)
    {
        double answer = 0.0;
        for (int bits = remaining; bits; bits &= bits - 1)
        {
            const int group = FirstBit(static_cast<std::uint32_t>(bits));
            answer += group_distance[group][root];
        }
        return answer;
    };

    auto CheapLower = [&](int root, int remaining, double& star_extension)
    {
        star_extension = 0.0;
        if (!remaining)
            return 0.0;
        double farthest = 0.0;
        double first = fp::kInf;
        double second = fp::kInf;
        for (int bits = remaining; bits; bits &= bits - 1)
        {
            const int group = FirstBit(static_cast<std::uint32_t>(bits));
            const double distance = group_distance[group][root];
            star_extension += distance;
            farthest = std::max(farthest, distance);
            if (distance < first)
            {
                second = first;
                first = distance;
            }
            else if (distance < second)
            {
                second = distance;
            }
        }
        if (popcount[remaining] == 1)
            return farthest;
        return std::max(farthest, mst_half[remaining] + (first + second) * 0.5);
    };

    auto Relax = [&](int root, int mask, double cost)
    {
        if (!mask || (mask & ~label_full))
            return;
        const int remaining = full_mask ^ mask;
        double prehash_lower = mst_half[remaining];
        if (remaining)
        {
            const int required = FirstBit(static_cast<std::uint32_t>(remaining));
            prehash_lower = std::max(prehash_lower, group_distance[required][root]);
        }
        if (cost + prehash_lower + fp::kEps >= best)
            return;

        LabelMap& labels = labels_at_root[root];
        Label* label = labels.Find(mask);
        if (label && (label->settled || cost >= label->cost))
            return;

        double lower = label ? label->lower : 0.0;
        if (!label)
        {
            double star_extension = 0.0;
            const double cheap = CheapLower(root, remaining, star_extension);
            best = std::min(best, cost + star_extension);
            if (cost + cheap + fp::kEps >= best)
                return;
            lower = std::max(cheap, tour.At(root, remaining, group_distance));
            lower = std::max(lower, dual.At(root, remaining));
        }
        if (cost + lower + fp::kEps >= best)
            return;

        if (!label)
        {
            label = labels.Insert(mask);
            label->lower = lower;
            ++open_labels;
            ++stats.created_labels;
            stats.peak_open_labels = std::max(stats.peak_open_labels, open_labels);
        }
        label->cost = cost;
        heap.push({cost + lower, root, mask});
    };

    for (int group = 1; group < group_count; ++group)
        for (int root : query.groups[group])
            Relax(root, 1 << group, 0.0);

    while (!heap.empty())
    {
        const HeapNode node = heap.top();
        heap.pop();
        if (node.key + fp::kEps >= best)
            break;

        Label* label = labels_at_root[node.root].Find(node.mask);
        if (!label || label->settled || node.key != label->cost + label->lower)
            continue;

        const double cost = label->cost;
        label->settled = true;
        --open_labels;
        ++stats.settled_labels;

        best = std::min(best, cost + StarExtension(node.root, full_mask ^ node.mask));
        if (node.mask == label_full && (color[node.root] & anchor_bit))
            best = std::min(best, cost);

        if (!disjoint_at_root[node.root])
            disjoint_at_root[node.root] = std::make_unique<DisjointIndex>();
        DisjointIndex& index = *disjoint_at_root[node.root];
        index.Insert(node.mask, cost, group_count);

        for (const AdjEdge& edge : graph.adj[node.root])
            Relax(edge.to, node.mask, cost + edge.w);

        auto Merge = [&](int other_mask, double other_cost)
        {
            Relax(node.root, node.mask | other_mask, cost + other_cost);
        };

        const int available = label_full ^ node.mask;
        const long long submask_work = (1LL << popcount[available]) - 1;
        const std::size_t word_count = (index.masks.size() + 63) / 64;
        const long long bitmap_work =
            static_cast<long long>(word_count) * popcount[node.mask];
        // Both branches enumerate every settled disjoint mask; choose the smaller work bound.
        if (bitmap_work <= submask_work)
        {
            for (std::size_t word = 0; word < word_count; ++word)
            {
                std::uint64_t blocked = 0;
                for (int bits = node.mask; bits; bits &= bits - 1)
                {
                    const int bit = FirstBit(static_cast<std::uint32_t>(bits));
                    blocked |= index.contains[word * static_cast<std::size_t>(group_count) + bit];
                }
                std::uint64_t candidates = ~blocked;
                if (word + 1 == word_count && index.masks.size() % 64)
                    candidates &= (std::uint64_t{1} << (index.masks.size() % 64)) - 1;
                while (candidates)
                {
                    const int offset = FirstBit(candidates);
                    const std::size_t id = word * 64 + static_cast<std::size_t>(offset);
                    Merge(index.masks[id], index.costs[id]);
                    candidates &= candidates - 1;
                }
            }
        }
        else
        {
            for (int other = available; other; other = (other - 1) & available)
            {
                Label* candidate = labels_at_root[node.root].Find(other);
                if (candidate && candidate->settled)
                    Merge(other, candidate->cost);
            }
        }
    }

    stats.search_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - phase_begin).count();
    stats.total_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - total_begin).count();
    result.feasible = best < fp::kInf / 4;
    result.best_weight = result.feasible ? best : -1.0;
    return result;
}

}  // namespace gst::methods::release_v2
