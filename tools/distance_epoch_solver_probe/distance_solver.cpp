#include "distance_solver.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#include "float_compare.h"
#include "query_feasibility.h"
#include "../dual_cut_probe/dual_cut_potential.h"

namespace gst::tools::distance_epoch
{
namespace
{
using Clock = std::chrono::steady_clock;

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

int FirstBit(int mask)
{
    int bit = 0;
    while (!((mask >> bit) & 1))
        ++bit;
    return bit;
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
                   double limit)
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
            in_tree[v] = 1;
            covered |= color[v];
        }
    }
    return cost;
}
}  // namespace

SolveResult SolveOneQuery(const Graph& graph, const Query& query, bool verbose)
{
    SolveResult result;
    ReleaseStats& stats = result.stats;
    stats.n = graph.n;
    stats.m = graph.m;
    stats.g = static_cast<int>(query.groups.size());
    stats.kept_by_size.assign(stats.g + 1, 0);
    stats.active_roots_by_size.assign(stats.g + 1, 0);
    stats.depth_sum_by_size.assign(stats.g + 1, 0);
    stats.max_depth_by_size.assign(stats.g + 1, 0);

    if (!stats.g)
        return {0.0, true, stats};
    if (stats.g > 20)
        throw std::runtime_error("ReleaseV1 supports group count <= 20.");
    if (!IsQueryFeasible(graph, query))
        return result;

    const auto total_start = Clock::now();
    const int n = graph.n;
    const int g = stats.g;
    const int subset_count = 1 << g;
    const int full_mask = subset_count - 1;
    const int half = g / 2;

    const auto distance_start = Clock::now();
    std::vector<std::vector<double>> group_distance = GroupDistances(graph, query);
    stats.group_distance_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - distance_start).count();

    std::vector<int> color(n + 1);
    for (int group = 0; group < g; ++group)
        for (int v : query.groups[group])
            color[v] |= 1 << group;

    const auto upper_start = Clock::now();
    int root = 1;
    double best = RootStarUpper(group_distance, n, root);
    if (g <= 3)
    {
        stats.upper_bound_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - upper_start).count();
        stats.total_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - total_start).count();
        result.best_weight = best;
        result.feasible = true;
        return result;
    }
    best = std::min(best, GreedyUpper(graph, color, full_mask, root, best));
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

    long long masks_in_layer = g;
    long long splits_in_mask = 0;
    for (int size = 1; size <= half; ++size)
    {
        if (size >= 2)
        {
            stats.dense_join_work +=
                static_cast<long long>(n) * masks_in_layer * splits_in_mask;
        }
        if (size < half)
        {
            masks_in_layer = masks_in_layer * (g - size) / (size + 1);
            splits_in_mask = 2 * splits_in_mask + 1;
        }
    }
    int heap_cost = 0;
    for (long long span = 1; span < n; span *= 2)
        ++heap_cost;
    stats.dual_cut_build_work =
        2LL * g * (static_cast<long long>(graph.m) +
                   static_cast<long long>(n) * std::max(1, heap_cost));
    stats.dual_cut_enabled = stats.dense_join_work >= stats.dual_cut_build_work;

    dual_cut::DualCutPotential dual_cut;
    if (stats.dual_cut_enabled)
    {
        const auto dual_cut_start = Clock::now();
        dual_cut.Build(graph, query, group_distance, root);
        stats.dual_cut_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - dual_cut_start).count();
        stats.dual_cut_objective = dual_cut.Objective();
        stats.dual_cut_primal_upper = dual_cut.PrimalUpper();
        best = std::min(best, stats.dual_cut_primal_upper);
    }
    auto FutureBound = [&](int vertex, int mask)
    {
        const double tsp = lower_bound.At(vertex, mask, group_distance);
        return stats.dual_cut_enabled ? std::max(tsp, dual_cut.At(vertex, mask)) : tsp;
    };

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
    std::vector<int> predecessor_depth(n + 1);
    std::vector<int> touched;
    std::vector<int> settled;
    std::vector<std::pair<int, int>> complement_pairs;
    std::vector<double> complement_row(n + 1, fp::kInf);
    int stamp = 0;
    long long live_states = 0;
    int current_size = 0;
    int masks_in_size = 0;
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

    bool pair_partition_done = false;
    auto PairPartitionUpper = [&]()
    {
        if (pair_partition_done)
            return false;
        pair_partition_done = true;

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

        const auto begin = Clock::now();
        const double old_best = best;
        std::vector<double> block_cost(subset_count, fp::kInf);
        std::vector<double> partition(subset_count, fp::kInf);
        for (int candidate_root : roots)
        {
            ++stats.pair_partition_roots;
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
                ++stats.pair_partition_updates;
            }
        }
        stats.pair_partition_upper = best;
        stats.pair_partition_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        if (verbose)
        {
            std::cerr << "[distance_epoch] pair_partition roots="
                      << stats.pair_partition_roots
                      << " old_best=" << old_best
                      << " best=" << best
                      << " elapsed_ms=" << stats.pair_partition_ms << std::endl;
        }
        return best < old_best;
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
        const auto begin = Clock::now();
        for (int mask = 1; mask < subset_count; ++mask)
        {
            Row& row = rows[mask];
            if (!row.ready || popcount[mask] == 1)
                continue;
            const int old_count = row.count;
            if (row.dense)
            {
                int count = 0;
                for (int v = 1; v <= n; ++v)
                {
                    if (row.distances[v] >= fp::kInf)
                        continue;
                    ++stats.compact_recomputed_states;
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
                    ++stats.compact_recomputed_states;
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
            live_states -= old_count - row.count;
        }
        stats.compact_recompute_ms +=
            std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        compacted_at_best = best;
    };

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
            if (verbose)
            {
                const double elapsed =
                    std::chrono::duration<double>(Clock::now() - dp_start).count();
                std::cerr << "[distance_epoch] enter_k=" << size
                          << " elapsed=" << elapsed
                          << " best=" << best
                          << " live_states=" << live_states
                          << " row_bytes=" << stats.distance_bytes << std::endl;
            }
        }
        ++masks_in_size;
        if (verbose && size == 2 && masks_in_size % 10 == 0)
        {
            const double elapsed =
                std::chrono::duration<double>(Clock::now() - dp_start).count();
            std::cerr << "[distance_epoch] k2_masks=" << masks_in_size
                      << " elapsed=" << elapsed
                      << " best=" << best
                      << " live_states=" << live_states
                      << " row_bytes=" << stats.distance_bytes << std::endl;
        }
        const int remaining_mask = full_mask ^ mask;
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
            predecessor_depth[vertex] = 0;
        };

        if (size == 2)
        {
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
                if (popcount[left] == 1 || popcount[right] == 1)
                {
                    const int singleton = popcount[left] == 1 ? left : right;
                    const int other = singleton == left ? right : left;
                    const Row& row = rows[other];
                    const int group = FirstBit(singleton);
                    if (row.dense)
                    {
                        for (int v = 1; v <= n; ++v)
                            if (row.distances[v] < fp::kInf)
                                Set(v, row.distances[v] + group_distance[group][v]);
                    }
                    else
                    {
                        for (size_t i = 0; i < row.vertices.size(); ++i)
                        {
                            Set(row.vertices[i],
                                row.distances[i] + group_distance[group][row.vertices[i]]);
                        }
                    }
                }
                else
                {
                    const Row& a = rows[left];
                    const Row& b = rows[right];
                    if (a.dense && b.dense)
                    {
                        for (int v = 1; v <= n; ++v)
                            if (a.distances[v] < fp::kInf && b.distances[v] < fp::kInf)
                                Set(v, a.distances[v] + b.distances[v]);
                    }
                    else if (a.dense || b.dense)
                    {
                        const Row& dense = a.dense ? a : b;
                        const Row& sparse = a.dense ? b : a;
                        for (size_t i = 0; i < sparse.vertices.size(); ++i)
                        {
                            const int v = sparse.vertices[i];
                            if (dense.distances[v] < fp::kInf)
                                Set(v, sparse.distances[i] + dense.distances[v]);
                        }
                    }
                    else
                    {
                        JoinRows(a, b,
                                 [&](int v, double x, double y) { Set(v, x + y); });
                    }
                }
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

        auto PutComplement = [&](int vertex, double value)
        {
            complement_row[vertex] = std::min(complement_row[vertex], value);
        };
        auto AddComplementPair = [&](int left, int right)
        {
            if (!left || !right)
            {
                const int nonempty = left | right;
                if (popcount[nonempty] == 1)
                {
                    const int group = FirstBit(nonempty);
                    for (int v = 1; v <= n; ++v)
                        PutComplement(v, group_distance[group][v]);
                }
                else
                {
                    const Row& row = rows[nonempty];
                    if (row.dense)
                    {
                        for (int v = 1; v <= n; ++v)
                            if (row.distances[v] < fp::kInf)
                                PutComplement(v, row.distances[v]);
                    }
                    else
                    {
                        for (size_t i = 0; i < row.vertices.size(); ++i)
                            PutComplement(row.vertices[i], row.distances[i]);
                    }
                }
                return;
            }
            if (popcount[left] == 1 && popcount[right] == 1)
            {
                const int a = FirstBit(left);
                const int b = FirstBit(right);
                for (int v = 1; v <= n; ++v)
                    PutComplement(v, group_distance[a][v] + group_distance[b][v]);
                return;
            }
            if (popcount[left] == 1 || popcount[right] == 1)
            {
                const int singleton = popcount[left] == 1 ? left : right;
                const int other = singleton == left ? right : left;
                const int group = FirstBit(singleton);
                const Row& row = rows[other];
                if (row.dense)
                {
                    for (int v = 1; v <= n; ++v)
                        if (row.distances[v] < fp::kInf)
                            PutComplement(v, row.distances[v] + group_distance[group][v]);
                }
                else
                {
                    for (size_t i = 0; i < row.vertices.size(); ++i)
                        PutComplement(row.vertices[i],
                                      row.distances[i] + group_distance[group][row.vertices[i]]);
                }
                return;
            }
            const Row& a = rows[left];
            const Row& b = rows[right];
            if (a.dense && b.dense)
            {
                for (int v = 1; v <= n; ++v)
                    if (a.distances[v] < fp::kInf && b.distances[v] < fp::kInf)
                        PutComplement(v, a.distances[v] + b.distances[v]);
            }
            else if (a.dense || b.dense)
            {
                const Row& dense = a.dense ? a : b;
                const Row& sparse = a.dense ? b : a;
                for (size_t i = 0; i < sparse.vertices.size(); ++i)
                {
                    const int v = sparse.vertices[i];
                    if (dense.distances[v] < fp::kInf)
                        PutComplement(v, sparse.distances[i] + dense.distances[v]);
                }
            }
            else
            {
                JoinRows(a, b,
                         [&](int v, double x, double y) { PutComplement(v, x + y); });
            }
        };
        auto BuildComplementRow = [&]()
        {
            std::fill(complement_row.begin(), complement_row.end(), fp::kInf);
            for (const auto [left, right] : complement_pairs)
                AddComplementPair(left, right);
            cache_ready = true;
            ++stats.complement_cache_builds;
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
            ++stats.queue_pushes;
        }
        while (!queue.empty())
        {
            const QueueNode current = queue.top();
            queue.pop();
            ++stats.queue_pops;
            if (current.distance != distance[current.vertex] || current.key > best)
                continue;

            Complete(current.vertex, current.distance);
            if (current.distance + H(current.vertex) > best)
                continue;
            settled.push_back(current.vertex);

            for (const auto& edge : graph.adj[current.vertex])
            {
                const double next = current.distance + edge.w;
                if (next >= distance[edge.to] || next + H(edge.to) > best)
                    continue;
                if (distance[edge.to] == fp::kInf)
                    touched.push_back(edge.to);
                distance[edge.to] = next;
                predecessor_depth[edge.to] = predecessor_depth[current.vertex] + 1;
                queue.push({next + H(edge.to), next, edge.to});
                ++stats.queue_pushes;
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
        for (int v : settled)
        {
            ++stats.kept_by_size[size];
            stats.active_roots_by_size[size] += predecessor_depth[v] == 0;
            stats.depth_sum_by_size[size] += predecessor_depth[v];
            stats.max_depth_by_size[size] =
                std::max(stats.max_depth_by_size[size], predecessor_depth[v]);
        }

        if (needed_later[mask])
        {
            const size_t baseline_sparse_bytes =
                settled.size() * (sizeof(int) + 2 * sizeof(double));
            const size_t baseline_dense_bytes =
                static_cast<size_t>(n + 1) * 2 * sizeof(double);
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
            live_states += row.count;
            stats.distance_states += row.count;
            stats.distance_bytes += row.dense ? dense_bytes : sparse_bytes;
            stats.baseline_row_bytes +=
                std::min(baseline_dense_bytes, baseline_sparse_bytes);
        }
        stats.saved_states += keep_count;
        stats.peak_live_states = std::max(stats.peak_live_states, live_states);

        for (int v : touched)
            distance[v] = fp::kInf;
    }

    stats.dp_ms = std::chrono::duration<double, std::milli>(Clock::now() - dp_start).count();
    stats.total_ms = std::chrono::duration<double, std::milli>(Clock::now() - total_start).count();
    result.best_weight = best;
    result.feasible = true;
    return result;
}

}  // namespace gst::tools::distance_epoch
