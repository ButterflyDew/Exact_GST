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
#include "tools/dual_cut_probe/dual_cut_potential.h"

namespace
{
using gst::Graph;
using gst::Query;
using gst::UndirectedEdge;
using gst::methods::test19::MetricTspLowerBound;
using gst::tools::dual_cut::DualCutPotential;

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
    long long last_label_completion_budget = 0;
    long long best_updates = 0;
    long long peak_open = 0;
    long long stop_open = 0;
    std::vector<long long> settled_by_size;
    std::vector<long long> open_by_size;
    std::vector<long long> peak_open_by_size;
    std::vector<long long> created_by_size;
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

GlobalHalfResult SolveGlobalLabels(const Graph& graph,
                                   const Query& query,
                                   bool anchored,
                                   int anchor_group,
                                   bool use_dual)
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

    const auto group_dist = ComputeGroupDistances(graph, query);
    const auto group_metric = ComputeGroupMetric(query, group_dist);
    MetricTspLowerBound future_bound;
    future_bound.Build(group_metric, full);

    std::vector<double> group_mst_half(1 << g, 0.0);
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

    GlobalHalfResult result;
    result.settled_by_size.assign(max_label_size + 1, 0);
    result.open_by_size.assign(max_label_size + 1, 0);
    result.peak_open_by_size.assign(max_label_size + 1, 0);
    result.created_by_size.assign(max_label_size + 1, 0);
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

    std::vector<int> greedy_roots;
    result.best = std::min(result.best, GrowGreedy(root_star_root, &greedy_roots));
    std::sort(greedy_roots.begin(), greedy_roots.end());
    greedy_roots.erase(std::unique(greedy_roots.begin(), greedy_roots.end()), greedy_roots.end());
    for (int root : greedy_roots)
        result.best = std::min(result.best, GrowGreedy(root, nullptr));
    result.initial_upper = result.best;

    DualCutPotential dual;
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

    std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> heap;
    std::vector<FlatLabelMap> labels_at_root(n + 1);
    std::vector<std::unordered_map<int, double>> two_block_at_root;
    if (!anchored)
        two_block_at_root.resize(n + 1);
    std::vector<std::unique_ptr<RootDisjointIndex>> disjoint_index(n + 1);
    long long open_count = 0;

    auto SettledCost = [&](int root, int mask)
    {
        if (!mask)
            return 0.0;
        const LabelRecord* record = labels_at_root[root].Find(mask);
        return !record || !record->settled ? gst::fp::kInf : record->cost;
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
        double prehash_lower = group_mst_half[remaining];
        if (remaining)
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
        else
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

        if (anchored && node.mask == label_full && (color[node.root] & anchor_bit) &&
            cost + 1e-9 < result.best)
        {
            result.best = cost;
            ++result.best_updates;
        }

        if (!anchored)
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
            if (anchored || popcount[united] <= half)
            {
                ++result.half_merge_attempts;
                Relax(node.root, united, cost + other_cost);
                return;
            }
            const double pair_cost = cost + other_cost;
            const int third_mask = full ^ united;
            if (pair_cost + group_mst_half[third_mask] + 1e-9 >= result.best)
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
    }
    return result;
}

int Usage()
{
    std::cerr << "Usage: gst_global_half_probe [seed=1] [iterations=100] [min_n=4]"
              << " [max_n=12] [min_g=2] [max_g=8]\n"
              << "       gst_global_half_probe --anchored|--dual-anchored"
              << " [same random arguments]\n"
              << "   or: gst_global_half_probe --dataset|--anchored-dataset|"
              << "--dual-anchored-dataset"
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
    const bool dual_anchored_dataset = mode == "--dual-anchored-dataset";
    const bool anchored_dataset = mode == "--anchored-dataset" || dual_anchored_dataset;
    if (argc >= 2 && (mode == "--dataset" || anchored_dataset))
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
            SolveGlobalLabels(graph, query, anchored_dataset, anchor_group, dual_anchored_dataset);
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
                  << " dual_enabled=" << dual_anchored_dataset
                  << " dual_objective=" << result.dual_objective
                  << " dual_primal_upper=" << result.dual_primal_upper
                  << " dual_ms=" << result.dual_ms
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
                  << " last_label_completion_budget=" << result.last_label_completion_budget
                  << " peak_open=" << result.peak_open
                  << " stop_open=" << result.stop_open
                  << " best_updates=" << result.best_updates
                  << " wall_sec=" << elapsed;
        for (int size = 1; size <= max_label_size; ++size)
            std::cout << " settled_k" << size << '=' << result.settled_by_size[size]
                      << " open_k" << size << '=' << result.open_by_size[size]
                      << " peak_open_k" << size << '=' << result.peak_open_by_size[size]
                      << " created_k" << size << '=' << result.created_by_size[size];
        std::cout << '\n';
        return 0;
    }
    const bool dual_anchored = mode == "--dual-anchored";
    const bool anchored = mode == "--anchored" || dual_anchored;
    const int first_arg = anchored ? 2 : 1;
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
        const GlobalHalfResult result = SolveGlobalLabels(graph, query, anchored, 0, dual_anchored);
        if (!exact.feasible || std::abs(result.best - exact.best_weight) > 1e-6)
        {
            ++mismatches;
            std::cout << "MISMATCH iteration=" << iteration
                      << " n=" << n
                      << " g=" << g
                      << " exact=" << exact.best_weight
                      << " global_labels=" << result.best
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
              << " dual=" << (dual_anchored ? 1 : 0)
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
