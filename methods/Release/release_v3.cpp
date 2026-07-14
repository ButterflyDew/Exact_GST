#include "release_v3.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "../../float_compare.h"
#include "../../query_feasibility.h"
#include "../Common/dual_cut_potential.h"

namespace gst::methods::release_v3
{
namespace
{
using Clock = std::chrono::steady_clock;

// A completed rooted subset row. Singleton rows live in group_distance instead.
struct Row
{
    std::vector<int> vertices;
    std::vector<double> distances;
    int count = 0;
    bool ready = false;
    bool dense = false;
};

struct QueueNode
{
    double key = 0.0;
    double distance = 0.0;
    int vertex = 0;
};

struct QueueGreater
{
    bool operator()(const QueueNode& a, const QueueNode& b) const
    {
        if (a.key != b.key)
            return a.key > b.key;
        if (a.distance != b.distance)
            return a.distance > b.distance;
        return a.vertex > b.vertex;
    }
};

int FirstBit(int mask);

// Global search stores only masks that exclude one permanent anchor group.
struct GlobalHeapNode
{
    double key = 0.0;
    int root = 0;
    int mask = 0;

    bool operator>(const GlobalHeapNode& other) const
    {
        if (key != other.key)
            return key > other.key;
        if (mask != other.mask)
            return mask > other.mask;
        return root > other.root;
    }
};

struct GlobalLabel
{
    double cost = fp::kInf;
    double lower = 0.0;
    int mask = 0;
    bool settled = false;
};

class GlobalLabelMap
{
public:
    GlobalLabel* Find(int mask)
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

    GlobalLabel* Insert(int mask)
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
    // Global masks are nonzero because one anchor group is omitted.
    // This leaves mask 0 available as the open-addressing empty sentinel.
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
        std::vector<GlobalLabel> old = std::move(table_);
        table_.assign(capacity, GlobalLabel{});
        for (const GlobalLabel& label : old)
        {
            if (!label.mask)
                continue;
            std::size_t slot = Hash(label.mask) & (table_.size() - 1);
            while (table_[slot].mask)
                slot = (slot + 1) & (table_.size() - 1);
            table_[slot] = label;
        }
    }

    std::vector<GlobalLabel> table_;
    std::size_t size_ = 0;
};

struct GlobalDisjointIndex
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
            contains.resize(required);
        masks.push_back(mask);
        costs.push_back(cost);
        const std::uint64_t id_bit = std::uint64_t{1} << (id % 64);
        for (int bits = mask; bits; bits &= bits - 1)
            contains[word * static_cast<std::size_t>(group_count) + FirstBit(bits)] |= id_bit;
    }
};

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

int FirstBit64(std::uint64_t bits)
{
#if defined(_MSC_VER)
    unsigned long index = 0;
    _BitScanForward64(&index, bits);
    return static_cast<int>(index);
#else
    return __builtin_ctzll(bits);
#endif
}

long long BinarySearchCost(size_t size)
{
    long long cost = 1;
    for (size_t p = 2; p < size + 1; p <<= 1)
        ++cost;
    return cost;
}

long long JoinCost(size_t a, size_t b)
{
    const long long linear = static_cast<long long>(a + b);
    const long long search_a = static_cast<long long>(a) * BinarySearchCost(b);
    const long long search_b = static_cast<long long>(b) * BinarySearchCost(a);
    return std::min({linear, search_a, search_b});
}

template <class Use>
void JoinRows(const Row& a, const Row& b, Use&& use)
{
    const auto& av = a.vertices;
    const auto& bv = b.vertices;
    const long long linear = static_cast<long long>(av.size() + bv.size());
    const long long search_a = static_cast<long long>(av.size()) * BinarySearchCost(bv.size());
    const long long search_b = static_cast<long long>(bv.size()) * BinarySearchCost(av.size());

    if (search_a < linear && search_a <= search_b)
    {
        for (size_t i = 0; i < av.size(); ++i)
        {
            auto it = std::lower_bound(bv.begin(), bv.end(), av[i]);
            if (it == bv.end() || *it != av[i])
                continue;
            const size_t j = static_cast<size_t>(it - bv.begin());
            use(av[i], a.distances[i], b.distances[j]);
        }
        return;
    }
    if (search_b < linear)
    {
        for (size_t j = 0; j < bv.size(); ++j)
        {
            auto it = std::lower_bound(av.begin(), av.end(), bv[j]);
            if (it == av.end() || *it != bv[j])
                continue;
            const size_t i = static_cast<size_t>(it - av.begin());
            use(bv[j], a.distances[i], b.distances[j]);
        }
        return;
    }

    size_t i = 0;
    size_t j = 0;
    while (i < av.size() && j < bv.size())
    {
        if (av[i] < bv[j])
            ++i;
        else if (bv[j] < av[i])
            ++j;
        else
        {
            use(av[i], a.distances[i], b.distances[j]);
            ++i;
            ++j;
        }
    }
}

std::vector<std::vector<double>> GroupDistances(const Graph& graph, const Query& query)
{
    using Item = std::pair<double, int>;
    using Heap = std::priority_queue<Item, std::vector<Item>, std::greater<Item>>;

    std::vector<std::vector<double>> result(
        query.groups.size(), std::vector<double>(graph.n + 1, fp::kInf));
    for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
    {
        Heap heap;
        for (int v : query.groups[group])
        {
            if (result[group][v] == 0.0)
                continue;
            result[group][v] = 0.0;
            heap.push({0.0, v});
        }
        while (!heap.empty())
        {
            const auto [distance, u] = heap.top();
            heap.pop();
            if (distance != result[group][u])
                continue;
            for (const auto& edge : graph.adj[u])
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

class TourLowerBound
{
public:
    void Build(const std::vector<std::vector<double>>& group_metric)
    {
        group_count_ = static_cast<int>(group_metric.size());
        const int subset_count = 1 << group_count_;
        const int full_mask = subset_count - 1;
        paths_.assign(static_cast<size_t>(subset_count) * group_count_ * group_count_, fp::kInf);

        for (int start = 0; start < group_count_; ++start)
        {
            paths_[Index(1 << start, start, start)] = 0.0;
            for (int mask = 1; mask < subset_count; ++mask)
            {
                if (!((mask >> start) & 1))
                    continue;
                for (int last = 0; last < group_count_; ++last)
                {
                    const double current = paths_[Index(mask, start, last)];
                    if (current >= fp::kInf)
                        continue;
                    for (int remaining = full_mask ^ mask; remaining; remaining &= remaining - 1)
                    {
                        const int bit = remaining & -remaining;
                        const int next = FirstBit(bit);
                        double& destination = paths_[Index(mask | bit, start, next)];
                        destination = std::min(destination, current + group_metric[last][next]);
                    }
                }
            }
        }

        endpoints_.resize(subset_count);
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (!(mask & (mask - 1)))
                continue;
            for (int left_bits = mask; left_bits; left_bits &= left_bits - 1)
            {
                const int left = FirstBit(left_bits & -left_bits);
                for (int right_bits = mask & ~((1 << (left + 1)) - 1);
                     right_bits;
                     right_bits &= right_bits - 1)
                {
                    const int right = FirstBit(right_bits & -right_bits);
                    const double path = std::min(paths_[Index(mask, left, right)],
                                                 paths_[Index(mask, right, left)]);
                    if (path < fp::kInf)
                        endpoints_[mask].push_back({left, right, path});
                }
            }
        }
        paths_.clear();
        paths_.shrink_to_fit();
    }

    double At(int vertex, int mask, const std::vector<std::vector<double>>& group_distance) const
    {
        if (!mask)
            return 0.0;
        if (!(mask & (mask - 1)))
            return group_distance[FirstBit(mask)][vertex];

        double tour = fp::kInf;
        for (const Endpoint& endpoint : endpoints_[mask])
        {
            tour = std::min(tour,
                            group_distance[endpoint.left][vertex] + endpoint.path +
                                group_distance[endpoint.right][vertex]);
        }
        return tour * 0.5;
    }

private:
    struct Endpoint
    {
        int left = 0;
        int right = 0;
        double path = 0.0;
    };

    size_t Index(int mask, int start, int last) const
    {
        return (static_cast<size_t>(mask) * group_count_ + start) * group_count_ + last;
    }

    int group_count_ = 0;
    std::vector<double> paths_;
    std::vector<std::vector<Endpoint>> endpoints_;
};

std::vector<double> GroupMstHalf(const std::vector<std::vector<double>>& metric,
                                 const std::vector<int>& popcount)
{
    const int g = static_cast<int>(metric.size());
    const int subset_count = 1 << g;
    std::vector<double> result(subset_count);
    for (int mask = 1; mask < subset_count; ++mask)
    {
        if (popcount[mask] <= 1)
            continue;
        std::vector<double> distance(g, fp::kInf);
        std::vector<char> used(g);
        distance[FirstBit(mask)] = 0.0;
        double sum = 0.0;
        for (int iteration = 0; iteration < popcount[mask]; ++iteration)
        {
            int next = -1;
            for (int group = 0; group < g; ++group)
                if ((mask & (1 << group)) && !used[group] &&
                    (next < 0 || distance[group] < distance[next]))
                    next = group;
            used[next] = 1;
            sum += distance[next];
            for (int group = 0; group < g; ++group)
                if ((mask & (1 << group)) && !used[group])
                    distance[group] = std::min(distance[group], metric[next][group]);
        }
        result[mask] = sum * 0.5;
    }
    return result;
}

double ContinueAnchoredGlobal(const Graph& graph,
                              const Query& query,
                              const std::vector<std::vector<double>>& group_distance,
                              const std::vector<std::vector<double>>& metric,
                              const std::vector<int>& popcount,
                              const std::vector<int>& color,
                              const TourLowerBound& tour,
                              const gst::methods::dual_cut::DualCutPotential& dual,
                              int anchor_root,
                              double best,
                              ReleaseStats& stats)
{
    // Turn the group farthest from the root-star center into the permanent goal.
    // It leaves the explicit mask space while remaining in every future bound.
    const auto begin = Clock::now();
    const int g = static_cast<int>(query.groups.size());
    const int full_mask = (1 << g) - 1;
    int anchor_group = 0;
    for (int group = 1; group < g; ++group)
        if (group_distance[group][anchor_root] >
            group_distance[anchor_group][anchor_root])
            anchor_group = group;
    const int anchor_bit = 1 << anchor_group;
    const int label_full = full_mask ^ anchor_bit;
    const std::vector<double> mst_half = GroupMstHalf(metric, popcount);
    stats.global_used = true;
    stats.global_anchor_group = anchor_group + 1;
    stats.global_anchor_distance = group_distance[anchor_group][anchor_root];

    std::priority_queue<GlobalHeapNode,
                        std::vector<GlobalHeapNode>,
                        std::greater<GlobalHeapNode>> heap;
    std::vector<GlobalLabelMap> labels_at_root(graph.n + 1);
    std::vector<std::unique_ptr<GlobalDisjointIndex>> disjoint_at_root(graph.n + 1);
    long long open_labels = 0;

    auto StarExtension = [&](int root, int remaining)
    {
        double answer = 0.0;
        for (int bits = remaining; bits; bits &= bits - 1)
            answer += group_distance[FirstBit(bits)][root];
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
            const double distance = group_distance[FirstBit(bits)][root];
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
            prehash_lower = std::max(
                prehash_lower, group_distance[FirstBit(remaining)][root]);
        if (cost + prehash_lower + fp::kEps >= best)
            return;

        GlobalLabelMap& labels = labels_at_root[root];
        GlobalLabel* label = labels.Find(mask);
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
            ++stats.global_created_labels;
            stats.global_peak_open_labels =
                std::max(stats.global_peak_open_labels, open_labels);
        }
        label->cost = cost;
        heap.push({cost + lower, root, mask});
    };

    for (int group = 0; group < g; ++group)
        if (group != anchor_group)
            for (int root : query.groups[group])
                Relax(root, 1 << group, 0.0);

    while (!heap.empty())
    {
        const GlobalHeapNode node = heap.top();
        heap.pop();
        if (node.key + fp::kEps >= best)
            break;
        GlobalLabel* label = labels_at_root[node.root].Find(node.mask);
        if (!label || label->settled || node.key != label->cost + label->lower)
            continue;

        const double cost = label->cost;
        label->settled = true;
        --open_labels;
        ++stats.global_settled_labels;
        best = std::min(best, cost + StarExtension(node.root, full_mask ^ node.mask));
        if (node.mask == label_full && (color[node.root] & anchor_bit))
            best = std::min(best, cost);

        if (!disjoint_at_root[node.root])
            disjoint_at_root[node.root] = std::make_unique<GlobalDisjointIndex>();
        GlobalDisjointIndex& index = *disjoint_at_root[node.root];
        index.Insert(node.mask, cost, g);

        for (const auto& edge : graph.adj[node.root])
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
        if (bitmap_work <= submask_work)
        {
            for (std::size_t word = 0; word < word_count; ++word)
            {
                std::uint64_t blocked = 0;
                for (int bits = node.mask; bits; bits &= bits - 1)
                    blocked |= index.contains[word * static_cast<std::size_t>(g) +
                                              FirstBit(bits)];
                std::uint64_t candidates = ~blocked;
                if (word + 1 == word_count && index.masks.size() % 64)
                    candidates &=
                        (std::uint64_t{1} << (index.masks.size() % 64)) - 1;
                while (candidates)
                {
                    const int offset = FirstBit64(candidates);
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
                GlobalLabel* candidate = labels_at_root[node.root].Find(other);
                if (candidate && candidate->settled)
                    Merge(other, candidate->cost);
            }
        }
    }

    stats.global_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return best;
}

double RootStarUpper(const std::vector<std::vector<double>>& group_distance, int n, int& root)
{
    double best = fp::kInf;
    for (int v = 1; v <= n; ++v)
    {
        double sum = 0.0;
        for (const auto& distance : group_distance)
            sum += distance[v];
        if (sum < best)
        {
            best = sum;
            root = v;
        }
    }
    return best;
}

double GreedyUpper(const Graph& graph,
                   const std::vector<int>& color,
                   int full_mask,
                   int root,
                   double limit,
                   std::vector<int>* covering_vertices = nullptr)
{
    using Item = std::pair<double, int>;
    using Heap = std::priority_queue<Item, std::vector<Item>, std::greater<Item>>;

    const int n = graph.n;
    std::vector<char> in_tree(n + 1);
    std::vector<int> parent(n + 1);
    std::vector<double> distance(n + 1, fp::kInf);
    in_tree[root] = 1;
    int covered = color[root];
    double cost = 0.0;
    if (covering_vertices)
    {
        covering_vertices->clear();
        if (covered)
            covering_vertices->push_back(root);
    }

    while (covered != full_mask && cost < limit)
    {
        Heap heap;
        std::fill(distance.begin(), distance.end(), fp::kInf);
        for (int v = 1; v <= n; ++v)
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
                cost += d;
                break;
            }
            for (const auto& edge : graph.adj[u])
            {
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
        for (int v = found; v && !in_tree[v]; v = parent[v])
        {
            const int newly_covered = color[v] & (full_mask ^ covered);
            in_tree[v] = 1;
            covered |= color[v];
            if (covering_vertices && newly_covered)
                covering_vertices->push_back(v);
        }
    }
    return cost;
}

double GoalRootStar(const Graph& graph, const Query& query)
{
    // For at most three representatives, an optimal tree has a median root v,
    // so OPT = min_v sum_a distance(v, group_a).
    using Item = std::pair<double, int>;
    using Heap = std::priority_queue<Item, std::vector<Item>, std::greater<Item>>;
    const int g = static_cast<int>(query.groups.size());
    if (g == 1)
        return 0.0;
    const int full = (1 << g) - 1;
    std::vector<std::vector<double>> distance(
        g, std::vector<double>(graph.n + 1, fp::kInf));
    std::vector<Heap> frontier(g);
    for (int group = 0; group < g; ++group)
        for (int vertex : query.groups[group])
            if (distance[group][vertex] != 0.0)
            {
                distance[group][vertex] = 0.0;
                frontier[group].push({0.0, vertex});
            }

    std::vector<int> settled_mask(graph.n + 1);
    std::vector<double> settled_sum(graph.n + 1);
    std::vector<int> mask_count(1 << g);
    mask_count[0] = graph.n;
    std::vector<Heap> base_by_mask(1 << g);
    std::vector<int> terminal_mask(graph.n + 1);
    for (int group = 0; group < g; ++group)
        for (int vertex : query.groups[group])
            terminal_mask[vertex] |= 1 << group;

    double best = fp::kInf;
    for (int vertex = 1; vertex <= graph.n; ++vertex)
        if (terminal_mask[vertex] == full)
            best = 0.0;

    auto CleanFrontier = [&](int group)
    {
        while (!frontier[group].empty())
        {
            const auto [value, vertex] = frontier[group].top();
            if (value == distance[group][vertex] &&
                !(settled_mask[vertex] & (1 << group)))
                break;
            frontier[group].pop();
        }
    };
    auto MinBase = [&](int mask)
    {
        if (!mask_count[mask])
            return fp::kInf;
        if (!mask)
            return 0.0;
        Heap& heap = base_by_mask[mask];
        while (!heap.empty())
        {
            const auto [value, vertex] = heap.top();
            if (settled_mask[vertex] == mask && value == settled_sum[vertex])
                return value;
            heap.pop();
        }
        return fp::kInf;
    };

    while (true)
    {
        std::vector<double> minimum(g, fp::kInf);
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
        if (next_group < 0)
            break;

        if (best < fp::kInf / 4)
        {
            double global_lower = fp::kInf;
            for (int mask = 0; mask <= full; ++mask)
            {
                double lower = MinBase(mask);
                if (lower >= fp::kInf / 4)
                    continue;
                for (int group = 0; group < g; ++group)
                    if (!(mask & (1 << group)))
                        lower += minimum[group];
                global_lower = std::min(global_lower, lower);
            }
            if (global_lower + fp::kEps >= best)
                break;
        }

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
        if (new_mask == full)
            best = std::min(best, settled_sum[vertex]);

        for (const auto& edge : graph.adj[vertex])
        {
            const double next = value + edge.w;
            if (next < distance[next_group][edge.to])
            {
                distance[next_group][edge.to] = next;
                frontier[next_group].push({next, edge.to});
            }
        }
    }
    return best;
}
}  // namespace

SolveResult SolveOneQuery(const Graph& graph, const Query& query)
{
    SolveResult result;
    ReleaseStats& stats = result.stats;
    stats.n = graph.n;
    stats.m = graph.m;
    stats.g = static_cast<int>(query.groups.size());

    if (!stats.g)
        return {0.0, true, stats};
    if (stats.g > 20)
        throw std::runtime_error("ReleaseV3 supports group count <= 20.");
    if (!IsQueryFeasible(graph, query))
        return result;

    const auto total_start = Clock::now();
    const int n = graph.n;
    const int g = stats.g;
    if (g <= 3)
    {
        const double best = GoalRootStar(graph, query);
        stats.total_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - total_start).count();
        result.best_weight = best;
        result.feasible = best < fp::kInf / 4;
        return result;
    }
    const int subset_count = 1 << g;
    const int full_mask = subset_count - 1;
    const int half = g / 2;
    stats.row_work = 0;

    int heap_cost = 0;
    for (long long span = 1; span < n; span *= 2)
        ++heap_cost;
    stats.dual_cut_build_work =
        2LL * g * (static_cast<long long>(graph.m) +
                   static_cast<long long>(n) * std::max(1, heap_cost));
    const long long greedy_upper_build_work = stats.dual_cut_build_work / 2;

    // Shared metric preprocessing feeds both the half-row and global phases.
    const auto distance_start = Clock::now();
    const auto group_distance = GroupDistances(graph, query);
    stats.group_distance_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - distance_start).count();

    std::vector<int> color(n + 1);
    for (int group = 0; group < g; ++group)
        for (int v : query.groups[group])
            color[v] |= 1 << group;

    const auto upper_start = Clock::now();
    int root = 1;
    double best = RootStarUpper(group_distance, n, root);
    std::vector<int> greedy_roots;
    bool greedy_upper_built = false;
    stats.upper_bound_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - upper_start).count();

    std::vector<std::vector<double>> group_metric(g, std::vector<double>(g, fp::kInf));
    for (int a = 0; a < g; ++a)
        for (int b = 0; b < g; ++b)
            for (int v : query.groups[b])
                group_metric[a][b] = std::min(group_metric[a][b], group_distance[a][v]);

    const auto tsp_start = Clock::now();
    TourLowerBound lower_bound;
    lower_bound.Build(group_metric);
    stats.tsp_ms = std::chrono::duration<double, std::milli>(Clock::now() - tsp_start).count();

    bool dual_ready = false;

    gst::methods::dual_cut::DualCutPotential dual_cut;
    auto BuildDual = [&]()
    {
        const auto dual_cut_start = Clock::now();
        dual_cut.Build(graph, query, group_distance, root);
        stats.dual_cut_ms +=
            std::chrono::duration<double, std::milli>(Clock::now() - dual_cut_start).count();
        best = std::min(best, dual_cut.PrimalUpper());
        dual_ready = true;
    };
    auto FutureBound = [&](int vertex, int mask)
    {
        const double tsp = lower_bound.At(vertex, mask, group_distance);
        return dual_ready ? std::max(tsp, dual_cut.At(vertex, mask)) : tsp;
    };

    // Cardinality order guarantees that every proper subrow is already complete.
    std::vector<int> popcount(subset_count);
    std::vector<int> order;
    for (int mask = 1; mask < subset_count; ++mask)
    {
        popcount[mask] = popcount[mask >> 1] + (mask & 1);
        if (popcount[mask] <= half)
            order.push_back(mask);
    }
    std::sort(order.begin(), order.end(), [&](int a, int b)
    {
        return popcount[a] != popcount[b] ? popcount[a] < popcount[b] : a < b;
    });
    std::vector<int> order_index(subset_count, -1);
    for (int i = 0; i < static_cast<int>(order.size()); ++i)
        order_index[order[i]] = i;

    std::vector<char> needed_later(subset_count, 1);
    // A half-size row is kept only when a later mask can still use it in a
    // recurrence or a two/three-block completion. Earlier masks are never revisited.
    for (int mask : order)
    {
        if (popcount[mask] < half)
            continue;
        needed_later[mask] = 0;
        const int remaining = full_mask ^ mask;
        if (popcount[remaining] == half)
        {
            needed_later[mask] = order_index[remaining] > order_index[mask];
            continue;
        }
        for (int bits = remaining; bits; bits &= bits - 1)
        {
            const int future = remaining ^ (bits & -bits);
            if (order_index[future] > order_index[mask])
            {
                needed_later[mask] = 1;
                break;
            }
        }
    }

    const auto dp_start = Clock::now();
    std::vector<Row> rows(subset_count);
    std::vector<double> distance(n + 1, fp::kInf);
    std::vector<double> heuristic(n + 1);
    std::vector<int> heuristic_stamp(n + 1);
    std::vector<int> touched;
    std::vector<int> settled;
    std::vector<std::pair<int, int>> complement_pairs;
    std::vector<double> complement_row(n + 1, fp::kInf);
    int stamp = 0;
    int current_size = 0;
    int masks_in_size = 0;
    long long queue_pushes = 0;
    long long queue_pops = 0;
    double compacted_at_best = best;

    auto Available = [&](int mask)
    {
        return mask && popcount[mask] <= half &&
               (popcount[mask] == 1 || rows[mask].ready);
    };
    auto Lookup = [&](int mask, int vertex) -> double
    {
        if (!mask)
            return 0.0;
        if (popcount[mask] == 1)
            return group_distance[FirstBit(mask)][vertex];
        if (!Available(mask))
            return fp::kInf;
        const Row& row = rows[mask];
        if (row.dense)
            return row.distances[vertex];
        auto it = std::lower_bound(row.vertices.begin(), row.vertices.end(), vertex);
        if (it == row.vertices.end() || *it != vertex)
            return fp::kInf;
        return row.distances[static_cast<size_t>(it - row.vertices.begin())];
    };
    // Centralize singleton/dense/sparse access so row seeds and completions agree.
    auto ForEachValue = [&](int mask, auto&& use)
    {
        if (popcount[mask] == 1)
        {
            const auto& values = group_distance[FirstBit(mask)];
            for (int vertex = 1; vertex <= n; ++vertex)
                use(vertex, values[vertex]);
            return;
        }
        const Row& row = rows[mask];
        if (row.dense)
        {
            for (int vertex = 1; vertex <= n; ++vertex)
                if (row.distances[vertex] < fp::kInf)
                    use(vertex, row.distances[vertex]);
            return;
        }
        for (size_t i = 0; i < row.vertices.size(); ++i)
            use(row.vertices[i], row.distances[i]);
    };
    auto ForEachPairSum = [&](int left, int right, auto&& use)
    {
        if (!left || !right)
        {
            ForEachValue(left | right, use);
            return;
        }
        if (popcount[left] == 1 && popcount[right] == 1)
        {
            const auto& a = group_distance[FirstBit(left)];
            const auto& b = group_distance[FirstBit(right)];
            for (int vertex = 1; vertex <= n; ++vertex)
                use(vertex, a[vertex] + b[vertex]);
            return;
        }
        if (popcount[left] == 1 || popcount[right] == 1)
        {
            const int singleton = popcount[left] == 1 ? left : right;
            const int other = singleton == left ? right : left;
            const auto& singleton_distance = group_distance[FirstBit(singleton)];
            ForEachValue(other, [&](int vertex, double value)
            {
                use(vertex, value + singleton_distance[vertex]);
            });
            return;
        }

        const Row& a = rows[left];
        const Row& b = rows[right];
        if (a.dense && b.dense)
        {
            for (int vertex = 1; vertex <= n; ++vertex)
                if (a.distances[vertex] < fp::kInf && b.distances[vertex] < fp::kInf)
                    use(vertex, a.distances[vertex] + b.distances[vertex]);
            return;
        }
        if (a.dense || b.dense)
        {
            const Row& dense = a.dense ? a : b;
            const Row& sparse = a.dense ? b : a;
            for (size_t i = 0; i < sparse.vertices.size(); ++i)
            {
                const int vertex = sparse.vertices[i];
                if (dense.distances[vertex] < fp::kInf)
                    use(vertex, sparse.distances[i] + dense.distances[vertex]);
            }
            return;
        }
        JoinRows(a, b, [&](int vertex, double x, double y)
        {
            use(vertex, x + y);
        });
    };

    // Once pair rows exist, this supplies a cheap feasible partition upper bound.
    auto PairPartitionUpper = [&]()
    {
        std::vector<int> roots{root};
        int smallest_group = 0;
        for (int group = 1; group < g; ++group)
        {
            if (query.groups[group].size() < query.groups[smallest_group].size())
                smallest_group = group;
        }
        roots.insert(
            roots.end(), query.groups[smallest_group].begin(), query.groups[smallest_group].end());
        std::sort(roots.begin(), roots.end());
        roots.erase(std::unique(roots.begin(), roots.end()), roots.end());

        std::vector<double> block_cost(subset_count, fp::kInf);
        std::vector<double> partition(subset_count, fp::kInf);
        for (int candidate_root : roots)
        {
            std::fill(block_cost.begin(), block_cost.end(), fp::kInf);
            block_cost[0] = 0.0;
            for (int group = 0; group < g; ++group)
                block_cost[1 << group] = group_distance[group][candidate_root];
            for (int a = 0; a < g; ++a)
            {
                for (int b = a + 1; b < g; ++b)
                {
                    const int pair = (1 << a) | (1 << b);
                    block_cost[pair] = Lookup(pair, candidate_root);
                }
            }

            std::fill(partition.begin(), partition.end(), fp::kInf);
            partition[0] = 0.0;
            for (int mask = 1; mask < subset_count; ++mask)
            {
                const int a = FirstBit(mask);
                const int a_bit = 1 << a;
                const int rest = mask ^ a_bit;
                double value = partition[rest] + block_cost[a_bit];
                for (int bits = rest; bits; bits &= bits - 1)
                {
                    const int b = FirstBit(bits);
                    const int pair = a_bit | (1 << b);
                    if (block_cost[pair] < fp::kInf)
                    {
                        value = std::min(
                            value, partition[mask ^ pair] + block_cost[pair]);
                    }
                }
                partition[mask] = value;
            }
            if (partition[full_mask] < best)
            {
                best = partition[full_mask];
            }
        }
    };

    auto LookupCost = [&](int mask) -> long long
    {
        if (!mask || popcount[mask] == 1)
            return 1;
        if (rows[mask].dense)
            return 1;
        return BinarySearchCost(rows[mask].vertices.size());
    };
    auto PairBuildCost = [&](int left, int right) -> long long
    {
        if (!left)
            return popcount[right] == 1 || rows[right].dense
                       ? n
                       : static_cast<long long>(rows[right].vertices.size());
        if (!right)
            return popcount[left] == 1 || rows[left].dense
                       ? n
                       : static_cast<long long>(rows[left].vertices.size());
        if (popcount[left] == 1 && popcount[right] == 1)
            return n;
        if (popcount[left] == 1)
            return rows[right].dense ? n : static_cast<long long>(rows[right].vertices.size());
        if (popcount[right] == 1)
            return rows[left].dense ? n : static_cast<long long>(rows[left].vertices.size());
        if (rows[left].dense && rows[right].dense)
            return n;
        if (rows[left].dense)
            return static_cast<long long>(rows[right].vertices.size());
        if (rows[right].dense)
            return static_cast<long long>(rows[left].vertices.size());
        return JoinCost(rows[left].vertices.size(), rows[right].vertices.size());
    };

    auto CompactRows = [&]()
    {
        for (int mask = 1; mask < subset_count; ++mask)
        {
            Row& row = rows[mask];
            if (!row.ready || popcount[mask] == 1)
                continue;
            if (row.dense)
            {
                int count = 0;
                for (int v = 1; v <= n; ++v)
                {
                    if (row.distances[v] >= fp::kInf)
                        continue;
                    if (row.distances[v] + FutureBound(v, full_mask ^ mask) > best)
                    {
                        row.distances[v] = fp::kInf;
                    }
                    else
                        ++count;
                }
                row.count = count;
            }
            else
            {
                int write = 0;
                for (int i = 0; i < row.count; ++i)
                {
                    if (row.distances[i] +
                            FutureBound(row.vertices[i], full_mask ^ mask) >
                        best)
                        continue;
                    if (write != i)
                    {
                        row.vertices[write] = row.vertices[i];
                        row.distances[write] = row.distances[i];
                    }
                    ++write;
                }
                row.vertices.resize(write);
                row.distances.resize(write);
                row.count = write;
            }
        }
        compacted_at_best = best;
    };

    // The global recurrence is self-contained, so all row-only storage dies here.
    auto FinishWithGlobal = [&](int size, int masks_in_current_size)
    {
        stats.global_switch_size = size;
        stats.global_switch_masks_in_size = masks_in_current_size;
        stats.dp_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - dp_start).count();
        std::vector<double>().swap(distance);
        std::vector<double>().swap(heuristic);
        std::vector<int>().swap(heuristic_stamp);
        std::vector<int>().swap(touched);
        std::vector<int>().swap(settled);
        std::vector<std::pair<int, int>>().swap(complement_pairs);
        std::vector<double>().swap(complement_row);
        std::vector<Row>().swap(rows);
        std::sort(greedy_roots.begin(), greedy_roots.end());
        greedy_roots.erase(
            std::unique(greedy_roots.begin(), greedy_roots.end()), greedy_roots.end());
        for (int greedy_root : greedy_roots)
            best = std::min(best, GreedyUpper(graph, color, full_mask, greedy_root, best));
        best = ContinueAnchoredGlobal(graph,
                                      query,
                                      group_distance,
                                      group_metric,
                                      popcount,
                                      color,
                                      lower_bound,
                                      dual_cut,
                                      root,
                                      best,
                                      stats);
        stats.total_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - total_start).count();
        result.best_weight = best;
        result.feasible = true;
    };

    if (!dual_ready && stats.row_work >= stats.dual_cut_build_work)
    {
        BuildDual();
        FinishWithGlobal(0, 0);
        return result;
    }

    // Build exact rows through same-root joins followed by A*-ordered closure.
    for (int mask : order)
    {
        const int size = popcount[mask];
        if (size == 1)
            continue;
        if (size != current_size)
        {
            if (current_size == 2)
                PairPartitionUpper();
            if (current_size && best < compacted_at_best)
                CompactRows();
            current_size = size;
            masks_in_size = 0;
        }
        ++masks_in_size;
        const int remaining_mask = full_mask ^ mask;
        long long mask_work = 0;
        const long long pushes_before = queue_pushes;
        const long long pops_before = queue_pops;
        ++stamp;
        touched.clear();
        settled.clear();

        auto H = [&](int vertex)
        {
            if (heuristic_stamp[vertex] != stamp)
            {
                heuristic_stamp[vertex] = stamp;
                heuristic[vertex] = FutureBound(vertex, remaining_mask);
            }
            return heuristic[vertex];
        };
        auto Set = [&](int vertex, double value)
        {
            if (value >= distance[vertex] || value + H(vertex) > best)
                return;
            if (distance[vertex] == fp::kInf)
                touched.push_back(vertex);
            distance[vertex] = value;
        };

        if (size == 2)
        {
            mask_work += n;
            const int a = FirstBit(mask);
            const int b = FirstBit(mask ^ (1 << a));
            for (int v = 1; v <= n; ++v)
                Set(v, group_distance[a][v] + group_distance[b][v]);
        }
        else
        {
            for (int left = (mask - 1) & mask; left; left = (left - 1) & mask)
            {
                const int right = mask ^ left;
                if (!right || left > right || !Available(left) || !Available(right))
                    continue;
                mask_work += PairBuildCost(left, right);
                ForEachPairSum(left, right, Set);
            }
        }

        complement_pairs.clear();
        if (size * 3 >= g)
        {
            for (int left = remaining_mask;; left = (left - 1) & remaining_mask)
            {
                const int right = remaining_mask ^ left;
                if (left <= right && popcount[left] <= half && popcount[right] <= half &&
                    (!left || Available(left)) && (!right || Available(right)))
                    complement_pairs.push_back({left, right});
                if (!left)
                    break;
            }
        }

        long long rent_per_query = 0;
        long long build_cost = 0;
        for (const auto [left, right] : complement_pairs)
        {
            rent_per_query += LookupCost(left) + LookupCost(right);
            build_cost += PairBuildCost(left, right);
        }
        long long paid_rent = 0;
        bool cache_ready = false;

        auto BuildComplementRow = [&]()
        {
            std::fill(complement_row.begin(), complement_row.end(), fp::kInf);
            for (const auto [left, right] : complement_pairs)
                ForEachPairSum(left, right, [&](int vertex, double value)
                {
                    complement_row[vertex] = std::min(complement_row[vertex], value);
                });
            cache_ready = true;
        };
        auto Complete = [&](int vertex, double value)
        {
            if (complement_pairs.empty())
                return;
            double other = fp::kInf;
            if (cache_ready)
            {
                other = complement_row[vertex];
            }
            else if (build_cost > 0 && rent_per_query > 0 && paid_rent + rent_per_query >= build_cost)
            {
                BuildComplementRow();
                other = complement_row[vertex];
            }
            else
            {
                paid_rent += rent_per_query;
                for (const auto [left, right] : complement_pairs)
                {
                    const double a = Lookup(left, vertex);
                    const double b = Lookup(right, vertex);
                    if (a < fp::kInf && b < fp::kInf)
                        other = std::min(other, a + b);
                }
            }
            if (other < fp::kInf)
                best = std::min(best, value + other);
        };

        std::priority_queue<QueueNode, std::vector<QueueNode>, QueueGreater> queue;
        for (int v : touched)
        {
            queue.push({distance[v] + H(v), distance[v], v});
            ++queue_pushes;
        }
        while (!queue.empty())
        {
            const QueueNode current = queue.top();
            queue.pop();
            ++queue_pops;
            if (current.distance != distance[current.vertex] || current.key > best)
                continue;

            Complete(current.vertex, current.distance);
            if (current.distance + H(current.vertex) > best)
                continue;
            settled.push_back(current.vertex);

            for (const auto& edge : graph.adj[current.vertex])
            {
                ++mask_work;
                const double next = current.distance + edge.w;
                if (next >= distance[edge.to] || next + H(edge.to) > best)
                    continue;
                if (distance[edge.to] == fp::kInf)
                    touched.push_back(edge.to);
                distance[edge.to] = next;
                queue.push({next + H(edge.to), next, edge.to});
                ++queue_pushes;
            }
        }

        std::sort(settled.begin(), settled.end());
        settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
        Row& row = rows[mask];
        int keep_count = 0;
        for (int v : settled)
        {
            const double need = distance[v] + H(v);
            if (need <= best)
                settled[keep_count++] = v;
        }
        settled.resize(keep_count);
        if (needed_later[mask])
        {
            const size_t sparse_bytes = settled.size() * (sizeof(int) + sizeof(double));
            const size_t dense_bytes = static_cast<size_t>(n + 1) * sizeof(double);
            row.dense = dense_bytes < sparse_bytes;
            row.count = keep_count;
            if (row.dense)
            {
                row.distances.assign(n + 1, fp::kInf);
                for (int v : settled)
                {
                    row.distances[v] = distance[v];
                }
            }
            else
            {
                row.vertices = settled;
                row.distances.reserve(settled.size());
                for (int v : settled)
                {
                    row.distances.push_back(distance[v]);
                }
            }
            row.ready = true;
            stats.distance_bytes += row.dense ? dense_bytes : sparse_bytes;
        }

        for (int v : touched)
            distance[v] = fp::kInf;
        mask_work += paid_rent;
        if (cache_ready)
            mask_work += build_cost;
        mask_work += (queue_pushes - pushes_before + queue_pops - pops_before) *
                     std::max(1, heap_cost);
        stats.row_work += mask_work;
        if (!greedy_upper_built && stats.row_work >= greedy_upper_build_work)
        {
            const auto greedy_start = Clock::now();
            const double old_best = best;
            best = std::min(best,
                            GreedyUpper(
                                graph, color, full_mask, root, best, &greedy_roots));
            stats.upper_bound_ms +=
                std::chrono::duration<double, std::milli>(Clock::now() - greedy_start).count();
            greedy_upper_built = true;
            if (best < old_best)
                CompactRows();
        }
        // Rent rows until their measured work pays for one dual/global build.
        // The accounting uses operations from the complexity bound, not wall time.
        if (!dual_ready && stats.row_work >= stats.dual_cut_build_work)
        {
            BuildDual();
            CompactRows();
            FinishWithGlobal(size, masks_in_size);
            return result;
        }
    }

    stats.dp_ms = std::chrono::duration<double, std::milli>(Clock::now() - dp_start).count();
    stats.total_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - total_start).count();
    result.best_weight = best;
    result.feasible = true;
    return result;
}

}  // namespace gst::methods::release_v3
