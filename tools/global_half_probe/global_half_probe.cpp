#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "float_compare.h"
#include "graph_io.h"
#include "methods/DPBF/dpbf_solver.h"
#include "methods/Test/test19_future_bounds.h"
#include "query_io.h"
#include "methods/Common/dual_cut_potential.h"

namespace
{
using gst::Graph;
using gst::Query;
using gst::UndirectedEdge;
using gst::methods::test19::MetricTspLowerBound;
using gst::methods::dual_cut::DualCutPotential;

struct Options
{
    std::uint64_t seed = 1;
    int iterations = 100;
    int min_n = 4;
    int max_n = 12;
    int min_g = 2;
    int max_g = 8;
};

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

struct LabelRecord
{
    double cost = gst::fp::kInf;
    double lower = 0.0;
    int key = 0;
    bool settled = false;
};

class FlatLabelMap
{
public:
    LabelRecord* Find(int key)
    {
        if (table_.empty())
            return nullptr;
        std::size_t slot = Hash(key) & (table_.size() - 1);
        while (table_[slot].key)
        {
            if (table_[slot].key == key)
                return &table_[slot];
            slot = (slot + 1) & (table_.size() - 1);
        }
        return nullptr;
    }

    std::pair<LabelRecord*, bool> TryEmplace(int key)
    {
        if (table_.empty())
            Rehash(8);
        else if ((size_ + 1) * 2 > table_.size())
            Rehash(table_.size() * 2);
        std::size_t slot = Hash(key) & (table_.size() - 1);
        while (table_[slot].key)
        {
            if (table_[slot].key == key)
                return {&table_[slot], false};
            slot = (slot + 1) & (table_.size() - 1);
        }
        table_[slot].key = key;
        ++size_;
        return {&table_[slot], true};
    }

    void AppendSettled(int root,
                       std::vector<std::vector<std::pair<int, double>>>& rows) const
    {
        for (const auto& entry : table_)
            if (entry.key && entry.settled)
                rows[entry.key].push_back({root, entry.cost});
    }

private:
    static std::uint32_t Hash(int key)
    {
        std::uint32_t value = static_cast<std::uint32_t>(key);
        value ^= value >> 16;
        value *= 0x7feb352dU;
        value ^= value >> 15;
        value *= 0x846ca68bU;
        return value ^ (value >> 16);
    }

    void Rehash(std::size_t capacity)
    {
        std::vector<LabelRecord> old = std::move(table_);
        table_.assign(capacity, LabelRecord{});
        for (auto& entry : old)
        {
            if (!entry.key)
                continue;
            std::size_t slot = Hash(entry.key) & (table_.size() - 1);
            while (table_[slot].key)
                slot = (slot + 1) & (table_.size() - 1);
            table_[slot] = std::move(entry);
        }
    }

    std::vector<LabelRecord> table_;
    std::size_t size_ = 0;
};

struct GlobalHalfResult
{
    double best = gst::fp::kInf;
    double root_star_upper = gst::fp::kInf;
    double initial_upper = gst::fp::kInf;
    double dual_objective = 0.0;
    double dual_primal_upper = gst::fp::kInf;
    double dual_ms = 0.0;
    double bounds_ms = 0.0;
    double upper_ms = 0.0;
    double search_ms = 0.0;
    double metric_full_lower = 0.0;
    long long pushes = 0;
    long long pops = 0;
    long long stale_pops = 0;
    long long bound_pruned = 0;
    long long prehash_bound_pruned = 0;
    long long cheap_bound_pruned = 0;
    long long future_queries = 0;
    long long open_future_reuses = 0;
    long long settled = 0;
    long long edge_relaxations = 0;
    long long legacy_merge_checks = 0;
    long long submask_probes = 0;
    long long submask_join_queries = 0;
    long long bitmap_join_queries = 0;
    long long bitmap_word_bit_ops = 0;
    long long disjoint_pairs = 0;
    long long half_merge_attempts = 0;
    long long oversized_pair_lb_pruned = 0;
    long long two_block_improvements = 0;
    long long completion_checks = 0;
    long long star_completion_checks = 0;
    long long star_completion_updates = 0;
    long long generated_star_checks = 0;
    long long generated_star_updates = 0;
    long long offline_completion_checks = 0;
    long long offline_completion_candidates = 0;
    double offline_completion_ms = 0.0;
    double threshold_lower = 0.0;
    long long threshold_stops = 0;
    long long last_label_completion_budget = 0;
    long long best_updates = 0;
    long long first_best_settled = 0;
    long long first_best_edge_relaxations = 0;
    long long peak_open = 0;
    long long stop_open = 0;
    std::vector<long long> settled_by_size;
    std::vector<long long> open_by_size;
    std::vector<long long> peak_open_by_size;
    std::vector<long long> created_by_size;
};

class ThresholdEnvelope
{
public:
    void Build(const std::vector<double>& completion_lower, int missing_count)
    {
        lower_ = completion_lower;
        missing_count_ = missing_count;
        base_.assign(lower_.size(), gst::fp::kInf);
        next_ = 0;
        active_base_ = gst::fp::kInf;
        inactive_ = {};
        order_.resize(lower_.size());
        std::iota(order_.begin(), order_.end(), 0);
        std::sort(order_.begin(), order_.end(), [&](int a, int b)
        {
            return Breakpoint(a) != Breakpoint(b) ? Breakpoint(a) < Breakpoint(b) : a < b;
        });
    }

    void Update(int mask, double base, double lambda)
    {
        if (mask < 0 || mask >= static_cast<int>(base_.size()) || base >= base_[mask])
            return;
        base_[mask] = base;
        if (Breakpoint(mask) <= lambda)
            active_base_ = std::min(active_base_, base);
        else
            inactive_.push({base + lower_[mask], mask, base});
    }

    double Query(double lambda)
    {
        while (next_ < order_.size() && Breakpoint(order_[next_]) <= lambda)
        {
            const int mask = order_[next_++];
            active_base_ = std::min(active_base_, base_[mask]);
        }
        while (!inactive_.empty())
        {
            const Entry& entry = inactive_.top();
            if (entry.base != base_[entry.mask] || Breakpoint(entry.mask) <= lambda)
                inactive_.pop();
            else
                break;
        }
        double answer = active_base_ + missing_count_ * lambda;
        if (!inactive_.empty())
            answer = std::min(answer, inactive_.top().key);
        return answer;
    }

private:
    struct Entry
    {
        double key = 0.0;
        int mask = 0;
        double base = 0.0;

        bool operator>(const Entry& other) const
        {
            if (key != other.key)
                return key > other.key;
            return mask > other.mask;
        }
    };

    double Breakpoint(int mask) const
    {
        return lower_[mask] / missing_count_;
    }

    int missing_count_ = 1;
    std::vector<double> lower_;
    std::vector<double> base_;
    std::vector<int> order_;
    std::size_t next_ = 0;
    double active_base_ = gst::fp::kInf;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> inactive_;
};

int TrailingZeros(std::uint64_t bits)
{
#if defined(_MSC_VER)
    unsigned long index = 0;
    _BitScanForward64(&index, bits);
    return static_cast<int>(index);
#else
    return __builtin_ctzll(bits);
#endif
}

struct RootDisjointIndex
{
    std::vector<int> masks;
    std::vector<double> costs;
    // Word-major layout: contains[word * g + bit].
    std::vector<std::uint64_t> contains;

    void Insert(int mask, double cost, int g)
    {
        const std::size_t id = masks.size();
        const std::size_t word = id / 64;
        if (contains.size() < (word + 1) * static_cast<std::size_t>(g))
            contains.resize((word + 1) * static_cast<std::size_t>(g), 0);
        masks.push_back(mask);
        costs.push_back(cost);
        const std::uint64_t id_bit = std::uint64_t{1} << (id % 64);
        for (int bits = mask; bits; bits &= bits - 1)
        {
            const int bit = TrailingZeros(static_cast<std::uint64_t>(bits));
            contains[word * static_cast<std::size_t>(g) + bit] |= id_bit;
        }
    }
};

int CountBits(int mask)
{
    int count = 0;
    while (mask)
    {
        mask &= mask - 1;
        ++count;
    }
    return count;
}

void AddEdge(Graph& graph, int u, int v, double w)
{
    UndirectedEdge edge;
    edge.id = static_cast<int>(graph.edges.size());
    edge.u = u;
    edge.v = v;
    edge.w = w;
    graph.edges.push_back(edge);
    graph.adj[u].push_back({v, edge.id, w});
    graph.adj[v].push_back({u, edge.id, w});
    graph.m = static_cast<int>(graph.edges.size());
}

Graph RandomGraph(std::mt19937_64& rng, int n)
{
    Graph graph;
    graph.n = n;
    graph.adj.assign(n + 1, {});
    std::uniform_int_distribution<int> weight(1, 30);
    for (int v = 2; v <= n; ++v)
    {
        std::uniform_int_distribution<int> parent(1, v - 1);
        AddEdge(graph, v, parent(rng), weight(rng));
    }
    std::bernoulli_distribution extra(0.30);
    for (int u = 1; u <= n; ++u)
        for (int v = u + 1; v <= n; ++v)
            if (extra(rng))
                AddEdge(graph, u, v, weight(rng));
    return graph;
}

Query RandomQuery(std::mt19937_64& rng, int n, int g)
{
    Query query;
    query.groups.resize(g);
    std::uniform_int_distribution<int> vertex(1, n);
    std::uniform_int_distribution<int> group_size(1, std::min(3, n));
    for (auto& group : query.groups)
    {
        const int target = group_size(rng);
        while (static_cast<int>(group.size()) < target)
        {
            const int v = vertex(rng);
            if (std::find(group.begin(), group.end(), v) == group.end())
                group.push_back(v);
        }
        std::sort(group.begin(), group.end());
    }
    return query;
}

std::vector<std::vector<double>> ComputeGroupDistances(const Graph& graph, const Query& query)
{
    using Item = std::pair<double, int>;
    const int g = static_cast<int>(query.groups.size());
    std::vector<std::vector<double>> gd(g, std::vector<double>(graph.n + 1, gst::fp::kInf));
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap;
    for (int a = 0; a < g; ++a)
    {
        while (!heap.empty())
            heap.pop();
        for (int v : query.groups[a])
        {
            gd[a][v] = 0.0;
            heap.push({0.0, v});
        }
        while (!heap.empty())
        {
            auto [d, u] = heap.top();
            heap.pop();
            if (d != gd[a][u])
                continue;
            for (const auto edge : graph.adj[u])
            {
                const double nd = d + edge.w;
                if (nd < gd[a][edge.to])
                {
                    gd[a][edge.to] = nd;
                    heap.push({nd, edge.to});
                }
            }
        }
    }
    return gd;
}

std::vector<std::vector<double>> ComputeGroupMetric(
    const Query& query,
    const std::vector<std::vector<double>>& group_dist)
{
    const int g = static_cast<int>(query.groups.size());
    std::vector<std::vector<double>> metric(g, std::vector<double>(g, gst::fp::kInf));
    for (int a = 0; a < g; ++a)
        for (int b = 0; b < g; ++b)
            for (int v : query.groups[b])
                metric[a][b] = std::min(metric[a][b], group_dist[a][v]);
    return metric;
}

std::vector<std::vector<double>> ComputeGroupMetricEarly(const Graph& graph,
                                                         const Query& query)
{
    using Item = std::pair<double, int>;
    const int g = static_cast<int>(query.groups.size());
    std::vector<int> color(graph.n + 1);
    for (int group = 0; group < g; ++group)
        for (int vertex : query.groups[group])
            color[vertex] |= 1 << group;

    std::vector<std::vector<double>> metric(g, std::vector<double>(g, gst::fp::kInf));
    std::vector<double> distance(graph.n + 1, gst::fp::kInf);
    std::vector<int> touched;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap;
    for (int source = 0; source < g; ++source)
    {
        for (int vertex : touched)
            distance[vertex] = gst::fp::kInf;
        touched.clear();
        while (!heap.empty())
            heap.pop();
        for (int vertex : query.groups[source])
        {
            if (distance[vertex] == 0.0)
                continue;
            distance[vertex] = 0.0;
            touched.push_back(vertex);
            heap.push({0.0, vertex});
        }

        int reached = 0;
        while (!heap.empty() && reached != (1 << g) - 1)
        {
            const auto [value, vertex] = heap.top();
            heap.pop();
            if (value != distance[vertex])
                continue;
            for (int bits = color[vertex] & ~reached; bits; bits &= bits - 1)
            {
                const int target = TrailingZeros(static_cast<std::uint64_t>(bits));
                metric[source][target] = value;
            }
            reached |= color[vertex];
            for (const auto edge : graph.adj[vertex])
            {
                const double next = value + edge.w;
                if (next >= distance[edge.to])
                    continue;
                if (distance[edge.to] == gst::fp::kInf)
                    touched.push_back(edge.to);
                distance[edge.to] = next;
                heap.push({next, edge.to});
            }
        }
    }
    return metric;
}

std::vector<double> ComputeGroupTourHalf(const std::vector<std::vector<double>>& metric)
{
    const int g = static_cast<int>(metric.size());
    const int subset_count = 1 << g;
    std::vector<double> result(subset_count);
    std::vector<double> path(static_cast<std::size_t>(subset_count) * g, gst::fp::kInf);
    for (int start = 0; start < g; ++start)
    {
        std::fill(path.begin(), path.end(), gst::fp::kInf);
        path[static_cast<std::size_t>(1 << start) * g + start] = 0.0;
        for (int mask = 1 << start; mask < subset_count; ++mask)
        {
            if (!(mask & (1 << start)))
                continue;
            for (int last = 0; last < g; ++last)
            {
                const double current = path[static_cast<std::size_t>(mask) * g + last];
                if (current >= gst::fp::kInf / 4)
                    continue;
                for (int remaining = (subset_count - 1) ^ mask; remaining;
                     remaining &= remaining - 1)
                {
                    const int bit = remaining & -remaining;
                    const int next = TrailingZeros(static_cast<std::uint64_t>(bit));
                    double& destination =
                        path[static_cast<std::size_t>(mask | bit) * g + next];
                    destination = std::min(destination, current + metric[last][next]);
                }
            }
            if (TrailingZeros(static_cast<std::uint64_t>(mask)) != start ||
                !(mask & (mask - 1)))
                continue;
            double cycle = gst::fp::kInf;
            for (int last = 0; last < g; ++last)
                if (last != start && (mask & (1 << last)))
                    cycle = std::min(
                        cycle, path[static_cast<std::size_t>(mask) * g + last] +
                                   metric[last][start]);
            result[mask] = cycle * 0.5;
        }
    }
    return result;
}

GlobalHalfResult SolveGlobalLabels(const Graph& graph,
                                   const Query& query,
                                   bool anchored,
                                   int anchor_group,
                                   bool use_dual,
                                   bool offline_completion = false,
                                   bool use_future_bounds = true,
                                   bool use_metric_bounds = false)
{
    const auto solve_begin = std::chrono::steady_clock::now();
    const bool progress = std::getenv("GST_GLOBAL_LABEL_PROGRESS") != nullptr;
    const int n = graph.n;
    const int g = static_cast<int>(query.groups.size());
    const int full = (1 << g) - 1;
    const int half = g / 2;
    const int anchor_bit = 1 << anchor_group;
    const int label_full = anchored ? full ^ anchor_bit : full;
    const int max_label_size = anchored ? g - 1 : half;
    std::vector<int> popcount(1 << g, 0);
    for (int mask = 1; mask <= full; ++mask)
        popcount[mask] = popcount[mask >> 1] + (mask & 1);

    std::vector<std::vector<long long>> choose(g + 1, std::vector<long long>(g + 1, 0));
    for (int i = 0; i <= g; ++i)
    {
        choose[i][0] = choose[i][i] = 1;
        for (int j = 1; j < i; ++j)
            choose[i][j] = choose[i - 1][j - 1] + choose[i - 1][j];
    }
    std::vector<long long> completion_budget_by_size(max_label_size + 1, 0);
    for (int size = 1; !anchored && size <= half; ++size)
    {
        const int remaining_size = g - size;
        long long ordered = 0;
        for (int left_size = 1; left_size <= half; ++left_size)
        {
            const int right_size = remaining_size - left_size;
            if (right_size >= 1 && right_size <= half)
                ordered += choose[remaining_size][left_size];
        }
        completion_budget_by_size[size] = ordered / 2;
    }

    GlobalHalfResult result;
    const auto bounds_begin = std::chrono::steady_clock::now();
    std::vector<std::vector<double>> group_dist;
    std::vector<std::vector<double>> group_metric;
    MetricTspLowerBound future_bound;
    std::vector<double> group_mst_half(1 << g, 0.0);
    std::vector<double> group_tour_half(1 << g, 0.0);
    std::vector<double> split_two_lower(1 << g, gst::fp::kInf);
    double split_three_lower = gst::fp::kInf;
    auto BuildSplitLowers = [&]()
    {
        std::fill(split_two_lower.begin(), split_two_lower.end(), gst::fp::kInf);
        split_three_lower = gst::fp::kInf;
        for (int mask = 1; mask <= full; ++mask)
        {
            for (int left = (mask - 1) & mask; left; left = (left - 1) & mask)
            {
                const int right = mask ^ left;
                if (!right || left > right || popcount[left] > half ||
                    popcount[right] > half)
                    continue;
                split_two_lower[mask] = std::min(
                    split_two_lower[mask], group_tour_half[left] + group_tour_half[right]);
            }
        }
        for (int first = (full - 1) & full; first; first = (first - 1) & full)
        {
            const int rest = full ^ first;
            if (popcount[first] > half || split_two_lower[rest] >= gst::fp::kInf / 4)
                continue;
            split_three_lower = std::min(
                split_three_lower, group_tour_half[first] + split_two_lower[rest]);
        }
    };
    if (use_future_bounds)
    {
        group_dist = ComputeGroupDistances(graph, query);
        group_metric = ComputeGroupMetric(query, group_dist);
        future_bound.Build(group_metric, full);
    }
    else if (use_metric_bounds)
    {
        group_metric = ComputeGroupMetricEarly(graph, query);
        group_tour_half = ComputeGroupTourHalf(group_metric);
    }
    if (use_future_bounds || use_metric_bounds)
    {
        for (int mask = 1; mask <= full; ++mask)
        {
            if (popcount[mask] <= 1)
                continue;
            std::vector<double> distance(g, gst::fp::kInf);
            std::vector<char> used(g, 0);
            const int first = TrailingZeros(static_cast<std::uint64_t>(mask));
            distance[first] = 0.0;
            double sum = 0.0;
            for (int iteration = 0; iteration < popcount[mask]; ++iteration)
            {
                int next = -1;
                for (int bit = 0; bit < g; ++bit)
                    if ((mask >> bit & 1) && !used[bit] &&
                        (next < 0 || distance[bit] < distance[next]))
                        next = bit;
                used[next] = 1;
                sum += distance[next];
                for (int bit = 0; bit < g; ++bit)
                    if ((mask >> bit & 1) && !used[bit])
                        distance[bit] = std::min(distance[bit], group_metric[next][bit]);
            }
            group_mst_half[mask] = sum * 0.5;
        }
    }
    if (!use_future_bounds)
        BuildSplitLowers();
    if (use_metric_bounds)
    {
        result.metric_full_lower = group_tour_half[full];
    }
    result.bounds_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - bounds_begin).count();

    result.settled_by_size.assign(max_label_size + 1, 0);
    result.open_by_size.assign(max_label_size + 1, 0);
    result.peak_open_by_size.assign(max_label_size + 1, 0);
    result.created_by_size.assign(max_label_size + 1, 0);
    std::vector<int> color(n + 1, 0);
    for (int a = 0; a < g; ++a)
        for (int root : query.groups[a])
            color[root] |= 1 << a;

    auto GrowGreedy = [&](int start, std::vector<int>* chosen)
    {
        using Item = std::pair<double, int>;
        std::vector<char> in_tree(n + 1, 0);
        std::vector<double> distance(n + 1, gst::fp::kInf);
        std::vector<int> parent(n + 1, 0);
        std::vector<int> sources{start};
        in_tree[start] = 1;
        int covered = color[start];
        double answer = 0.0;
        if (chosen)
        {
            chosen->clear();
            if (color[start])
                chosen->push_back(start);
        }
        while (covered != full)
        {
            std::fill(distance.begin(), distance.end(), gst::fp::kInf);
            std::fill(parent.begin(), parent.end(), 0);
            std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
            for (int root : sources)
            {
                distance[root] = 0.0;
                queue.push({0.0, root});
            }
            int hit = 0;
            while (!queue.empty())
            {
                const auto [d, u] = queue.top();
                queue.pop();
                if (d != distance[u])
                    continue;
                if (color[u] & (full ^ covered))
                {
                    hit = u;
                    answer += d;
                    break;
                }
                for (const auto edge : graph.adj[u])
                {
                    const double nd = d + edge.w;
                    if (nd < distance[edge.to])
                    {
                        distance[edge.to] = nd;
                        parent[edge.to] = u;
                        queue.push({nd, edge.to});
                    }
                }
            }
            if (!hit)
                return gst::fp::kInf;
            for (int v = hit; !in_tree[v]; v = parent[v])
            {
                const int newly_covered = color[v] & (full ^ covered);
                in_tree[v] = 1;
                sources.push_back(v);
                covered |= color[v];
                if (chosen && newly_covered)
                    chosen->push_back(v);
            }
        }
        return answer;
    };

    const auto upper_begin = std::chrono::steady_clock::now();
    DualCutPotential dual;
    if (use_future_bounds)
    {
        int root_star_root = 1;
        for (int root = 1; root <= n; ++root)
        {
            double candidate = 0.0;
            for (int a = 0; a < g; ++a)
                candidate += group_dist[a][root];
            if (candidate < result.best)
            {
                result.best = candidate;
                root_star_root = root;
            }
        }
        result.root_star_upper = result.best;

        std::vector<int> greedy_roots;
        result.best = std::min(result.best, GrowGreedy(root_star_root, &greedy_roots));
        std::sort(greedy_roots.begin(), greedy_roots.end());
        greedy_roots.erase(
            std::unique(greedy_roots.begin(), greedy_roots.end()), greedy_roots.end());
        for (int root : greedy_roots)
            result.best = std::min(result.best, GrowGreedy(root, nullptr));
        result.initial_upper = result.best;

        if (use_dual)
        {
            const auto dual_begin = std::chrono::steady_clock::now();
            dual.Build(graph, query, group_dist, root_star_root);
            result.dual_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - dual_begin).count();
            result.dual_objective = dual.Objective();
            result.dual_primal_upper = dual.PrimalUpper();
            result.best = std::min(result.best, result.dual_primal_upper);
            result.initial_upper = result.best;
        }
    }
    else if (use_metric_bounds)
    {
        int start = 0;
        for (int vertex = 1; vertex <= n; ++vertex)
            if (color[vertex] &&
                (!start || popcount[color[vertex]] > popcount[color[start]] ||
                 (popcount[color[vertex]] == popcount[color[start]] && vertex < start)))
                start = vertex;
        std::vector<int> greedy_roots;
        result.best = GrowGreedy(start, &greedy_roots);
        std::sort(greedy_roots.begin(), greedy_roots.end());
        greedy_roots.erase(
            std::unique(greedy_roots.begin(), greedy_roots.end()), greedy_roots.end());
        for (int root : greedy_roots)
            result.best = std::min(result.best, GrowGreedy(root, nullptr));
        result.initial_upper = result.best;
        if (group_tour_half[full] + 1e-9 >= result.best)
        {
            result.upper_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - upper_begin).count();
            return result;
        }
    }
    result.upper_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - upper_begin).count();

    const auto search_begin = std::chrono::steady_clock::now();
    const bool search_started_without_best = result.best >= gst::fp::kInf / 4;
    std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> heap;
    std::vector<FlatLabelMap> labels_at_root(n + 1);
    std::vector<std::unordered_map<int, double>> two_block_at_root;
    if (!anchored && !offline_completion)
        two_block_at_root.resize(n + 1);
    std::vector<std::unique_ptr<RootDisjointIndex>> disjoint_index(n + 1);
    long long open_count = 0;

    auto CanSplitIntoTwo = [&](int mask)
    {
        const int size = popcount[mask];
        return size >= 2 && size <= 2 * half;
    };
    const bool threshold_completion = !use_future_bounds && !anchored;
    ThresholdEnvelope threshold_one;
    ThresholdEnvelope threshold_two;
    if (threshold_completion)
    {
        threshold_one.Build(group_tour_half, 1);
        threshold_two.Build(split_two_lower, 2);
        if (split_two_lower[full] < gst::fp::kInf / 4)
            threshold_two.Update(full, 0.0, 0.0);
    }

    auto SettledCost = [&](int root, int mask)
    {
        if (!mask)
            return 0.0;
        const LabelRecord* record = labels_at_root[root].Find(mask);
        return !record || !record->settled ? gst::fp::kInf : record->cost;
    };

    auto CompleteOffline = [&]()
    {
        const auto begin = std::chrono::steady_clock::now();
        using State = std::pair<int, double>;
        std::vector<std::vector<State>> rows(1 << g);
        for (int root = 1; root <= n; ++root)
            labels_at_root[root].AppendSettled(root, rows);

        auto CompleteTwo = [&](const std::vector<State>& left,
                               const std::vector<State>& right)
        {
            std::size_t i = 0;
            std::size_t j = 0;
            while (i < left.size() && j < right.size())
            {
                ++result.offline_completion_checks;
                if (left[i].first < right[j].first)
                    ++i;
                else if (right[j].first < left[i].first)
                    ++j;
                else
                {
                    ++result.offline_completion_candidates;
                    result.best = std::min(result.best, left[i].second + right[j].second);
                    ++i;
                    ++j;
                }
            }
        };

        auto CompleteThree = [&](const std::vector<State>& first,
                                 const std::vector<State>& second,
                                 const std::vector<State>& third)
        {
            std::size_t i = 0;
            std::size_t j = 0;
            std::size_t k = 0;
            while (i < first.size() && j < second.size() && k < third.size())
            {
                ++result.offline_completion_checks;
                const int root = std::max({first[i].first, second[j].first, third[k].first});
                if (first[i].first < root)
                    ++i;
                else if (second[j].first < root)
                    ++j;
                else if (third[k].first < root)
                    ++k;
                else
                {
                    ++result.offline_completion_candidates;
                    result.best = std::min(
                        result.best, first[i].second + second[j].second + third[k].second);
                    ++i;
                    ++j;
                    ++k;
                }
            }
        };

        for (int first = 1; first < full; ++first)
        {
            if (popcount[first] > half || rows[first].empty())
                continue;
            const int rest = full ^ first;
            if (first < rest && popcount[rest] <= half && !rows[rest].empty())
                CompleteTwo(rows[first], rows[rest]);

            for (int second = rest; second; second = (second - 1) & rest)
            {
                const int third = rest ^ second;
                if (!(first < second && second < third) || popcount[second] > half ||
                    popcount[third] > half || rows[second].empty() || rows[third].empty())
                    continue;
                CompleteThree(rows[first], rows[second], rows[third]);
            }
        }
        result.offline_completion_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
    };

    auto CheapLowerBound = [&](int root, int remaining, double& star_sum)
    {
        star_sum = 0.0;
        if (!remaining)
            return 0.0;
        double far = 0.0;
        double first = gst::fp::kInf;
        double second = gst::fp::kInf;
        for (int bits = remaining; bits; bits &= bits - 1)
        {
            const int bit = TrailingZeros(static_cast<std::uint64_t>(bits));
            const double distance = group_dist[bit][root];
            star_sum += distance;
            far = std::max(far, distance);
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
        if (popcount[remaining] <= 1)
            return far;
        return std::max(far, group_mst_half[remaining] + (first + second) * 0.5);
    };

    auto Relax = [&](int root, int mask, double cost)
    {
        if (!mask || (mask & ~label_full) || popcount[mask] > max_label_size)
            return;
        const int remaining = full ^ mask;
        if (use_future_bounds || use_metric_bounds)
        {
            double prehash_lower = use_metric_bounds ? group_tour_half[remaining]
                                                     : group_mst_half[remaining];
            if (use_future_bounds && remaining)
            {
                const int required_group = TrailingZeros(static_cast<std::uint64_t>(remaining));
                prehash_lower = std::max(prehash_lower, group_dist[required_group][root]);
            }
            if (cost + prehash_lower + 1e-9 >= result.best)
            {
                ++result.bound_pruned;
                ++result.prehash_bound_pruned;
                return;
            }
        }
        FlatLabelMap& labels = labels_at_root[root];
        LabelRecord* record = labels.Find(mask);
        if (record && (record->settled || cost >= record->cost))
            return;
        double lower = 0.0;
        if (record)
        {
            lower = record->lower;
            ++result.open_future_reuses;
        }
        else if (use_future_bounds)
        {
            double star_sum = 0.0;
            const double cheap_lower = CheapLowerBound(root, remaining, star_sum);
            ++result.generated_star_checks;
            if (cost + star_sum + 1e-9 < result.best)
            {
                result.best = cost + star_sum;
                ++result.generated_star_updates;
                ++result.best_updates;
            }
            if (cost + cheap_lower + 1e-9 >= result.best)
            {
                ++result.bound_pruned;
                ++result.cheap_bound_pruned;
                return;
            }
            ++result.future_queries;
            lower = std::max(cheap_lower, future_bound.TourHalf(remaining, root, group_dist));
            if (use_dual)
                lower = std::max(lower, dual.At(root, remaining));
        }
        const double key = cost + lower;
        if (key + 1e-9 >= result.best)
        {
            ++result.bound_pruned;
            return;
        }
        if (!record)
        {
            record = labels.TryEmplace(mask).first;
            record->cost = cost;
            record->lower = lower;
            ++open_count;
            const int size = popcount[mask];
            ++result.open_by_size[size];
            ++result.created_by_size[size];
            result.peak_open_by_size[size] =
                std::max(result.peak_open_by_size[size], result.open_by_size[size]);
        }
        else
        {
            record->cost = cost;
        }
        heap.push({key, root, mask});
        ++result.pushes;
        result.peak_open = std::max(result.peak_open, open_count);
    };

    for (int a = 0; a < g; ++a)
    {
        if (anchored && a == anchor_group)
            continue;
        for (int root : query.groups[a])
            Relax(root, 1 << a, 0.0);
    }

    while (!heap.empty())
    {
        const HeapNode node = heap.top();
        heap.pop();
        ++result.pops;
        if (node.key + 1e-9 >= result.best)
        {
            result.stop_open = open_count;
            break;
        }
        LabelRecord* label = labels_at_root[node.root].Find(node.mask);
        if (!label || label->settled || node.key != label->cost + label->lower)
        {
            ++result.stale_pops;
            continue;
        }
        const double cost = label->cost;
        if (threshold_completion)
        {
            const double lambda = node.key;
            result.threshold_lower = std::min(
                {threshold_one.Query(lambda),
                 threshold_two.Query(lambda),
                 std::max(split_three_lower, 3.0 * lambda)});
            if (result.threshold_lower + 1e-9 >= result.best)
            {
                ++result.threshold_stops;
                result.stop_open = open_count;
                break;
            }
        }
        label->settled = true;
        --open_count;
        --result.open_by_size[popcount[node.mask]];
        if (!disjoint_index[node.root])
            disjoint_index[node.root] = std::make_unique<RootDisjointIndex>();
        RootDisjointIndex& root_index = *disjoint_index[node.root];
        root_index.Insert(node.mask, cost, g);
        ++result.settled;
        ++result.settled_by_size[popcount[node.mask]];
        result.last_label_completion_budget += completion_budget_by_size[popcount[node.mask]];
        if (threshold_completion)
        {
            const int remaining = full ^ node.mask;
            if (remaining && popcount[remaining] <= half)
                threshold_one.Update(remaining, cost, node.key);
            if (CanSplitIntoTwo(remaining))
                threshold_two.Update(remaining, cost, node.key);
        }
        if (progress && (result.settled & (result.settled - 1)) == 0)
        {
            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - solve_begin).count();
            std::cerr << "progress anchored=" << (anchored ? 1 : 0)
                      << " settled=" << result.settled
                      << " pops=" << result.pops
                      << " open=" << open_count
                      << " best=" << std::setprecision(10) << result.best
                      << " elapsed=" << std::fixed << std::setprecision(3) << elapsed;
            for (int size = 1; size <= max_label_size; ++size)
                if (result.open_by_size[size])
                    std::cerr << " open_k" << size << '=' << result.open_by_size[size];
            std::cerr << '\n';
        }

        if (use_future_bounds)
        {
            double star_completion = cost;
            for (int bits = full ^ node.mask; bits; bits &= bits - 1)
            {
                const int group = TrailingZeros(static_cast<std::uint64_t>(bits));
                star_completion += group_dist[group][node.root];
            }
            ++result.star_completion_checks;
            if (star_completion + 1e-9 < result.best)
            {
                result.best = star_completion;
                ++result.star_completion_updates;
                ++result.best_updates;
            }
        }

        if (anchored && node.mask == label_full && (color[node.root] & anchor_bit) &&
            cost + 1e-9 < result.best)
        {
            result.best = cost;
            ++result.best_updates;
        }

        if (!anchored && !offline_completion)
        {
            const auto two_it = two_block_at_root[node.root].find(full ^ node.mask);
            if (two_it != two_block_at_root[node.root].end())
            {
                ++result.completion_checks;
                const double candidate = cost + two_it->second;
                if (candidate + 1e-9 < result.best)
                {
                    result.best = candidate;
                    ++result.best_updates;
                }
            }
        }

        for (const auto edge : graph.adj[node.root])
        {
            ++result.edge_relaxations;
            Relax(edge.to, node.mask, cost + edge.w);
        }

        auto ProcessDisjoint = [&](int other_mask, double other_cost)
        {
            ++result.disjoint_pairs;
            const int united = node.mask | other_mask;
            if (threshold_completion)
            {
                const int third = full ^ united;
                if (third && popcount[third] <= half)
                    threshold_one.Update(third, cost + other_cost, node.key);
            }
            if (anchored || popcount[united] <= half)
            {
                ++result.half_merge_attempts;
                Relax(node.root, united, cost + other_cost);
                return;
            }
            if (offline_completion)
                return;
            const double pair_cost = cost + other_cost;
            const int third_mask = full ^ united;
            const double third_lower = use_metric_bounds ? group_tour_half[third_mask]
                                                         : group_mst_half[third_mask];
            if (pair_cost + third_lower + 1e-9 >= result.best)
            {
                ++result.oversized_pair_lb_pruned;
                return;
            }
            auto [pair_it, inserted] = two_block_at_root[node.root].emplace(united, pair_cost);
            if (inserted)
            {
                ++result.two_block_improvements;
            }
            else if (pair_cost < pair_it->second)
            {
                pair_it->second = pair_cost;
                ++result.two_block_improvements;
            }
            else
            {
                return;
            }
            const double third_cost = SettledCost(node.root, third_mask);
            if (third_cost < gst::fp::kInf / 4)
            {
                ++result.completion_checks;
                const double candidate = pair_it->second + third_cost;
                if (candidate + 1e-9 < result.best)
                {
                    result.best = candidate;
                    ++result.best_updates;
                }
            }
        };

        result.legacy_merge_checks += static_cast<long long>(root_index.masks.size());
        const int available = label_full ^ node.mask;
        const long long submask_budget = (1LL << popcount[available]) - 1;
        const std::size_t words = (root_index.masks.size() + 63) / 64;
        const long long bitmap_work = static_cast<long long>(words) * popcount[node.mask];
        if (bitmap_work <= submask_budget)
        {
            ++result.bitmap_join_queries;
            result.bitmap_word_bit_ops += bitmap_work;
            for (std::size_t word = 0; word < words; ++word)
            {
                std::uint64_t blocked = 0;
                for (int bits = node.mask; bits; bits &= bits - 1)
                {
                    const int bit = TrailingZeros(static_cast<std::uint64_t>(bits));
                    blocked |= root_index.contains[word * static_cast<std::size_t>(g) + bit];
                }
                std::uint64_t candidates = ~blocked;
                if (word + 1 == words && root_index.masks.size() % 64)
                    candidates &= (std::uint64_t{1} << (root_index.masks.size() % 64)) - 1;
                while (candidates)
                {
                    const int offset = TrailingZeros(candidates);
                    const std::size_t id = word * 64 + static_cast<std::size_t>(offset);
                    ProcessDisjoint(root_index.masks[id], root_index.costs[id]);
                    candidates &= candidates - 1;
                }
            }
        }
        else
        {
            ++result.submask_join_queries;
            for (int other_mask = available; other_mask; other_mask = (other_mask - 1) & available)
            {
                ++result.submask_probes;
                if (popcount[other_mask] > max_label_size)
                    continue;
                const LabelRecord* other = labels_at_root[node.root].Find(other_mask);
                if (other && other->settled)
                    ProcessDisjoint(other_mask, other->cost);
            }
        }
        if (search_started_without_best && !result.first_best_settled &&
            result.best < gst::fp::kInf / 4)
        {
            result.first_best_settled = result.settled;
            result.first_best_edge_relaxations = result.edge_relaxations;
        }
    }
    if (offline_completion)
        CompleteOffline();
    result.search_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - search_begin).count();
    return result;
}

int Usage()
{
    std::cerr << "Usage: gst_global_half_probe [seed=1] [iterations=100] [min_n=4]"
              << " [max_n=12] [min_g=2] [max_g=8]\n"
              << "       gst_global_half_probe --anchored|--dual-anchored|--dual-half|"
              << "--dual-half-offline"
              << "|--bare-half|--bare-anchored|--metric-half|--metric-anchored"
              << " [same random arguments]\n"
              << "   or: gst_global_half_probe --dataset|--bare-half-dataset|"
              << "--bare-anchored-dataset|"
              << "--metric-half-dataset|--metric-anchored-dataset|"
              << "--dual-half-dataset|"
              << "--dual-half-offline-dataset|"
              << "--anchored-dataset|--dual-anchored-dataset"
              << " <graph_folder> <query_selector>"
              << " [query_index=1] [anchor_group_1_based=1]\n";
    return 2;
}

Options ParseOptions(int argc, char** argv, int first_arg)
{
    Options options;
    if (argc > first_arg)
        options.seed = std::strtoull(argv[first_arg], nullptr, 10);
    if (argc > first_arg + 1)
        options.iterations = std::atoi(argv[first_arg + 1]);
    if (argc > first_arg + 2)
        options.min_n = std::atoi(argv[first_arg + 2]);
    if (argc > first_arg + 3)
        options.max_n = std::atoi(argv[first_arg + 3]);
    if (argc > first_arg + 4)
        options.min_g = std::atoi(argv[first_arg + 4]);
    if (argc > first_arg + 5)
        options.max_g = std::atoi(argv[first_arg + 5]);
    return options;
}
}  // namespace

int main(int argc, char** argv)
{
    const std::string mode = argc >= 2 ? argv[1] : "";
    const bool bare_half_dataset = mode == "--bare-half-dataset";
    const bool bare_anchored_dataset = mode == "--bare-anchored-dataset";
    const bool metric_half_dataset = mode == "--metric-half-dataset";
    const bool metric_anchored_dataset = mode == "--metric-anchored-dataset";
    const bool dual_half_dataset = mode == "--dual-half-dataset";
    const bool dual_half_offline_dataset = mode == "--dual-half-offline-dataset";
    const bool dual_anchored_dataset = mode == "--dual-anchored-dataset";
    const bool anchored_dataset = mode == "--anchored-dataset" || bare_anchored_dataset ||
                                   metric_anchored_dataset || dual_anchored_dataset;
    if (argc >= 2 &&
        (mode == "--dataset" || bare_half_dataset || metric_half_dataset ||
         dual_half_dataset || dual_half_offline_dataset ||
         anchored_dataset))
    {
        if (argc < 4 || argc > (anchored_dataset ? 6 : 5))
            return Usage();
        const std::string graph_folder = argv[2];
        const std::string query_selector = argv[3];
        const int query_index = argc > 4 ? std::atoi(argv[4]) : 1;
        const Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const std::vector<Query> queries = gst::LoadQueriesFromFolder(graph_folder, query_selector);
        if (query_index < 1 || query_index > static_cast<int>(queries.size()))
            return Usage();
        const Query& query = queries[query_index - 1];
        const int anchor_group = anchored_dataset && argc > 5 ? std::atoi(argv[5]) - 1 : 0;
        if (anchor_group < 0 || anchor_group >= static_cast<int>(query.groups.size()))
            return Usage();
        const auto begin = std::chrono::steady_clock::now();
        const GlobalHalfResult result =
            SolveGlobalLabels(graph,
                              query,
                              anchored_dataset,
                              anchor_group,
                              dual_half_dataset || dual_half_offline_dataset ||
                                  dual_anchored_dataset,
                               dual_half_offline_dataset,
                              !(bare_half_dataset || bare_anchored_dataset ||
                                metric_half_dataset || metric_anchored_dataset),
                              metric_half_dataset || metric_anchored_dataset);
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - begin).count();
        const int g = static_cast<int>(query.groups.size());
        const int max_label_size = anchored_dataset ? g - 1 : g / 2;
        long long label_masks = 0;
        if (anchored_dataset)
            label_masks = (1LL << (g - 1)) - 1;
        else
            for (int mask = 1; mask < (1 << g); ++mask)
                if (CountBits(mask) <= max_label_size)
                    ++label_masks;
        const long long label_universe = label_masks * graph.n;
        const int label_groups = anchored_dataset ? g - 1 : g;
        const long long pair_universe = max_label_size >= 2
                                            ? static_cast<long long>(label_groups) * (label_groups - 1) / 2 * graph.n
                                            : 0;
        const long long pair_settled = result.settled_by_size.size() > 2
                                           ? result.settled_by_size[2]
                                           : 0;
        std::cout << std::fixed << std::setprecision(10)
                  << "mode=" << (anchored_dataset ? "anchored_dataset" : "half_dataset")
                  << " graph_folder=" << graph_folder
                  << " query_selector=" << query_selector
                  << " query_index=" << query_index
                  << " n=" << graph.n
                  << " m=" << graph.m
                  << " g=" << g
                  << " anchor_group=" << (anchored_dataset ? anchor_group + 1 : 0)
                  << " anchor_size=" << (anchored_dataset ? query.groups[anchor_group].size() : 0)
                  << " dual_enabled="
                  << (dual_half_dataset || dual_half_offline_dataset || dual_anchored_dataset)
                  << " bounds_enabled=" << !(bare_half_dataset || bare_anchored_dataset)
                  << " metric_only="
                  << (metric_half_dataset || metric_anchored_dataset)
                  << " dual_objective=" << result.dual_objective
                  << " dual_primal_upper=" << result.dual_primal_upper
                  << " dual_ms=" << result.dual_ms
                  << " bounds_ms=" << result.bounds_ms
                  << " upper_ms=" << result.upper_ms
                  << " search_ms=" << result.search_ms
                  << " metric_full_lower=" << result.metric_full_lower
                  << " best=" << result.best
                  << " root_star_upper=" << result.root_star_upper
                  << " initial_upper=" << result.initial_upper
                  << " settled=" << result.settled
                  << " label_universe=" << label_universe
                  << " settled_ratio="
                  << (label_universe ? static_cast<double>(result.settled) / label_universe : 0.0)
                  << " pair_settled=" << pair_settled
                  << " pair_universe=" << pair_universe
                  << " pair_ratio="
                  << (pair_universe ? static_cast<double>(pair_settled) / pair_universe : 0.0)
                  << " pushes=" << result.pushes
                  << " pops=" << result.pops
                  << " bound_pruned=" << result.bound_pruned
                  << " prehash_bound_pruned=" << result.prehash_bound_pruned
                  << " cheap_bound_pruned=" << result.cheap_bound_pruned
                  << " future_queries=" << result.future_queries
                  << " open_future_reuses=" << result.open_future_reuses
                  << " edge_relaxations=" << result.edge_relaxations
                  << " legacy_merge_checks=" << result.legacy_merge_checks
                  << " submask_probes=" << result.submask_probes
                  << " submask_join_queries=" << result.submask_join_queries
                  << " bitmap_join_queries=" << result.bitmap_join_queries
                  << " bitmap_word_bit_ops=" << result.bitmap_word_bit_ops
                  << " disjoint_pairs=" << result.disjoint_pairs
                  << " half_merge_attempts=" << result.half_merge_attempts
                  << " oversized_pair_lb_pruned=" << result.oversized_pair_lb_pruned
                  << " two_block_improvements=" << result.two_block_improvements
                  << " completion_checks=" << result.completion_checks
                  << " star_completion_checks=" << result.star_completion_checks
                  << " star_completion_updates=" << result.star_completion_updates
                  << " generated_star_checks=" << result.generated_star_checks
                  << " generated_star_updates=" << result.generated_star_updates
                  << " offline_completion_checks=" << result.offline_completion_checks
                  << " offline_completion_candidates=" << result.offline_completion_candidates
                  << " offline_completion_ms=" << result.offline_completion_ms
                  << " threshold_lower=" << result.threshold_lower
                  << " threshold_stops=" << result.threshold_stops
                  << " last_label_completion_budget=" << result.last_label_completion_budget
                  << " peak_open=" << result.peak_open
                  << " stop_open=" << result.stop_open
                  << " best_updates=" << result.best_updates
                  << " first_best_settled=" << result.first_best_settled
                  << " first_best_edge_relax=" << result.first_best_edge_relaxations
                  << " wall_sec=" << elapsed;
        for (int size = 1; size <= max_label_size; ++size)
            std::cout << " settled_k" << size << '=' << result.settled_by_size[size]
                      << " open_k" << size << '=' << result.open_by_size[size]
                      << " peak_open_k" << size << '=' << result.peak_open_by_size[size]
                      << " created_k" << size << '=' << result.created_by_size[size];
        std::cout << '\n';
        return 0;
    }
    const bool bare_half = mode == "--bare-half";
    const bool bare_anchored = mode == "--bare-anchored";
    const bool metric_half = mode == "--metric-half";
    const bool metric_anchored = mode == "--metric-anchored";
    const bool dual_half = mode == "--dual-half";
    const bool dual_half_offline = mode == "--dual-half-offline";
    const bool dual_anchored = mode == "--dual-anchored";
    const bool anchored = mode == "--anchored" || bare_anchored || metric_anchored ||
                          dual_anchored;
    const int first_arg =
        (anchored || bare_half || metric_half || dual_half || dual_half_offline)
            ? 2
            : 1;
    if (argc > first_arg + 6)
        return Usage();
    const Options options = ParseOptions(argc, argv, first_arg);
    if (options.iterations < 0 || options.min_n < 1 || options.min_n > options.max_n ||
        options.min_g < 2 || options.min_g > options.max_g || options.max_g >= 20)
        return Usage();

    std::mt19937_64 rng(options.seed);
    std::uniform_int_distribution<int> n_dist(options.min_n, options.max_n);
    std::uniform_int_distribution<int> g_dist(options.min_g, options.max_g);

    long long total_settled = 0;
    long long total_half_universe = 0;
    long long total_pair_settled = 0;
    long long total_pair_universe = 0;
    long long total_pushes = 0;
    long long total_pops = 0;
    long long total_bound_pruned = 0;
    long long total_best_updates = 0;
    long long max_open = 0;
    int mismatches = 0;
    const auto begin = std::chrono::steady_clock::now();

    for (int iteration = 1; iteration <= options.iterations; ++iteration)
    {
        const int n = n_dist(rng);
        const int g = g_dist(rng);
        Graph graph = RandomGraph(rng, n);
        Query query = RandomQuery(rng, n, g);
        const auto exact = gst::methods::dpbf::SolveOneQuery(graph, query);
        const GlobalHalfResult result = SolveGlobalLabels(graph,
                                                          query,
                                                          anchored,
                                                          0,
                                                          dual_half || dual_half_offline ||
                                                              dual_anchored,
                                                          dual_half_offline,
                                                          !(bare_half || bare_anchored || metric_half ||
                                                            metric_anchored),
                                                          metric_half || metric_anchored);
        if (!exact.feasible || std::abs(result.best - exact.best_weight) > 1e-6)
        {
            ++mismatches;
            std::cout << "MISMATCH iteration=" << iteration
                      << " n=" << n
                      << " g=" << g
                      << " exact=" << exact.best_weight
                      << " global_labels=" << result.best
                      << " initial_upper=" << result.initial_upper
                      << " anchored=" << (anchored ? 1 : 0) << '\n';
            for (const auto& edge : graph.edges)
                std::cout << "edge " << edge.u << ' ' << edge.v << ' ' << edge.w << '\n';
            for (int a = 0; a < g; ++a)
            {
                std::cout << "group " << a;
                for (int v : query.groups[a])
                    std::cout << ' ' << v;
                std::cout << '\n';
            }
            break;
        }

        const int max_label_size = anchored ? g - 1 : g / 2;
        long long mask_count = anchored ? (1LL << (g - 1)) - 1 : 0;
        if (!anchored)
            for (int mask = 1; mask < (1 << g); ++mask)
                if (CountBits(mask) <= max_label_size)
                    ++mask_count;
        total_settled += result.settled;
        total_half_universe += mask_count * n;
        total_pair_settled += result.settled_by_size.size() > 2 ? result.settled_by_size[2] : 0;
        const int label_groups = anchored ? g - 1 : g;
        total_pair_universe += max_label_size >= 2
                                   ? static_cast<long long>(label_groups) * (label_groups - 1) / 2 * n
                                   : 0;
        total_pushes += result.pushes;
        total_pops += result.pops;
        total_bound_pruned += result.bound_pruned;
        total_best_updates += result.best_updates;
        max_open = std::max(max_open, result.peak_open);
        if (iteration % 100 == 0)
            std::cout << "ok " << iteration << '\n';
    }

    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - begin).count();
    std::cout << std::fixed << std::setprecision(6)
              << "seed=" << options.seed
              << " anchored=" << (anchored ? 1 : 0)
              << " dual=" << (dual_half || dual_half_offline || dual_anchored ? 1 : 0)
              << " bounds=" << (!(bare_half || bare_anchored) ? 1 : 0)
              << " metric_only=" << (metric_half || metric_anchored ? 1 : 0)
              << " iterations=" << options.iterations
              << " mismatches=" << mismatches
              << " settled=" << total_settled
              << " label_universe=" << total_half_universe
              << " settled_ratio="
              << (total_half_universe ? static_cast<double>(total_settled) / total_half_universe : 0.0)
              << " pair_settled=" << total_pair_settled
              << " pair_universe=" << total_pair_universe
              << " pair_ratio="
              << (total_pair_universe ? static_cast<double>(total_pair_settled) / total_pair_universe : 0.0)
              << " pushes=" << total_pushes
              << " pops=" << total_pops
              << " bound_pruned=" << total_bound_pruned
              << " best_updates=" << total_best_updates
              << " max_open=" << max_open
              << " wall_sec=" << elapsed << '\n';
    return mismatches ? 1 : 0;
}
