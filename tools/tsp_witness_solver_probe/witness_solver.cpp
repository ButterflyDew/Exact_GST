#include "witness_solver.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#include "float_compare.h"
#include "query_feasibility.h"

namespace gst::tools::tsp_witness
{
namespace
{
using Clock = std::chrono::steady_clock;

struct Row
{
    std::vector<int> vertices;
    std::vector<double> distances;
    std::vector<std::uint8_t> witness;
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

template <class AliveA, class AliveB, class Use>
void JoinRows(const Row& a, const Row& b, AliveA&& alive_a, AliveB&& alive_b, Use&& use)
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
            if (!alive_a(i))
                continue;
            auto it = std::lower_bound(bv.begin(), bv.end(), av[i]);
            if (it == bv.end() || *it != av[i])
                continue;
            const size_t j = static_cast<size_t>(it - bv.begin());
            if (alive_b(j))
                use(av[i], a.distances[i], b.distances[j]);
        }
        return;
    }
    if (search_b < linear)
    {
        for (size_t j = 0; j < bv.size(); ++j)
        {
            if (!alive_b(j))
                continue;
            auto it = std::lower_bound(av.begin(), av.end(), bv[j]);
            if (it == av.end() || *it != bv[j])
                continue;
            const size_t i = static_cast<size_t>(it - av.begin());
            if (alive_a(i))
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
            if (alive_a(i) && alive_b(j))
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
    struct WitnessValue
    {
        double value = 0.0;
        std::uint8_t witness = 0;
    };

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
        return AtWithWitness(vertex, mask, group_distance).value;
    }

    WitnessValue AtWithWitness(
        int vertex,
        int mask,
        const std::vector<std::vector<double>>& group_distance) const
    {
        if (!mask)
            return {};
        if (!(mask & (mask - 1)))
            return {group_distance[FirstBit(mask)][vertex], 0};

        double tour = fp::kInf;
        std::uint8_t witness = 255;
        for (size_t i = 0; i < endpoints_[mask].size(); ++i)
        {
            const Endpoint& endpoint = endpoints_[mask][i];
            const double candidate = group_distance[endpoint.left][vertex] + endpoint.path +
                                     group_distance[endpoint.right][vertex];
            if (candidate < tour)
            {
                tour = candidate;
                witness = static_cast<std::uint8_t>(i);
            }
        }
        return {tour * 0.5, witness};
    }

    double FromWitness(
        int vertex,
        int mask,
        std::uint8_t witness,
        const std::vector<std::vector<double>>& group_distance) const
    {
        if (!mask)
            return 0.0;
        if (!(mask & (mask - 1)))
            return group_distance[FirstBit(mask)][vertex];
        if (witness == 255)
            return fp::kInf * 0.5;
        const Endpoint& endpoint = endpoints_[mask][witness];
        return 0.5 * (group_distance[endpoint.left][vertex] + endpoint.path +
                      group_distance[endpoint.right][vertex]);
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
    std::vector<std::uint8_t> heuristic_witness(n + 1);
    std::vector<int> heuristic_stamp(n + 1);
    std::vector<int> touched;
    std::vector<int> settled;
    std::vector<std::pair<int, int>> complement_pairs;
    std::vector<double> complement_row(n + 1, fp::kInf);
    int stamp = 0;
    long long live_states = 0;
    int current_size = 0;
    double compacted_at_best = best;

    auto Available = [&](int mask)
    {
        return mask && popcount[mask] <= half &&
               (popcount[mask] == 1 || rows[mask].ready);
    };
    auto DenseAlive = [&](int mask, const Row& row, int vertex)
    {
        return row.distances[vertex] < fp::kInf &&
               row.distances[vertex] + lower_bound.FromWitness(
                                           vertex,
                                           full_mask ^ mask,
                                           row.witness[vertex],
                                           group_distance) <=
                   best;
    };
    auto SparseAlive = [&](int mask, const Row& row, size_t index)
    {
        const int vertex = row.vertices[index];
        return row.distances[index] + lower_bound.FromWitness(
                                          vertex,
                                          full_mask ^ mask,
                                          row.witness[index],
                                          group_distance) <=
               best;
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
            return DenseAlive(mask, row, vertex) ? row.distances[vertex] : fp::kInf;
        auto it = std::lower_bound(row.vertices.begin(), row.vertices.end(), vertex);
        if (it == row.vertices.end() || *it != vertex)
            return fp::kInf;
        const size_t index = static_cast<size_t>(it - row.vertices.begin());
        return SparseAlive(mask, row, index) ? row.distances[index] : fp::kInf;
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
            const int old_count = row.count;
            if (row.dense)
            {
                int count = 0;
                for (int v = 1; v <= n; ++v)
                {
                    if (row.distances[v] >= fp::kInf)
                        continue;
                    if (!DenseAlive(mask, row, v))
                    {
                        row.distances[v] = fp::kInf;
                        row.witness[v] = 255;
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
                    if (!SparseAlive(mask, row, i))
                        continue;
                    if (write != i)
                    {
                        row.vertices[write] = row.vertices[i];
                        row.distances[write] = row.distances[i];
                        row.witness[write] = row.witness[i];
                    }
                    ++write;
                }
                row.vertices.resize(write);
                row.distances.resize(write);
                row.witness.resize(write);
                row.count = write;
            }
            live_states -= old_count - row.count;
        }
        compacted_at_best = best;
    };

    for (int mask : order)
    {
        const int size = popcount[mask];
        if (size == 1)
            continue;
        if (size != current_size)
        {
            if (current_size && best < compacted_at_best)
                CompactRows();
            current_size = size;
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
                const auto value =
                    lower_bound.AtWithWitness(vertex, remaining_mask, group_distance);
                heuristic[vertex] = value.value;
                heuristic_witness[vertex] = value.witness;
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
                            if (DenseAlive(other, row, v))
                                Set(v, row.distances[v] + group_distance[group][v]);
                    }
                    else
                    {
                        for (size_t i = 0; i < row.vertices.size(); ++i)
                        {
                            if (SparseAlive(other, row, i))
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
                            if (DenseAlive(left, a, v) && DenseAlive(right, b, v))
                                Set(v, a.distances[v] + b.distances[v]);
                    }
                    else if (a.dense || b.dense)
                    {
                        const Row& dense = a.dense ? a : b;
                        const Row& sparse = a.dense ? b : a;
                        const int dense_mask = a.dense ? left : right;
                        const int sparse_mask = a.dense ? right : left;
                        for (size_t i = 0; i < sparse.vertices.size(); ++i)
                        {
                            const int v = sparse.vertices[i];
                            if (SparseAlive(sparse_mask, sparse, i) &&
                                DenseAlive(dense_mask, dense, v))
                                Set(v, sparse.distances[i] + dense.distances[v]);
                        }
                    }
                    else
                    {
                        JoinRows(a,
                                 b,
                                 [&](size_t i) { return SparseAlive(left, a, i); },
                                 [&](size_t i) { return SparseAlive(right, b, i); },
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
                            if (DenseAlive(nonempty, row, v))
                                PutComplement(v, row.distances[v]);
                    }
                    else
                    {
                        for (size_t i = 0; i < row.vertices.size(); ++i)
                            if (SparseAlive(nonempty, row, i))
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
                        if (DenseAlive(other, row, v))
                            PutComplement(v, row.distances[v] + group_distance[group][v]);
                }
                else
                {
                    for (size_t i = 0; i < row.vertices.size(); ++i)
                        if (SparseAlive(other, row, i))
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
                    if (DenseAlive(left, a, v) && DenseAlive(right, b, v))
                        PutComplement(v, a.distances[v] + b.distances[v]);
            }
            else if (a.dense || b.dense)
            {
                const Row& dense = a.dense ? a : b;
                const Row& sparse = a.dense ? b : a;
                const int dense_mask = a.dense ? left : right;
                const int sparse_mask = a.dense ? right : left;
                for (size_t i = 0; i < sparse.vertices.size(); ++i)
                {
                    const int v = sparse.vertices[i];
                    if (SparseAlive(sparse_mask, sparse, i) &&
                        DenseAlive(dense_mask, dense, v))
                        PutComplement(v, sparse.distances[i] + dense.distances[v]);
                }
            }
            else
            {
                JoinRows(a,
                         b,
                         [&](size_t i) { return SparseAlive(left, a, i); },
                         [&](size_t i) { return SparseAlive(right, b, i); },
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

        if (needed_later[mask])
        {
            const size_t baseline_sparse_bytes =
                settled.size() * (sizeof(int) + 2 * sizeof(double));
            const size_t baseline_dense_bytes =
                static_cast<size_t>(n + 1) * 2 * sizeof(double);
            const size_t sparse_bytes =
                settled.size() * (sizeof(int) + sizeof(double) + sizeof(std::uint8_t));
            const size_t dense_bytes =
                static_cast<size_t>(n + 1) * (sizeof(double) + sizeof(std::uint8_t));
            row.dense = dense_bytes < sparse_bytes;
            row.count = keep_count;
            if (row.dense)
            {
                row.distances.assign(n + 1, fp::kInf);
                row.witness.assign(n + 1, 255);
                for (int v : settled)
                {
                    row.distances[v] = distance[v];
                    row.witness[v] = heuristic_witness[v];
                }
            }
            else
            {
                row.vertices = settled;
                row.distances.reserve(settled.size());
                row.witness.reserve(settled.size());
                for (int v : settled)
                {
                    row.distances.push_back(distance[v]);
                    row.witness.push_back(heuristic_witness[v]);
                }
            }
            row.ready = true;
            live_states += row.count;
            stats.witness_states += row.count;
            stats.witness_bytes += row.dense ? dense_bytes : sparse_bytes;
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

}  // namespace gst::tools::tsp_witness
