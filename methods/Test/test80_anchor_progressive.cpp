#include "test80_anchor_progressive.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include "../../float_compare.h"
#include "../../query_feasibility.h"
#include "../Common/anchor_junction_upper.h"
#include "../Common/dual_cut_potential.h"

namespace gst::methods::test80_anchor_progressive
{
namespace
{
using Clock = std::chrono::steady_clock;
using DenseRow = std::vector<double>;
using HeapItem = std::pair<double, int>;
using Heap = std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>>;

struct OrdinaryRow
{
    std::vector<int> vertices;
    std::vector<double> distances;
    std::vector<std::uint64_t> branch_bits;
    size_t branch_count = 0;
    bool ready = false;
    bool dense = false;

    bool IsBranch(size_t index) const
    {
        return (branch_bits[index >> 6] >> (index & 63)) & 1ULL;
    }
};

long long RowStorageBytes(const OrdinaryRow& row)
{
    return static_cast<long long>(row.vertices.capacity()) * sizeof(int) +
           static_cast<long long>(row.distances.capacity()) * sizeof(double) +
           static_cast<long long>(row.branch_bits.capacity()) * sizeof(std::uint64_t);
}

long long ValueScanWork(const OrdinaryRow& row, int n)
{
    return row.dense ? n : static_cast<long long>(row.vertices.size());
}

long long BranchScanWork(const OrdinaryRow& row)
{
    return static_cast<long long>(row.branch_bits.size()) +
           static_cast<long long>(row.branch_count);
}

int FirstBit64(std::uint64_t bits)
{
#ifdef _MSC_VER
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
    for (size_t bound = 2; bound < size + 1; bound <<= 1)
        ++cost;
    return cost;
}

struct RowHeapNode
{
    double key = 0.0;
    double distance = 0.0;
    int vertex = 0;

    bool operator>(const RowHeapNode& other) const
    {
        if (key != other.key)
            return key > other.key;
        if (distance != other.distance)
            return distance > other.distance;
        return vertex > other.vertex;
    }
};

int FirstBit(int mask)
{
    int bit = 0;
    while (!(mask & (1 << bit)))
        ++bit;
    return bit;
}

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

std::vector<std::vector<double>> GroupDistances(const Graph& graph, const Query& query)
{
    std::vector<std::vector<double>> result(
        query.groups.size(), std::vector<double>(graph.n + 1, fp::kInf));
    for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
    {
        Heap heap;
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
            for (const auto& edge : graph.adj[vertex])
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

double RootStarUpper(const std::vector<std::vector<double>>& group_distance, int n, int& root)
{
    double best = fp::kInf;
    for (int vertex = 1; vertex <= n; ++vertex)
    {
        double value = 0.0;
        for (const auto& distance : group_distance)
            value += distance[vertex];
        if (value < best)
        {
            best = value;
            root = vertex;
        }
    }
    return best;
}

class TourLowerBound
{
public:
    void Build(const std::vector<std::vector<double>>& metric)
    {
        group_count_ = static_cast<int>(metric.size());
        const int subset_count = 1 << group_count_;
        const int full_mask = subset_count - 1;
        std::vector<double> path(
            static_cast<size_t>(subset_count) * group_count_ * group_count_, fp::kInf);
        auto Index = [&](int mask, int start, int last)
        {
            return (static_cast<size_t>(mask) * group_count_ + start) * group_count_ + last;
        };

        for (int start = 0; start < group_count_; ++start)
        {
            path[Index(1 << start, start, start)] = 0.0;
            for (int mask = 1; mask < subset_count; ++mask)
            {
                if (!(mask & (1 << start)))
                    continue;
                for (int last = 0; last < group_count_; ++last)
                {
                    const double current = path[Index(mask, start, last)];
                    if (current >= fp::kInf)
                        continue;
                    for (int bits = full_mask ^ mask; bits; bits &= bits - 1)
                    {
                        const int next = FirstBit(bits & -bits);
                        double& destination = path[Index(mask | (1 << next), start, next)];
                        destination = std::min(destination, current + metric[last][next]);
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
                    const double value = std::min(path[Index(mask, left, right)],
                                                  path[Index(mask, right, left)]);
                    if (value < fp::kInf)
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
            return group_distance[FirstBit(mask)][vertex];
        double value = fp::kInf;
        for (const Endpoint& endpoint : endpoints_[mask])
        {
            value = std::min(value,
                             group_distance[endpoint.left][vertex] + endpoint.path +
                                 group_distance[endpoint.right][vertex]);
        }
        return value * 0.5;
    }

private:

    struct Endpoint
    {

        int left = 0;
        int right = 0;
        double path = 0.0;
    };

    int group_count_ = 0;
    std::vector<std::vector<Endpoint>> endpoints_;
};

}  // namespace

SolveResult SolveOneQuery(const Graph& graph, const Query& query)
{
    SolveResult result;
    Test80Stats& stats = result.stats;
    stats.n = graph.n;
    stats.m = graph.m;
    stats.g = static_cast<int>(query.groups.size());

    if (!stats.g)
        return {0.0, true, stats};
    if (stats.g > 16)
        throw std::runtime_error("Test80 supports group count <= 16.");
    if (!IsQueryFeasible(graph, query))
        return result;

    stats.min_group_size = graph.n;
    for (const auto& group : query.groups)
    {
        const int size = static_cast<int>(group.size());
        stats.min_group_size = std::min(stats.min_group_size, size);
        stats.max_group_size = std::max(stats.max_group_size, size);
        stats.total_group_vertices += size;
    }

    const auto total_start = Clock::now();
    const auto distance_start = Clock::now();
    const std::vector<std::vector<double>> group_distance = GroupDistances(graph, query);
    stats.group_distance_bytes =
        static_cast<long long>(stats.g) * (graph.n + 1LL) * sizeof(double);
    stats.group_distance_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - distance_start).count();

    int root_star_root = 1;
    double best = RootStarUpper(group_distance, graph.n, root_star_root);
    stats.root_star_root = root_star_root;
    stats.root_star_upper = best;
    if (stats.g == 1)
    {
        stats.total_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - total_start).count();
        result.best_weight = 0.0;
        result.feasible = true;
        return result;
    }
    int anchor_group = 0;
    for (int group = 1; group < stats.g; ++group)
        if (group_distance[group][root_star_root] >
            group_distance[anchor_group][root_star_root])
            anchor_group = group;
    stats.anchor_group = anchor_group + 1;
    stats.anchor_group_size = static_cast<int>(query.groups[anchor_group].size());
    stats.half = stats.g / 2;

    std::vector<double> root_group_distances;
    root_group_distances.reserve(stats.g);
    for (int group = 0; group < stats.g; ++group)
        root_group_distances.push_back(group_distance[group][root_star_root]);
    std::sort(root_group_distances.begin(), root_group_distances.end());
    stats.root_group_distance_min = root_group_distances.front();
    stats.root_group_distance_max = root_group_distances.back();
    stats.root_group_distance_second_max =
        root_group_distances.size() > 1 ? root_group_distances[root_group_distances.size() - 2]
                                        : root_group_distances.back();
    stats.root_group_distance_average =
        std::accumulate(root_group_distances.begin(), root_group_distances.end(), 0.0) /
        stats.g;

    std::vector<std::vector<double>> group_metric(
        stats.g, std::vector<double>(stats.g, fp::kInf));
    for (int left = 0; left < stats.g; ++left)
        for (int right = 0; right < stats.g; ++right)
            for (int vertex : query.groups[right])
                group_metric[left][right] =
                    std::min(group_metric[left][right], group_distance[left][vertex]);
    TourLowerBound lower_bound;
    lower_bound.Build(group_metric);

    std::vector<int> bit_to_group;
    bit_to_group.reserve(stats.g - 1);
    for (int group = 0; group < stats.g; ++group)
        if (group != anchor_group)
            bit_to_group.push_back(group);

    const int nonanchor_count = static_cast<int>(bit_to_group.size());
    const int subset_count = 1 << nonanchor_count;
    const int full_mask = subset_count - 1;
    const int original_full_mask = (1 << stats.g) - 1;
    const int anchor_bit = 1 << anchor_group;
    const int nonanchor_original_mask = original_full_mask ^ anchor_bit;
    std::vector<int> popcount(subset_count);
    std::vector<int> original_mask(subset_count);
    for (int mask = 1; mask < subset_count; ++mask)
    {
        popcount[mask] = popcount[mask >> 1] + (mask & 1);
        const int bit = FirstBit(mask & -mask);
        original_mask[mask] = original_mask[mask ^ (1 << bit)] | (1 << bit_to_group[bit]);
    }

    for (int mask = 1; mask < (1 << stats.g); ++mask)
        if (CountBits(mask) <= stats.half)
            ++stats.original_half_masks;

    const auto dual_start = Clock::now();
    gst::methods::dual_cut::DualCutPotential dual;
    dual.BuildKeepingResidual(graph, query, group_distance, root_star_root);
    stats.dual_primal_upper = dual.PrimalUpper();
    best = std::min(best, stats.dual_primal_upper);
    stats.root_tour_lower = lower_bound.At(root_star_root, original_full_mask, group_distance);
    stats.root_dual_lower = dual.At(root_star_root, original_full_mask);
    stats.dual_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - dual_start).count();

    stats.junction_before = best;
    const auto junction_start = Clock::now();
    const auto junction = gst::methods::anchor_junction::BuildUpper(
        graph, query, group_distance, root_star_root, anchor_group);
    best = std::min(best, junction.upper);
    stats.junction_after = best;
    stats.junction_upper = junction.upper;
    stats.junction_path_vertices = static_cast<int>(junction.anchor_path.size());
    stats.junction_candidate_roots = junction.candidate_roots;
    stats.junction_tree_vertices = junction.tree_vertices;
    stats.junction_triple_scans = junction.triple_scans;
    stats.junction_convolutions = junction.convolutions;
    stats.junction_work = junction.work;
    stats.junction_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - junction_start).count();

    auto FutureBound = [&](int vertex, int remaining_original)
    {
        return std::max(lower_bound.At(vertex, remaining_original, group_distance),
                        dual.At(vertex, remaining_original));
    };
    std::vector<OrdinaryRow> ordinary(subset_count);
    auto Available = [&](int mask)
    {
        return mask && (popcount[mask] == 1 || ordinary[mask].ready);
    };
    auto ForEachValue = [&](int mask, auto&& use)
    {
        if (popcount[mask] == 1)
        {
            const auto& values = group_distance[bit_to_group[FirstBit(mask)]];
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                use(vertex, values[vertex]);
            return;
        }
        const OrdinaryRow& row = ordinary[mask];
        if (row.dense)
        {
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                if (row.distances[vertex] < fp::kInf)
                    use(vertex, row.distances[vertex]);
            return;
        }
        for (size_t index = 0; index < row.vertices.size(); ++index)
            use(row.vertices[index], row.distances[index]);
    };
    auto ForEachBranchIndex = [&](const OrdinaryRow& row, auto&& use)
    {
        for (size_t word_index = 0; word_index < row.branch_bits.size(); ++word_index)
        {
            std::uint64_t bits = row.branch_bits[word_index];
            while (bits)
            {
                const int bit = FirstBit64(bits);
                use((word_index << 6) + static_cast<size_t>(bit));
                bits &= bits - 1;
            }
        }
    };
    auto ForEachBranchValue = [&](int mask, auto&& use)
    {
        if (popcount[mask] == 1)
        {
            const auto& values = group_distance[bit_to_group[FirstBit(mask)]];
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                use(vertex, values[vertex]);
            return;
        }
        const OrdinaryRow& row = ordinary[mask];
        ForEachBranchIndex(row, [&](size_t index)
        {
            const int vertex = row.dense ? static_cast<int>(index) : row.vertices[index];
            use(vertex, row.distances[index]);
        });
    };
    auto ForEachPairValues = [&](int left, int right, auto&& use)
    {
        if (!left || !right)
        {
            ForEachValue(left | right, [&](int vertex, double value)
            {
                if (left)
                    use(vertex, value, 0.0);
                else
                    use(vertex, 0.0, value);
            });
            return;
        }
        if (popcount[left] == 1 || popcount[right] == 1)
        {
            const int singleton = popcount[left] == 1 ? left : right;
            const int other = singleton == left ? right : left;
            const auto& singleton_distance =
                group_distance[bit_to_group[FirstBit(singleton)]];
            ForEachValue(other, [&](int vertex, double value)
            {
                if (singleton == left)
                    use(vertex, singleton_distance[vertex], value);
                else
                    use(vertex, value, singleton_distance[vertex]);
            });
            return;
        }
        const OrdinaryRow& a = ordinary[left];
        const OrdinaryRow& b = ordinary[right];
        if (a.dense && b.dense)
        {
            for (int vertex = 1; vertex <= graph.n; ++vertex)
            {
                if (a.distances[vertex] < fp::kInf && b.distances[vertex] < fp::kInf)
                    use(vertex, a.distances[vertex], b.distances[vertex]);
            }
            return;
        }
        if (a.dense)
        {
            for (size_t index = 0; index < b.vertices.size(); ++index)
            {
                const int vertex = b.vertices[index];
                if (a.distances[vertex] < fp::kInf)
                    use(vertex, a.distances[vertex], b.distances[index]);
            }
            return;
        }
        if (b.dense)
        {
            for (size_t index = 0; index < a.vertices.size(); ++index)
            {
                const int vertex = a.vertices[index];
                if (b.distances[vertex] < fp::kInf)
                    use(vertex, a.distances[index], b.distances[vertex]);
            }
            return;
        }
        size_t left_index = 0;
        size_t right_index = 0;
        while (left_index < a.vertices.size() && right_index < b.vertices.size())
        {
            if (a.vertices[left_index] < b.vertices[right_index])
                ++left_index;
            else if (b.vertices[right_index] < a.vertices[left_index])
                ++right_index;
            else
            {
                use(a.vertices[left_index],
                    a.distances[left_index],
                    b.distances[right_index]);
                ++left_index;
                ++right_index;
            }

        }
    };
    auto ForEachPivotBranchPair = [&](int accumulator, int branch, auto&& use)
    {
        if (popcount[branch] == 1)
        {
            ++stats.ordinary_join_direct_calls;
            stats.ordinary_join_direct_work +=
                popcount[accumulator] == 1
                    ? graph.n
                    : ValueScanWork(ordinary[accumulator], graph.n);
            const auto& branch_distance =
                group_distance[bit_to_group[FirstBit(branch)]];
            ForEachValue(accumulator, [&](int vertex, double value)
            {
                use(vertex, value, branch_distance[vertex]);
            });
            return;
        }
        if (popcount[accumulator] == 1)
        {
            ++stats.ordinary_join_direct_calls;
            stats.ordinary_join_direct_work += BranchScanWork(ordinary[branch]);
            const auto& accumulator_distance =
                group_distance[bit_to_group[FirstBit(accumulator)]];
            ForEachBranchValue(branch, [&](int vertex, double value)
            {
                use(vertex, accumulator_distance[vertex], value);
            });
            return;
        }

        const OrdinaryRow& a = ordinary[accumulator];
        const OrdinaryRow& b = ordinary[branch];
        if (a.dense)
        {
            ++stats.ordinary_join_direct_calls;
            stats.ordinary_join_direct_work += BranchScanWork(b);
            ForEachBranchIndex(b, [&](size_t index)
            {
                const int vertex = b.dense ? static_cast<int>(index) : b.vertices[index];
                if (a.distances[vertex] < fp::kInf)
                    use(vertex, a.distances[vertex], b.distances[index]);
            });
            return;
        }
        if (b.dense)
        {
            const long long search_cost =
                static_cast<long long>(b.branch_count) * BinarySearchCost(a.vertices.size());
            if (search_cost < static_cast<long long>(a.vertices.size()))
            {
                ++stats.ordinary_join_binary_calls;
                stats.ordinary_join_binary_work += BranchScanWork(b) + search_cost;
                ForEachBranchIndex(b, [&](size_t index)
                {
                    const int vertex = static_cast<int>(index);
                    const auto found =
                        std::lower_bound(a.vertices.begin(), a.vertices.end(), vertex);
                    if (found != a.vertices.end() && *found == vertex)
                    {
                        const size_t a_index =
                            static_cast<size_t>(found - a.vertices.begin());
                        use(vertex, a.distances[a_index], b.distances[index]);
                    }
                });
            }
            else
            {
                ++stats.ordinary_join_direct_calls;
                stats.ordinary_join_direct_work += a.vertices.size();
                for (size_t index = 0; index < a.vertices.size(); ++index)
                {
                    const int vertex = a.vertices[index];
                    if (b.IsBranch(vertex))
                        use(vertex, a.distances[index], b.distances[vertex]);
                }
            }
            return;
        }

        const long long linear_cost =
            static_cast<long long>(a.vertices.size() + b.vertices.size());
        const long long branch_search_cost =
            static_cast<long long>(b.branch_count) * BinarySearchCost(a.vertices.size());
        const long long accumulator_search_cost =
            static_cast<long long>(a.vertices.size()) * BinarySearchCost(b.vertices.size());
        if (branch_search_cost < linear_cost &&
            branch_search_cost <= accumulator_search_cost)
        {
            ++stats.ordinary_join_binary_calls;
            stats.ordinary_join_binary_work += BranchScanWork(b) + branch_search_cost;
            ForEachBranchIndex(b, [&](size_t index)
            {
                const int vertex = b.vertices[index];
                const auto found =
                    std::lower_bound(a.vertices.begin(), a.vertices.end(), vertex);
                if (found != a.vertices.end() && *found == vertex)
                {
                    const size_t a_index = static_cast<size_t>(found - a.vertices.begin());
                    use(vertex, a.distances[a_index], b.distances[index]);
                }
            });
            return;
        }
        if (accumulator_search_cost < linear_cost)
        {
            ++stats.ordinary_join_binary_calls;
            stats.ordinary_join_binary_work += accumulator_search_cost;
            for (size_t index = 0; index < a.vertices.size(); ++index)
            {
                const int vertex = a.vertices[index];
                const auto found =
                    std::lower_bound(b.vertices.begin(), b.vertices.end(), vertex);
                if (found == b.vertices.end() || *found != vertex)
                    continue;
                const size_t b_index = static_cast<size_t>(found - b.vertices.begin());
                if (b.IsBranch(b_index))
                    use(vertex, a.distances[index], b.distances[b_index]);
            }
            return;
        }
        ++stats.ordinary_join_linear_calls;
        stats.ordinary_join_linear_work += linear_cost;
        size_t a_index = 0;
        size_t b_index = 0;
        while (a_index < a.vertices.size() && b_index < b.vertices.size())
        {
            if (a.vertices[a_index] < b.vertices[b_index])
                ++a_index;
            else if (b.vertices[b_index] < a.vertices[a_index])
                ++b_index;
            else
            {
                if (b.IsBranch(b_index))
                    use(a.vertices[a_index],
                        a.distances[a_index],
                        b.distances[b_index]);
                ++a_index;
                ++b_index;
            }
        }
    };

    std::vector<double> row_distance(graph.n + 1, fp::kInf);
    std::vector<double> split_distance(graph.n + 1, fp::kInf);

    std::vector<double> heuristic(graph.n + 1);
    std::vector<int> heuristic_stamp(graph.n + 1);
    std::vector<int> touched;
    std::vector<int> settled;
    std::vector<int> seed_vertices;
    std::vector<int> branch_vertices;
    int stamp = 0;
    auto StoreOrdinaryRow = [&](int mask,
                                const std::vector<int>& vertices,
                                const std::vector<int>& branch_vertices)
    {
        OrdinaryRow& row = ordinary[mask];
        const size_t sparse_bytes = vertices.size() * (sizeof(int) + sizeof(double));
        const size_t dense_bytes = static_cast<size_t>(graph.n + 1) * sizeof(double);
        row.dense = dense_bytes < sparse_bytes;
        if (row.dense)
        {
            row.distances.assign(graph.n + 1, fp::kInf);
            for (int vertex : vertices)
                row.distances[vertex] = row_distance[vertex];
        }
        else
        {
            row.vertices = vertices;
            row.distances.reserve(vertices.size());
            for (int vertex : vertices)
                row.distances.push_back(row_distance[vertex]);
        }
        const size_t branch_domain = row.dense ? dense_bytes / sizeof(double) : vertices.size();
        row.branch_count = branch_vertices.size();
        row.branch_bits.assign((branch_domain + 63) / 64, 0);
        if (row.dense)
        {
            for (int vertex : branch_vertices)
                row.branch_bits[static_cast<size_t>(vertex) >> 6] |=
                    1ULL << (vertex & 63);
        }
        else
        {
            size_t branch_index = 0;
            for (size_t index = 0; index < vertices.size() &&
                                   branch_index < branch_vertices.size();
                 ++index)
            {
                if (vertices[index] != branch_vertices[branch_index])
                    continue;
                row.branch_bits[index >> 6] |= 1ULL << (index & 63);
                ++branch_index;
            }
        }
        row.ready = true;
        const int size = popcount[mask];
        if (row.dense)
            ++stats.ordinary_dense_rows_by_size[size];
        else
            ++stats.ordinary_sparse_rows_by_size[size];
        stats.ordinary_row_bytes_by_size[size] += RowStorageBytes(row);
    };
    auto ForEachTripleValue = [&](int first, int second, int third, auto&& use)
    {
        const int masks[3] = {first, second, third};
        const OrdinaryRow* rows[3] = {nullptr, nullptr, nullptr};
        const std::vector<int>* driver = nullptr;
        for (int index = 0; index < 3; ++index)
        {
            const int mask = masks[index];
            if (mask && popcount[mask] > 1)
            {
                rows[index] = &ordinary[mask];
                if (!rows[index]->dense &&
                    (!driver || rows[index]->vertices.size() < driver->size()))
                    driver = &rows[index]->vertices;
            }
        }

        size_t positions[3] = {0, 0, 0};
        auto Visit = [&](int vertex)
        {
            ++stats.early_upper_probes;
            double total = group_distance[anchor_group][vertex];
            for (int index = 0; index < 3; ++index)
            {
                const int mask = masks[index];
                if (!mask)
                    continue;
                if (popcount[mask] == 1)
                {
                    total += group_distance[bit_to_group[FirstBit(mask)]][vertex];
                    continue;
                }
                const OrdinaryRow& row = *rows[index];
                if (row.dense)
                {
                    if (row.distances[vertex] >= fp::kInf)
                        return;
                    total += row.distances[vertex];
                    continue;
                }
                size_t& position = positions[index];
                while (position < row.vertices.size() && row.vertices[position] < vertex)
                    ++position;
                if (position == row.vertices.size() || row.vertices[position] != vertex)
                    return;
                total += row.distances[position];
            }
            use(vertex, total);
        };

        if (driver)
            for (int vertex : *driver)
                Visit(vertex);
        else
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                Visit(vertex);
    };
    const int early_block_limit = (nonanchor_count + 2) / 3;
    const int quarter_block_limit = (nonanchor_count + 3) / 4;
    double witness_value = fp::kInf;
    int witness_masks[3] = {0, 0, 0};
    auto EvaluateEarlyPartition = [&](int first, int second, int third)
    {
        ++stats.early_upper_partitions;
        ForEachTripleValue(first, second, third, [&](int, double value)
        {
            best = std::min(best, value);
            if (value < witness_value)
            {
                witness_value = value;
                witness_masks[0] = first;
                witness_masks[1] = second;
                witness_masks[2] = third;
            }
        });
    };
    const int pair_row_count = nonanchor_count * (nonanchor_count - 1) / 2;
    int completed_pair_rows = 0;
    bool packing_built = false;
    stats.packing_budget =
        static_cast<long long>(stats.g) * (2LL * graph.m + graph.n);
    if (!pair_row_count || stats.half < 2)
        dual.ReleaseResidual();

    auto BuildPacking = [&]()
    {
        stats.packing_trigger_pair_rows = completed_pair_rows;
        stats.packing_trigger_pair_work = stats.pair_work;
        stats.packing_trigger_pair_values = stats.ordinary_values_by_size[2];
        const auto packing_start = Clock::now();
        const auto packing = dual.StrengthenAlongPath(
            graph, query, junction.anchor_path);
        stats.packing_rounds = packing.rounds;
        stats.packing_min_scale = packing.min_scale;
        stats.packing_average_scale = packing.average_scale;
        stats.packing_max_scale = packing.max_scale;
        stats.packing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - packing_start)
                .count();
        packing_built = true;
    };
    const auto ordinary_start = Clock::now();
    for (int size = 1; size <= stats.half; ++size)
    {
        const auto layer_start = Clock::now();
        if (size == early_block_limit)
            stats.early_upper_before = best;
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (popcount[mask] != size)
                continue;
            ++stats.ordinary_masks;
            ++stats.ordinary_masks_by_size[size];
            if (size == 1)
            {
                stats.ordinary_values += graph.n;
                stats.ordinary_values_by_size[size] += graph.n;
                stats.ordinary_branch_values += graph.n;
                stats.ordinary_branch_values_by_size[size] += graph.n;
                continue;
            }

            ++stamp;
            touched.clear();
            settled.clear();
            const int remaining_original = original_full_mask ^ original_mask[mask];
            auto H = [&](int vertex)
            {
                if (heuristic_stamp[vertex] != stamp)
                {
                    heuristic_stamp[vertex] = stamp;
                    double farthest = 0.0;
                    for (int bits = remaining_original; bits; bits &= bits - 1)
                        farthest = std::max(
                            farthest, group_distance[FirstBit(bits & -bits)][vertex]);
                    const double tour =
                        lower_bound.At(vertex, remaining_original, group_distance);
                    const double cut = dual.At(vertex, remaining_original);
                    heuristic[vertex] = std::max(farthest, std::max(tour, cut));
                    ++stats.ordinary_h_evals_by_size[size];
                    if (farthest >= tour && farthest >= cut)
                        ++stats.ordinary_h_farthest_by_size[size];
                    else if (tour >= cut)
                        ++stats.ordinary_h_tour_by_size[size];
                    else
                        ++stats.ordinary_h_dual_by_size[size];
                }
                return heuristic[vertex];
            };
            auto Set = [&](int vertex, double value)
            {
                ++stats.ordinary_seed_candidates_by_size[size];
                if (value >= row_distance[vertex])
                {
                    ++stats.ordinary_seed_reject_old_by_size[size];
                    return;
                }
                if (value + H(vertex) > best)
                {
                    ++stats.ordinary_seed_reject_bound_by_size[size];
                    return;
                }
                ++stats.ordinary_seed_accept_by_size[size];
                if (row_distance[vertex] == fp::kInf)
                    touched.push_back(vertex);
                row_distance[vertex] = value;
            };

            if (size == 2)
            {
                const int first = FirstBit(mask);
                const int second = FirstBit(mask ^ (1 << first));
                const auto& first_distance = group_distance[bit_to_group[first]];
                const auto& second_distance = group_distance[bit_to_group[second]];
                stats.pair_work += graph.n;
                for (int vertex = 1; vertex <= graph.n; ++vertex)
                    Set(vertex, first_distance[vertex] + second_distance[vertex]);
            }
            else
            {
                const int branch_domain = mask ^ (mask & -mask);
                for (int right = branch_domain; right; right = (right - 1) & branch_domain)
                {
                    const int left = mask ^ right;
                    if (!Available(left) || !Available(right))
                        continue;
                    ForEachPivotBranchPair(left, right, [&](int vertex, double a, double b)
                    {
                        Set(vertex, a + b);
                    });
                }
            }

            seed_vertices = touched;
            for (int vertex : seed_vertices)
                split_distance[vertex] = row_distance[vertex];
            std::priority_queue<RowHeapNode,
                                std::vector<RowHeapNode>,
                                std::greater<RowHeapNode>> queue;
            stats.ordinary_seeds_by_size[size] += touched.size();
            for (int vertex : touched)
            {
                queue.push({row_distance[vertex] + H(vertex), row_distance[vertex], vertex});
                if (size == 2)
                    ++stats.pair_work;
            }
            while (!queue.empty())
            {
                const RowHeapNode node = queue.top();
                queue.pop();
                ++stats.ordinary_queue_pops;
                ++stats.ordinary_pops_by_size[size];
                if (size == 2)
                    ++stats.pair_work;
                if (node.distance != row_distance[node.vertex])
                {
                    ++stats.ordinary_pop_stale_by_size[size];
                    continue;
                }
                if (node.key > best)
                {
                    ++stats.ordinary_pop_bound_by_size[size];
                    continue;
                }
                settled.push_back(node.vertex);
                if (size == 2)
                    stats.pair_work += graph.adj[node.vertex].size();
                for (const auto& edge : graph.adj[node.vertex])
                {
                    const double next = node.distance + edge.w;
                    ++stats.ordinary_relax_attempts_by_size[size];
                    if (next >= row_distance[edge.to])
                    {
                        ++stats.ordinary_relax_reject_old_by_size[size];
                        continue;
                    }
                    if (next + H(edge.to) > best)
                    {
                        ++stats.ordinary_relax_reject_bound_by_size[size];
                        continue;
                    }
                    ++stats.ordinary_relax_accept_by_size[size];
                    if (row_distance[edge.to] == fp::kInf)
                        touched.push_back(edge.to);
                    row_distance[edge.to] = next;
                    queue.push({next + H(edge.to), next, edge.to});
                    if (size == 2)
                        ++stats.pair_work;
                }
            }

            std::sort(settled.begin(), settled.end());
            settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
            branch_vertices.clear();
            for (int vertex : settled)
                if (row_distance[vertex] < split_distance[vertex])
                    branch_vertices.push_back(vertex);
            StoreOrdinaryRow(mask, settled, branch_vertices);
            stats.ordinary_values += settled.size();
            stats.ordinary_values_by_size[size] += settled.size();
            stats.ordinary_branch_values += branch_vertices.size();
            stats.ordinary_branch_values_by_size[size] += branch_vertices.size();
            if (size == early_block_limit)
            {
                const auto early_row_start = Clock::now();
                const int remaining = full_mask ^ mask;
                for (int second = remaining;; second = (second - 1) & remaining)
                {
                    const int third = remaining ^ second;
                    if (second <= third && popcount[second] <= size &&
                        popcount[third] <= size && (!second || Available(second)) &&
                        (!third || Available(third)))
                        EvaluateEarlyPartition(mask, second, third);
                    if (!second)
                        break;
                }
                stats.early_upper_ms +=
                    std::chrono::duration<double, std::milli>(Clock::now() -
                                                               early_row_start)
                        .count();
            }
            for (int vertex : seed_vertices)
                split_distance[vertex] = fp::kInf;
            for (int vertex : touched)
                row_distance[vertex] = fp::kInf;

            if (size == 2)
            {
                // D2 pays for the optional residual strengthening with the
                // same seed, heap, and edge-scan operations it actually uses.
                ++completed_pair_rows;
                if (!packing_built && completed_pair_rows < pair_row_count &&
                    stats.pair_work >= stats.packing_budget)
                    BuildPacking();
            }
        }

        if (size == 2 && !packing_built)
            dual.ReleaseResidual();

        stats.ordinary_ms_by_size[size] =
            std::chrono::duration<double, std::milli>(Clock::now() - layer_start).count();
        if (size == quarter_block_limit && quarter_block_limit < early_block_limit)
        {
            const auto quarter_start = Clock::now();
            stats.quarter_upper_before = best;
            double quarter_witness = fp::kInf;
            int quarter_masks[4] = {0, 0, 0, 0};
            auto RootValue = [&](int mask)
            {
                if (!mask)
                    return 0.0;
                if (popcount[mask] == 1)
                    return group_distance[bit_to_group[FirstBit(mask)]][root_star_root];
                if (!ordinary[mask].ready)
                    return fp::kInf;
                const OrdinaryRow& row = ordinary[mask];
                if (row.dense)
                    return row.distances[root_star_root];
                const auto found =
                    std::lower_bound(row.vertices.begin(), row.vertices.end(), root_star_root);
                if (found == row.vertices.end() || *found != root_star_root)
                    return fp::kInf;
                return row.distances[static_cast<size_t>(found - row.vertices.begin())];
            };

            std::vector<double> pair_value(subset_count, fp::kInf);
            std::vector<int> pair_split(subset_count);
            pair_value[0] = 0.0;
            for (int mask = 1; mask < subset_count; ++mask)
            {
                if (popcount[mask] > 2 * size)
                    continue;
                ++stats.quarter_upper_halves;
                for (int first = mask;; first = (first - 1) & mask)
                {
                    const int second = mask ^ first;
                    if (first <= second && popcount[first] <= size &&
                        popcount[second] <= size)
                    {
                        ++stats.quarter_upper_pair_probes;
                        const double left = RootValue(first);
                        const double right = RootValue(second);
                        if (left + right < pair_value[mask])
                        {
                            pair_value[mask] = left + right;
                            pair_split[mask] = first;
                        }
                    }
                    if (!first)
                        break;
                }
            }
            for (int left_mask = 1; left_mask < full_mask; ++left_mask)
            {
                const int right_mask = full_mask ^ left_mask;
                if (left_mask >= right_mask || popcount[left_mask] > 2 * size ||
                    popcount[right_mask] > 2 * size)
                    continue;
                ++stats.quarter_upper_join_probes;
                const double value = group_distance[anchor_group][root_star_root] +
                                     pair_value[left_mask] + pair_value[right_mask];
                if (value < fp::kInf)
                {
                    best = std::min(best, value);
                    if (value < quarter_witness)
                    {
                        quarter_witness = value;
                        quarter_masks[0] = pair_split[left_mask];
                        quarter_masks[1] = left_mask ^ quarter_masks[0];
                        quarter_masks[2] = pair_split[right_mask];
                        quarter_masks[3] = right_mask ^ quarter_masks[2];
                    }
                }
            }
            stats.quarter_upper_after = best;
            stats.quarter_witness_before = best;
            if (best < stats.quarter_upper_before && quarter_witness < fp::kInf)
            {
                DenseRow quarter_target(graph.n + 1, fp::kInf);
                std::vector<int> quarter_target_vertices;
                for (int choice = 0; choice < 4; ++choice)
                {
                    const int anchor_side = quarter_masks[choice];
                    const int first = quarter_masks[(choice + 1) % 4];
                    const int second = quarter_masks[(choice + 2) % 4];
                    const int third = quarter_masks[(choice + 3) % 4];
                    quarter_target_vertices.clear();
                    ForEachTripleValue(first, second, third, [&](int vertex, double value)
                    {
                        if (quarter_target[vertex] == fp::kInf)
                            quarter_target_vertices.push_back(vertex);
                        quarter_target[vertex] =
                            std::min(quarter_target[vertex],
                                     value - group_distance[anchor_group][vertex]);
                    });

                    ++stamp;
                    touched.clear();
                    const int remaining_nonanchor = full_mask ^ anchor_side;
                    const int remaining_original =
                        nonanchor_original_mask ^ original_mask[anchor_side];
                    auto H = [&](int vertex)
                    {
                        if (heuristic_stamp[vertex] != stamp)
                        {
                            heuristic_stamp[vertex] = stamp;
                            double farthest = 0.0;
                            for (int bits = remaining_nonanchor; bits; bits &= bits - 1)
                                farthest = std::max(
                                    farthest,
                                    group_distance
                                        [bit_to_group[FirstBit(bits & -bits)]][vertex]);
                            heuristic[vertex] =
                                std::max(farthest, FutureBound(vertex, remaining_original));
                        }
                        return heuristic[vertex];
                    };
                    auto Set = [&](int vertex, double value)
                    {
                        if (value >= row_distance[vertex] || value + H(vertex) > best)
                            return;
                        if (row_distance[vertex] == fp::kInf)
                            touched.push_back(vertex);
                        row_distance[vertex] = value;
                    };
                    if (!anchor_side)
                    {
                        for (int vertex = 1; vertex <= graph.n; ++vertex)
                            Set(vertex, group_distance[anchor_group][vertex]);
                    }
                    else
                    {
                        ForEachValue(anchor_side, [&](int vertex, double value)
                        {
                            Set(vertex, group_distance[anchor_group][vertex] + value);
                        });
                    }

                    std::priority_queue<RowHeapNode,
                                        std::vector<RowHeapNode>,
                                        std::greater<RowHeapNode>> queue;
                    for (int vertex : touched)
                        queue.push({row_distance[vertex] + H(vertex),
                                    row_distance[vertex],
                                    vertex});
                    while (!queue.empty())
                    {
                        const RowHeapNode node = queue.top();
                        queue.pop();
                        ++stats.quarter_witness_pops;
                        if (node.distance != row_distance[node.vertex] || node.key > best)
                            continue;
                        if (quarter_target[node.vertex] < fp::kInf)
                            best = std::min(best,
                                            node.distance + quarter_target[node.vertex]);
                        ++stats.quarter_witness_values;
                        for (const auto& edge : graph.adj[node.vertex])
                        {
                            const double next = node.distance + edge.w;
                            if (next >= row_distance[edge.to] || next + H(edge.to) > best)
                                continue;
                            if (row_distance[edge.to] == fp::kInf)
                                touched.push_back(edge.to);
                            row_distance[edge.to] = next;
                            queue.push({next + H(edge.to), next, edge.to});
                        }
                    }
                    for (int vertex : touched)
                        row_distance[vertex] = fp::kInf;
                    for (int vertex : quarter_target_vertices)
                        quarter_target[vertex] = fp::kInf;
                }
            }
            stats.quarter_witness_after = best;
            stats.quarter_upper_ms =
                std::chrono::duration<double, std::milli>(Clock::now() - quarter_start)
                    .count();
        }
        if (size == early_block_limit)
        {
            const auto early_start = Clock::now();
            if (size == 1)
            {
                for (int first = full_mask;; first = (first - 1) & full_mask)
                {
                    const int remaining = full_mask ^ first;
                    for (int second = remaining;; second = (second - 1) & remaining)
                    {
                        const int third = remaining ^ second;
                        if (first <= second && second <= third &&
                            popcount[first] <= size && popcount[second] <= size &&
                            popcount[third] <= size && (!first || Available(first)) &&
                            (!second || Available(second)) &&
                            (!third || Available(third)))
                            EvaluateEarlyPartition(first, second, third);
                        if (!second)
                            break;
                    }
                    if (!first)
                        break;
                }
            }

            stats.early_witness_before = best;
            DenseRow witness_target(graph.n + 1, fp::kInf);
            std::vector<int> witness_target_vertices;
            for (int choice = 0; choice < 3 && witness_value < fp::kInf; ++choice)
            {
                const int anchor_side = witness_masks[choice];
                const int left = witness_masks[(choice + 1) % 3];
                const int right = witness_masks[(choice + 2) % 3];
                witness_target_vertices.clear();
                ForEachPairValues(left, right, [&](int vertex, double a, double b)
                {
                    if (witness_target[vertex] == fp::kInf)
                        witness_target_vertices.push_back(vertex);
                    witness_target[vertex] = std::min(witness_target[vertex], a + b);
                });

                ++stamp;
                touched.clear();
                settled.clear();
                const int remaining_nonanchor = full_mask ^ anchor_side;
                const int remaining_original =
                    nonanchor_original_mask ^ original_mask[anchor_side];
                auto H = [&](int vertex)
                {
                    if (heuristic_stamp[vertex] != stamp)
                    {
                        heuristic_stamp[vertex] = stamp;
                        double farthest = 0.0;
                        for (int bits = remaining_nonanchor; bits; bits &= bits - 1)
                            farthest = std::max(
                                farthest,
                                group_distance
                                    [bit_to_group[FirstBit(bits & -bits)]][vertex]);
                        heuristic[vertex] =
                            std::max(farthest, FutureBound(vertex, remaining_original));
                    }
                    return heuristic[vertex];
                };
                auto Set = [&](int vertex, double value)
                {
                    if (value >= row_distance[vertex] || value + H(vertex) > best)
                        return;
                    if (row_distance[vertex] == fp::kInf)
                        touched.push_back(vertex);
                    row_distance[vertex] = value;
                };
                if (!anchor_side)
                {
                    for (int vertex = 1; vertex <= graph.n; ++vertex)
                        Set(vertex, group_distance[anchor_group][vertex]);
                }
                else
                {
                    ForEachValue(anchor_side, [&](int vertex, double value)
                    {
                        Set(vertex, group_distance[anchor_group][vertex] + value);
                    });
                }


                std::priority_queue<RowHeapNode,
                                    std::vector<RowHeapNode>,
                                    std::greater<RowHeapNode>> queue;
                for (int vertex : touched)
                    queue.push({row_distance[vertex] + H(vertex),
                                row_distance[vertex],

                                vertex});
                while (!queue.empty())
                {
                    const RowHeapNode node = queue.top();
                    queue.pop();
                    ++stats.early_witness_pops;
                    if (node.distance != row_distance[node.vertex] || node.key > best)
                        continue;
                    if (witness_target[node.vertex] < fp::kInf)
                        best = std::min(best,
                                        node.distance + witness_target[node.vertex]);
                    settled.push_back(node.vertex);
                    for (const auto& edge : graph.adj[node.vertex])
                    {
                        const double next = node.distance + edge.w;
                        if (next >= row_distance[edge.to] || next + H(edge.to) > best)
                            continue;
                        if (row_distance[edge.to] == fp::kInf)
                            touched.push_back(edge.to);
                        row_distance[edge.to] = next;
                        queue.push({next + H(edge.to), next, edge.to});
                    }
                }
                std::sort(settled.begin(), settled.end());
                settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
                stats.early_witness_values += settled.size();
                for (int vertex : touched)
                    row_distance[vertex] = fp::kInf;
                for (int vertex : witness_target_vertices)
                    witness_target[vertex] = fp::kInf;

            }
            stats.early_witness_after = best;
            stats.early_upper_after = best;
            stats.early_upper_ms +=
                std::chrono::duration<double, std::milli>(Clock::now() - early_start)
                    .count();
        }
        stats.best_after_ordinary_size[size] = best;
    }
    stats.ordinary_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - ordinary_start).count();
    // A(S,v) covers the anchor and S. Masks are processed offline by size;
    // exactly one merge side is an earlier A row and the other is a D row.
    const auto anchored_start = Clock::now();
    std::vector<OrdinaryRow> anchored(subset_count);
    auto AnchoredAvailable = [&](int mask)
    {
        return !mask || anchored[mask].ready;
    };
    auto ForEachAnchoredValue = [&](int mask, auto&& use)
    {
        if (!mask)
        {
            const auto& values = group_distance[anchor_group];
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                use(vertex, values[vertex]);
            return;
        }
        const OrdinaryRow& row = anchored[mask];
        if (row.dense)
        {
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                if (row.distances[vertex] < fp::kInf)
                    use(vertex, row.distances[vertex]);
            return;
        }
        for (size_t index = 0; index < row.vertices.size(); ++index)
            use(row.vertices[index], row.distances[index]);
    };
    auto ForEachAnchoredSum = [&](int anchor_side, int ordinary_side, auto&& use)
    {
        if (!anchor_side)
        {
            ++stats.anchored_join_direct_calls;
            stats.anchored_join_direct_work +=
                popcount[ordinary_side] == 1
                    ? graph.n
                    : BranchScanWork(ordinary[ordinary_side]);
            const auto& anchor_distance = group_distance[anchor_group];
            ForEachBranchValue(ordinary_side, [&](int vertex, double value)
            {
                use(vertex, anchor_distance[vertex] + value);
            });
            return;
        }
        if (popcount[ordinary_side] == 1)
        {
            ++stats.anchored_join_direct_calls;
            stats.anchored_join_direct_work +=
                ValueScanWork(anchored[anchor_side], graph.n);
            const auto& singleton =
                group_distance[bit_to_group[FirstBit(ordinary_side)]];
            ForEachAnchoredValue(anchor_side, [&](int vertex, double value)
            {
                use(vertex, value + singleton[vertex]);
            });
            return;
        }
        const OrdinaryRow& a = anchored[anchor_side];
        const OrdinaryRow& d = ordinary[ordinary_side];
        if (a.dense)
        {
            ++stats.anchored_join_direct_calls;
            stats.anchored_join_direct_work += BranchScanWork(d);
            ForEachBranchIndex(d, [&](size_t index)
            {
                const int vertex = d.dense ? static_cast<int>(index) : d.vertices[index];
                if (a.distances[vertex] < fp::kInf)
                    use(vertex, a.distances[vertex] + d.distances[index]);
            });
            return;
        }
        if (d.dense)
        {
            const long long search_cost =
                static_cast<long long>(d.branch_count) * BinarySearchCost(a.vertices.size());
            if (search_cost < static_cast<long long>(a.vertices.size()))
            {
                ++stats.anchored_join_binary_calls;
                stats.anchored_join_binary_work += BranchScanWork(d) + search_cost;
                ForEachBranchIndex(d, [&](size_t index)
                {
                    const int vertex = static_cast<int>(index);
                    const auto found =
                        std::lower_bound(a.vertices.begin(), a.vertices.end(), vertex);
                    if (found != a.vertices.end() && *found == vertex)
                    {
                        const size_t a_index =
                            static_cast<size_t>(found - a.vertices.begin());
                        use(vertex, a.distances[a_index] + d.distances[index]);
                    }
                });
            }
            else
            {
                ++stats.anchored_join_direct_calls;
                stats.anchored_join_direct_work += a.vertices.size();
                for (size_t index = 0; index < a.vertices.size(); ++index)
                {
                    const int vertex = a.vertices[index];
                    if (d.IsBranch(vertex))
                        use(vertex, a.distances[index] + d.distances[vertex]);
                }
            }
            return;
        }

        const long long linear_cost =
            static_cast<long long>(a.vertices.size() + d.vertices.size());
        const long long branch_search_cost =
            static_cast<long long>(d.branch_count) * BinarySearchCost(a.vertices.size());
        const long long anchor_search_cost =
            static_cast<long long>(a.vertices.size()) * BinarySearchCost(d.vertices.size());
        if (branch_search_cost < linear_cost && branch_search_cost <= anchor_search_cost)
        {
            ++stats.anchored_join_binary_calls;
            stats.anchored_join_binary_work += BranchScanWork(d) + branch_search_cost;
            ForEachBranchIndex(d, [&](size_t index)
            {
                const int vertex = d.vertices[index];
                const auto found =
                    std::lower_bound(a.vertices.begin(), a.vertices.end(), vertex);
                if (found != a.vertices.end() && *found == vertex)
                {
                    const size_t a_index = static_cast<size_t>(found - a.vertices.begin());
                    use(vertex, a.distances[a_index] + d.distances[index]);
                }
            });
            return;
        }
        if (anchor_search_cost < linear_cost)
        {
            ++stats.anchored_join_binary_calls;
            stats.anchored_join_binary_work += anchor_search_cost;
            for (size_t index = 0; index < a.vertices.size(); ++index)
            {
                const int vertex = a.vertices[index];
                const auto found =
                    std::lower_bound(d.vertices.begin(), d.vertices.end(), vertex);
                if (found == d.vertices.end() || *found != vertex)
                    continue;
                const size_t d_index = static_cast<size_t>(found - d.vertices.begin());
                if (d.IsBranch(d_index))
                    use(vertex, a.distances[index] + d.distances[d_index]);
            }
            return;
        }
        ++stats.anchored_join_linear_calls;
        stats.anchored_join_linear_work += linear_cost;
        size_t a_index = 0;
        size_t d_index = 0;
        while (a_index < a.vertices.size() && d_index < d.vertices.size())
        {
            if (a.vertices[a_index] < d.vertices[d_index])
                ++a_index;
            else if (d.vertices[d_index] < a.vertices[a_index])
                ++d_index;
            else
            {
                if (d.IsBranch(d_index))
                    use(a.vertices[a_index], a.distances[a_index] + d.distances[d_index]);
                ++a_index;
                ++d_index;
            }
        }
    };

    int active_anchored_size = 0;
    auto CompleteRoots = [&](int mask, const std::vector<int>& roots)
    {
        const auto begin = Clock::now();
        ++stats.completion_rows;
        ++stats.completion_rows_by_size[active_anchored_size];
        stats.completion_vertices += roots.size();
        const int remaining = full_mask ^ mask;
        for (int left = remaining;; left = (left - 1) & remaining)
        {
            const int right = remaining ^ left;
            if (left <= right && popcount[left] <= stats.half &&
                popcount[right] <= stats.half &&
                (!left || Available(left)) && (!right || Available(right)))
            {
                const OrdinaryRow* left_row =
                    left && popcount[left] > 1 ? &ordinary[left] : nullptr;
                const OrdinaryRow* right_row =
                    right && popcount[right] > 1 ? &ordinary[right] : nullptr;
                const std::vector<int>* driver = &roots;
                if (left_row && !left_row->dense &&
                    left_row->vertices.size() < driver->size())
                    driver = &left_row->vertices;
                if (right_row && !right_row->dense &&
                    right_row->vertices.size() < driver->size())
                    driver = &right_row->vertices;

                size_t root_index = 0;
                size_t left_index = 0;
                size_t right_index = 0;
                for (int vertex : *driver)
                {
                    ++stats.completion_scan_vertices;
                    ++stats.completion_scan_vertices_by_size[active_anchored_size];
                    while (root_index < roots.size() && roots[root_index] < vertex)
                        ++root_index;
                    if (root_index == roots.size() || roots[root_index] != vertex)
                        continue;

                    double a = 0.0;
                    if (left)
                    {
                        if (popcount[left] == 1)
                            a = group_distance[bit_to_group[FirstBit(left)]][vertex];
                        else if (left_row->dense)
                            a = left_row->distances[vertex];
                        else
                        {
                            while (left_index < left_row->vertices.size() &&
                                   left_row->vertices[left_index] < vertex)
                                ++left_index;
                            if (left_index == left_row->vertices.size() ||
                                left_row->vertices[left_index] != vertex)
                                continue;
                            a = left_row->distances[left_index];
                        }
                    }

                    double b = 0.0;
                    if (right)
                    {
                        if (popcount[right] == 1)
                            b = group_distance[bit_to_group[FirstBit(right)]][vertex];
                        else if (right_row->dense)
                            b = right_row->distances[vertex];
                        else
                        {
                            while (right_index < right_row->vertices.size() &&

                                   right_row->vertices[right_index] < vertex)
                                ++right_index;
                            if (right_index == right_row->vertices.size() ||
                                right_row->vertices[right_index] != vertex)
                                continue;

                            b = right_row->distances[right_index];
                        }
                    }
                    best = std::min(best, row_distance[vertex] + a + b);
                    ++stats.completion_checks;
                    ++stats.completion_checks_by_size[active_anchored_size];
                }
            }
            if (!left)
                break;
        }
        const double elapsed =
            std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        stats.completion_ms += elapsed;
        stats.completion_ms_by_size[active_anchored_size] += elapsed;
    };

    const auto anchored_zero_start = Clock::now();
    ++stats.anchored_masks;
    ++stats.anchored_masks_by_size[0];
    stats.anchored_values += graph.n;
    stats.anchored_values_by_size[0] += graph.n;
    std::vector<int> all_vertices(graph.n);
    std::iota(all_vertices.begin(), all_vertices.end(), 1);
    for (int vertex : all_vertices)
        row_distance[vertex] = group_distance[anchor_group][vertex];
    CompleteRoots(0, all_vertices);
    stats.best_after_anchored_size[0] = best;
    stats.anchored_ms_by_size[0] =
        std::chrono::duration<double, std::milli>(Clock::now() - anchored_zero_start).count();
    for (int vertex : all_vertices)
        row_distance[vertex] = fp::kInf;

    for (int size = 1; size <= stats.half - 1; ++size)
    {
        const auto anchored_layer_start = Clock::now();
        active_anchored_size = size;
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (popcount[mask] != size)
                continue;
            ++stats.anchored_masks;
            ++stats.anchored_masks_by_size[size];
            ++stamp;
            touched.clear();
            settled.clear();
            const int remaining_nonanchor = full_mask ^ mask;
            const int remaining_original = nonanchor_original_mask ^ original_mask[mask];
            auto BaseH = [&](int vertex)
            {
                if (heuristic_stamp[vertex] != stamp)
                {
                    heuristic_stamp[vertex] = stamp;
                    double farthest = 0.0;
                    for (int bits = remaining_nonanchor; bits; bits &= bits - 1)
                        farthest = std::max(
                            farthest,
                            group_distance[bit_to_group[FirstBit(bits & -bits)]][vertex]);
                    const double tour =
                        lower_bound.At(vertex, remaining_original, group_distance);
                    const double cut = dual.At(vertex, remaining_original);
                    heuristic[vertex] = std::max(farthest, std::max(tour, cut));
                    ++stats.anchored_h_evals_by_size[size];
                    if (farthest >= tour && farthest >= cut)
                        ++stats.anchored_h_farthest_by_size[size];
                    else if (tour >= cut)
                        ++stats.anchored_h_tour_by_size[size];
                    else
                        ++stats.anchored_h_dual_by_size[size];
                }
                return heuristic[vertex];
            };
            auto Set = [&](int vertex, double value)
            {
                ++stats.anchored_seed_candidates_by_size[size];
                if (value >= row_distance[vertex])
                {
                    ++stats.anchored_seed_reject_old_by_size[size];
                    return;
                }
                if (value + BaseH(vertex) > best)
                {
                    ++stats.anchored_seed_reject_bound_by_size[size];
                    return;
                }
                ++stats.anchored_seed_accept_by_size[size];
                if (row_distance[vertex] == fp::kInf)
                    touched.push_back(vertex);
                row_distance[vertex] = value;
            };

            for (int ordinary_side = mask;
                 ordinary_side;
                 ordinary_side = (ordinary_side - 1) & mask)
            {
                const int anchor_side = mask ^ ordinary_side;
                if (!Available(ordinary_side) || !AnchoredAvailable(anchor_side))
                    continue;
                ForEachAnchoredSum(anchor_side, ordinary_side, [&](int vertex, double value)
                {
                    ++stats.anchored_merge_probes;
                    ++stats.anchored_merge_probes_by_size[size];
                    Set(vertex, value);
                });
            }

            // No admissible A value survived for this mask, so neither this row nor
            // any extension through it can improve the incumbent.
            if (touched.empty())
                continue;

            std::priority_queue<RowHeapNode,
                                std::vector<RowHeapNode>,
                                std::greater<RowHeapNode>> queue;
            for (int vertex : touched)
                queue.push(
                    {row_distance[vertex] + BaseH(vertex), row_distance[vertex], vertex});
            stats.anchored_touched_values += touched.size();
            stats.anchored_touched_by_size[size] += touched.size();
            stats.anchored_peak_queue =
                std::max(stats.anchored_peak_queue, static_cast<long long>(queue.size()));

            while (!queue.empty())
            {
                const RowHeapNode node = queue.top();
                queue.pop();
                ++stats.anchored_queue_pops;
                ++stats.anchored_pops_by_size[size];
                if (node.distance != row_distance[node.vertex])
                {
                    ++stats.anchored_pop_stale_by_size[size];
                    continue;
                }
                if (node.key > best)
                {
                    ++stats.anchored_pop_bound_by_size[size];
                    continue;
                }
                double star_extension = node.distance;
                for (int bits = remaining_nonanchor; bits; bits &= bits - 1)
                    star_extension +=

                        group_distance[bit_to_group[FirstBit(bits & -bits)]][node.vertex];
                best = std::min(best, star_extension);
                settled.push_back(node.vertex);
                for (const auto& edge : graph.adj[node.vertex])
                {
                    const double next = node.distance + edge.w;
                    ++stats.anchored_relax_attempts_by_size[size];
                    if (next >= row_distance[edge.to])
                    {
                        ++stats.anchored_relax_reject_old_by_size[size];
                        continue;
                    }
                    if (next + BaseH(edge.to) > best)
                    {
                        ++stats.anchored_relax_reject_bound_by_size[size];
                        continue;
                    }
                    ++stats.anchored_relax_accept_by_size[size];
                    if (row_distance[edge.to] == fp::kInf)
                    {
                        touched.push_back(edge.to);
                        ++stats.anchored_touched_values;
                        ++stats.anchored_touched_by_size[size];
                    }
                    row_distance[edge.to] = next;
                    queue.push({next + BaseH(edge.to), next, edge.to});
                    stats.anchored_peak_queue =
                        std::max(stats.anchored_peak_queue,
                                 static_cast<long long>(queue.size()));
                }
            }

            std::sort(settled.begin(), settled.end());
            settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
            CompleteRoots(mask, settled);
            settled.erase(
                std::remove_if(settled.begin(), settled.end(), [&](int vertex)
                {
                    return row_distance[vertex] + BaseH(vertex) > best;
                }),
                settled.end());
            stats.anchored_settled_values += settled.size();
            stats.anchored_values += settled.size();
            stats.anchored_settled_by_size[size] += settled.size();
            stats.anchored_values_by_size[size] += settled.size();
            if (size < stats.half - 1)
            {
                OrdinaryRow& row = anchored[mask];
                const size_t sparse_bytes = settled.size() * (sizeof(int) + sizeof(double));
                const size_t dense_bytes = static_cast<size_t>(graph.n + 1) * sizeof(double);
                row.dense = dense_bytes < sparse_bytes;
                if (row.dense)
                {
                    row.distances.assign(graph.n + 1, fp::kInf);
                    for (int vertex : settled)
                        row.distances[vertex] = row_distance[vertex];
                }
                else
                {
                    row.vertices = settled;
                    row.distances.reserve(settled.size());
                    for (int vertex : settled)
                        row.distances.push_back(row_distance[vertex]);
                }
                row.ready = true;
                if (row.dense)
                    ++stats.anchored_dense_rows_by_size[size];
                else
                    ++stats.anchored_sparse_rows_by_size[size];
                stats.anchored_row_bytes_by_size[size] += RowStorageBytes(row);
            }
            for (int vertex : touched)
                row_distance[vertex] = fp::kInf;
        }
        stats.anchored_ms_by_size[size] =
            std::chrono::duration<double, std::milli>(Clock::now() - anchored_layer_start)
                .count();
        stats.best_after_anchored_size[size] = best;
    }
    stats.anchored_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - anchored_start).count();

    stats.total_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - total_start).count();
    result.best_weight = best;
    result.feasible = best < fp::kInf / 4;
    return result;
}


}  // namespace gst::methods::test80_anchor_progressive
