#include "test80_anchor_progressive.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef GST_TEST80_PROGRESS
#include <iostream>
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include "../../float_compare.h"
#include "../../query_feasibility.h"
#include "../Common/anchor_junction_upper.h"
#include "../Common/dual_cut_potential.h"

#if defined(_MSC_VER)
#define GST_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define GST_NOINLINE __attribute__((noinline))
#else
#define GST_NOINLINE
#endif

#ifndef GST_TEST157_ABLATION_STAGE
#define GST_TEST157_ABLATION_STAGE 0
#endif

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
    std::vector<std::uint64_t> vertex_bits;
    std::vector<std::uint32_t> rank_before_word;
    std::vector<std::uint64_t> branch_bits;
    size_t branch_count = 0;
    bool ready = false;
    bool dense = false;
    bool bitmap = false;

    bool IsBranchIndex(size_t index) const
    {
        return (branch_bits[index >> 6] >> (index & 63)) & 1ULL;
    }

    bool IsBranchVertex(int vertex) const
    {
        return (branch_bits[static_cast<size_t>(vertex) >> 6] >> (vertex & 63)) & 1ULL;
    }
};

long long RowStorageBytes(const OrdinaryRow& row)
{
    return static_cast<long long>(row.vertices.capacity()) * sizeof(int) +
           static_cast<long long>(row.distances.capacity()) * sizeof(double) +
           static_cast<long long>(row.vertex_bits.capacity()) * sizeof(std::uint64_t) +
           static_cast<long long>(row.rank_before_word.capacity()) * sizeof(std::uint32_t) +
           static_cast<long long>(row.branch_bits.capacity()) * sizeof(std::uint64_t);
}

long long ValueScanWork(const OrdinaryRow& row, int n)
{
    if (row.dense)
        return n;
    if (row.bitmap)
        return static_cast<long long>(row.vertex_bits.size() + row.distances.size());
    return static_cast<long long>(row.vertices.size());
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

int Popcount64(std::uint64_t bits)
{
#ifdef _MSC_VER
    return static_cast<int>(__popcnt64(bits));
#else
    return __builtin_popcountll(bits);
#endif
}

#ifdef GST_TEST157_VALIDATE_ROWS
[[noreturn]] void RowInvariantFailure(const char* role, const char* detail)
{
    throw std::runtime_error(
        std::string("Test157 ") + role + " row invariant failed: " + detail);
}

void ValidateStoredRow(const OrdinaryRow& row,
                       int n,
                       bool carries_branches,
                       const char* role)
{
    if (!row.ready)
        RowInvariantFailure(role, "row is not marked ready");
    if (row.dense && row.bitmap)
        RowInvariantFailure(role, "dense and bitmap modes overlap");

    const size_t vertex_domain = static_cast<size_t>(n) + 1;
    const size_t word_count = (vertex_domain + 63) / 64;
    auto CheckTail = [&](const std::vector<std::uint64_t>& words, size_t domain)
    {
        if (words.size() != (domain + 63) / 64)
            RowInvariantFailure(role, "bit-vector length does not match its domain");
        if (!words.empty() && (domain & 63))
        {
            const std::uint64_t valid = (std::uint64_t{1} << (domain & 63)) - 1;
            if (words.back() & ~valid)
                RowInvariantFailure(role, "bit-vector has nonzero tail bits");
        }
    };
    auto CheckValue = [&](double value, bool represented)
    {
        if (!std::isfinite(value))
            RowInvariantFailure(role, "row contains a non-finite value");
        if (represented && (value >= fp::kInf || value < -fp::kEps))
            RowInvariantFailure(role, "represented value is outside the cost domain");
    };

    if (row.dense)
    {
        if (!row.vertices.empty() || !row.vertex_bits.empty() ||
            !row.rank_before_word.empty() || row.distances.size() != vertex_domain)
            RowInvariantFailure(role, "invalid dense payload");
        for (double value : row.distances)
            CheckValue(value, value < fp::kInf);
    }
    else if (row.bitmap)
    {
        if (!row.vertices.empty() || row.vertex_bits.size() != word_count ||
            row.rank_before_word.size() != word_count)
            RowInvariantFailure(role, "invalid ranked-bitmap payload");
        CheckTail(row.vertex_bits, vertex_domain);
        if (!row.vertex_bits.empty() && (row.vertex_bits[0] & 1ULL))
            RowInvariantFailure(role, "vertex zero is present");
        std::uint64_t rank = 0;
        for (size_t word = 0; word < word_count; ++word)
        {
            if (row.rank_before_word[word] != rank)
                RowInvariantFailure(role, "rank prefix is inconsistent");
            rank += static_cast<std::uint64_t>(Popcount64(row.vertex_bits[word]));
        }
        if (rank != row.distances.size())
            RowInvariantFailure(role, "bitmap population differs from value count");
        for (double value : row.distances)
            CheckValue(value, true);
    }
    else
    {
        if (!row.vertex_bits.empty() || !row.rank_before_word.empty() ||
            row.vertices.size() != row.distances.size())
            RowInvariantFailure(role, "invalid sparse payload");
        for (size_t index = 0; index < row.vertices.size(); ++index)
        {
            if (row.vertices[index] < 1 || row.vertices[index] > n ||
                (index && row.vertices[index - 1] >= row.vertices[index]))
                RowInvariantFailure(role, "sparse vertices are not unique and sorted");
            CheckValue(row.distances[index], true);
        }
    }

    if (!carries_branches)
    {
        if (row.branch_count || !row.branch_bits.empty())
            RowInvariantFailure(role, "branch payload exists on a value-only row");
        return;
    }

    const size_t branch_domain =
        row.dense || row.bitmap ? vertex_domain : row.vertices.size();
    CheckTail(row.branch_bits, branch_domain);
    size_t branches = 0;
    for (size_t word = 0; word < row.branch_bits.size(); ++word)
    {
        std::uint64_t bits = row.branch_bits[word];
        branches += static_cast<size_t>(Popcount64(bits));
        while (bits)
        {
            const int bit = FirstBit64(bits);
            const size_t index = (word << 6) + static_cast<size_t>(bit);
            if (row.dense && row.distances[index] >= fp::kInf)
                RowInvariantFailure(role, "dense branch is not a stored value");
            if (row.bitmap &&
                !(row.vertex_bits[word] & (std::uint64_t{1} << bit)))
                RowInvariantFailure(role, "bitmap branch is not a stored value");
            if (!row.dense && !row.bitmap && index >= row.vertices.size())
                RowInvariantFailure(role, "sparse branch index is outside the row");
            bits &= bits - 1;
        }
    }
    if (branches != row.branch_count)
        RowInvariantFailure(role, "branch population differs from branch_count");
}
#endif

double BitmapValue(const OrdinaryRow& row, int vertex)
{
    const size_t word = static_cast<size_t>(vertex) >> 6;
    const int offset = vertex & 63;
    const std::uint64_t bits = row.vertex_bits[word];
    const std::uint64_t bit = 1ULL << offset;
    if (!(bits & bit))
        return fp::kInf;
    const std::uint64_t lower = offset ? bits & (bit - 1) : 0;
    const size_t index = row.rank_before_word[word] + Popcount64(lower);
    return row.distances[index];
}

template <typename Use>
GST_NOINLINE long long ForEachBitmapBranchPair(const OrdinaryRow& values,
                                               const OrdinaryRow& branches,
                                               Use&& use)
{
    long long work = static_cast<long long>(values.vertex_bits.size());
    for (size_t word = 0; word < values.vertex_bits.size(); ++word)
    {
        std::uint64_t bits = values.vertex_bits[word] & branches.branch_bits[word];
        while (bits)
        {
            const int bit = FirstBit64(bits);
            const int vertex = static_cast<int>((word << 6) + bit);
            const std::uint64_t lower_mask = bit ? (1ULL << bit) - 1 : 0;
            const size_t value_index =
                values.rank_before_word[word] +
                Popcount64(values.vertex_bits[word] & lower_mask);
            const double branch_value =
                branches.bitmap
                    ? branches.distances[
                          branches.rank_before_word[word] +
                          Popcount64(branches.vertex_bits[word] & lower_mask)]
                    : branches.distances[vertex];
            use(vertex, values.distances[value_index], branch_value);
            ++work;
            bits &= bits - 1;
        }
    }
    return work;
}

struct CompletionBitmapResult
{
    double best = fp::kInf;
    long long visits = 0;
    long long checks = 0;
    long long root_rejects = 0;
    long long component_rejects = 0;
};

GST_NOINLINE CompletionBitmapResult CompleteBitmapIntersection(
    const std::vector<std::uint64_t>& root_bits,
    const OrdinaryRow* left,
    const OrdinaryRow* right,
    const DenseRow* left_singleton,
    const DenseRow* right_singleton,
    const DenseRow& anchored_values,
    double left_minimum,
    double right_minimum,
    double best)
{
    CompletionBitmapResult result{best, 0, 0, 0, 0};
    for (size_t word = 0; word < root_bits.size(); ++word)
    {
        std::uint64_t bits = root_bits[word];
        if (left)
            bits &= left->vertex_bits[word];
        if (right)
            bits &= right->vertex_bits[word];
        while (bits)
        {
            const int bit = FirstBit64(bits);
            const int vertex = static_cast<int>((word << 6) + bit);
            ++result.visits;
            if (anchored_values[vertex] + left_minimum + right_minimum > result.best)
            {
                ++result.root_rejects;
                bits &= bits - 1;
                continue;
            }
            const std::uint64_t lower_mask = bit ? (1ULL << bit) - 1 : 0;
            const size_t left_index =
                left ? left->rank_before_word[word] +
                           Popcount64(left->vertex_bits[word] & lower_mask)
                     : 0;
            const double left_value =
                left ? left->distances[left_index]
                     : left_singleton ? (*left_singleton)[vertex] : 0.0;
            if (right && (left || left_singleton) &&
                anchored_values[vertex] + left_value + right_minimum > result.best)
            {
                ++result.component_rejects;
                bits &= bits - 1;
                continue;
            }
            const size_t right_index =
                right ? right->rank_before_word[word] +
                            Popcount64(right->vertex_bits[word] & lower_mask)
                      : 0;
            const double right_value =
                right ? right->distances[right_index]
                      : right_singleton ? (*right_singleton)[vertex] : 0.0;
            result.best = std::min(result.best,
                                   anchored_values[vertex] + left_value + right_value);
            ++result.checks;
            bits &= bits - 1;
        }
    }
    return result;
}

double RowValueAt(const OrdinaryRow& row, int vertex)
{
    if (row.dense)
        return row.distances[vertex];
    if (row.bitmap)
        return BitmapValue(row, vertex);
    const auto found = std::lower_bound(row.vertices.begin(), row.vertices.end(), vertex);
    if (found == row.vertices.end() || *found != vertex)
        return fp::kInf;
    return row.distances[static_cast<size_t>(found - row.vertices.begin())];
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

struct AnchorTreeFacilityValue
{
    double upper = fp::kInf;
    long long probes = 0;
};

AnchorTreeFacilityValue ComputeAnchorTreeFacility(
    const gst::methods::anchor_junction::AnchorTree& tree,
    int subset_count,
    const std::vector<int>& popcount,
    const std::vector<int>& bit_to_group,
    const std::vector<std::vector<double>>& group_distance,
    const std::vector<OrdinaryRow>& ordinary,
    double anchor_path_cost)
{
    struct Child
    {
        int id = 0;
        double length = 0.0;
    };

    AnchorTreeFacilityValue result;
    const int root = static_cast<int>(tree.vertices.size());
    std::vector<std::vector<Child>> children(root + 1);
    for (int node = 0; node < root; ++node)
        children[tree.parent[node]].push_back(
            {node, tree.parent_edge[node]});
    std::vector<int> order{root};
    for (size_t index = 0; index < order.size(); ++index)
        for (const Child& child : children[order[index]])
            order.push_back(child.id);

    std::vector<std::vector<double>> dp(
        root + 1,
        std::vector<double>(subset_count, fp::kInf));
    std::vector<double> block_cost(subset_count, fp::kInf);
    std::vector<double> local(subset_count, fp::kInf);
    std::vector<double> merged(subset_count, fp::kInf);
    for (auto it = order.rbegin(); it != order.rend(); ++it)
    {
        const int node = *it;
        dp[node][0] = 0.0;
        if (node != root)
        {
            const int vertex = tree.vertices[node];
            std::fill(block_cost.begin(), block_cost.end(), fp::kInf);
            for (int mask = 1; mask < subset_count; ++mask)
            {
                if (popcount[mask] == 1)
                {
                    block_cost[mask] =
                        group_distance[bit_to_group[FirstBit(mask)]][vertex];
                }
                else if (ordinary[mask].ready)
                {
                    block_cost[mask] = RowValueAt(ordinary[mask], vertex);
                }
            }

            std::fill(local.begin(), local.end(), fp::kInf);
            local[0] = 0.0;
            for (int remaining = 1; remaining < subset_count; ++remaining)
            {
                const int first = remaining & -remaining;
                for (int block = remaining; block;
                     block = (block - 1) & remaining)
                {
                    if (!(block & first))
                        continue;
                    ++result.probes;
                    if (block_cost[block] >= fp::kInf ||
                        local[remaining ^ block] >= fp::kInf)
                        continue;
                    local[remaining] = std::min(
                        local[remaining],
                        local[remaining ^ block] + block_cost[block]);
                }
            }
            dp[node] = local;
        }

        for (const Child& child : children[node])
        {
            std::fill(merged.begin(), merged.end(), fp::kInf);
            for (int mask = 0; mask < subset_count; ++mask)
                for (int below = mask;; below = (below - 1) & mask)
                {
                    ++result.probes;
                    if (dp[node][mask ^ below] < fp::kInf &&
                        dp[child.id][below] < fp::kInf)
                    {
                        merged[mask] = std::min(
                            merged[mask],
                            dp[node][mask ^ below] + dp[child.id][below] +
                                (below ? child.length : 0.0));
                    }
                    if (!below)
                        break;
                }
            dp[node].swap(merged);
        }
    }
    result.upper = anchor_path_cost + dp[root][subset_count - 1];
    return result;
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
#ifdef GST_TEST157_IWATA_M0
    constexpr bool kIwataM0 = true;
#else
    constexpr bool kIwataM0 = false;
#endif
    SolveResult result;
    Test80Stats& stats = result.stats;
    stats.n = graph.n;
    stats.m = graph.m;
    stats.g = static_cast<int>(query.groups.size());
    stats.ablation_stage = GST_TEST157_ABLATION_STAGE;

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
    stats.anchor_group = kIwataM0 ? 0 : anchor_group + 1;
    stats.anchor_group_size =
        kIwataM0 ? 0 : static_cast<int>(query.groups[anchor_group].size());
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

    // M0 keeps every group in the ordinary mask; anchored stages remove the
    // permanent anchor and use the same ordinary-row engine on the remainder.
    std::vector<int> bit_to_group;
    bit_to_group.reserve(stats.g);
    for (int group = 0; group < stats.g; ++group)
        if (kIwataM0 || group != anchor_group)
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

    gst::methods::dual_cut::DualCutPotential dual;
    stats.dual_primal_upper = fp::kInf;
#ifdef GST_TEST80_NO_DUAL_CUT
    constexpr bool dual_available = false;
#else
    constexpr bool dual_available = true;
#endif
    bool dual_built = false;
    bool dual_active = false;
    stats.dual_buy_work =
        static_cast<long long>(stats.g) * (2LL * graph.m + graph.n);
    stats.dual_step_buy_work = 2LL * graph.m + graph.n;
    auto DualAt = [&](int vertex, int mask)
    {
        return dual_active ? dual.At(vertex, mask) : 0.0;
    };
    auto DualGroupAt = [&](int vertex, int group)
    {
        return dual_active ? dual.GroupAt(vertex, group) : 0.0;
    };
    auto BuildDual = [&]()
    {
        if (!dual_available || dual_built)
            return;
        const auto dual_start = Clock::now();
#ifdef GST_DUAL_CHANGED_ARC_SEEDS
        dual.BuildKeepingResidualChangedArcs(
            graph, query, group_distance, root_star_root);
#else
        dual.BuildKeepingResidual(graph, query, group_distance, root_star_root);
#endif
        dual_built = true;
        dual_active = true;
        stats.dual_built = 1;
        stats.dual_groups_built = stats.g;
        stats.dual_primal_upper = dual.PrimalUpper();
        best = std::min(best, stats.dual_primal_upper);
        stats.root_dual_lower = dual.At(root_star_root, original_full_mask);
        stats.dual_ms +=
            std::chrono::duration<double, std::milli>(Clock::now() - dual_start)
                .count();
        stats.dual_seed_arc_scans = dual.SeedArcScans();
        stats.dual_full_seed_arc_scans = dual.FullSeedArcScans();
        stats.dual_changed_arcs = dual.ChangedArcs();
    };
#ifdef GST_TEST80_PROGRESSIVE_DUAL
    bool progressive_dual_started = false;
    auto AdvanceDual = [&]()
    {
        const auto dual_start = Clock::now();
        if (!progressive_dual_started)
        {
            dual.BeginProgressiveChangedArcs(
                graph, query, group_distance, root_star_root);
            progressive_dual_started = true;
        }
        if (!dual.AdvanceProgressiveChangedArcs(graph))
            return;
        dual_active = true;
        stats.dual_groups_built = dual.ProgressiveGroupsBuilt();
        stats.root_dual_lower = dual.At(root_star_root, original_full_mask);
        if (dual.ProgressiveComplete())
        {
            dual.RecoverProgressivePrimal(graph, query, root_star_root);
            dual_built = true;
            stats.dual_built = 1;
            stats.dual_primal_upper = dual.PrimalUpper();
            best = std::min(best, stats.dual_primal_upper);
        }
        stats.dual_ms +=
            std::chrono::duration<double, std::milli>(Clock::now() - dual_start)
                .count();
        stats.dual_seed_arc_scans = dual.SeedArcScans();
        stats.dual_full_seed_arc_scans = dual.FullSeedArcScans();
        stats.dual_changed_arcs = dual.ChangedArcs();
    };
#endif
#if !defined(GST_TEST80_NO_DUAL_CUT) && !defined(GST_TEST80_PROGRESSIVE_DUAL)
    BuildDual();
#endif
    stats.root_tour_lower = lower_bound.At(root_star_root, original_full_mask, group_distance);
    stats.root_dual_lower = DualAt(root_star_root, original_full_mask);

    stats.junction_before = best;
    const auto junction_start = Clock::now();
#if defined(GST_TEST80_ANCHOR_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_FACILITY) || \
    defined(GST_TEST80_ANCHOR_TREE_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_TREE_FACILITY)
    std::vector<double> anchor_path_distance;
#if defined(GST_TEST80_ANCHOR_TREE_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_TREE_FACILITY)
    gst::methods::anchor_junction::AnchorTree anchor_tree;
    const auto junction = gst::methods::anchor_junction::BuildUpper(
        graph,
        query,
        group_distance,
        root_star_root,
        anchor_group,
        &anchor_path_distance,
        &anchor_tree);
#else
    const auto junction = gst::methods::anchor_junction::BuildUpper(
        graph,
        query,
        group_distance,
        root_star_root,
        anchor_group,
        &anchor_path_distance);
#endif
#else
    const auto junction = gst::methods::anchor_junction::BuildUpper(
        graph, query, group_distance, root_star_root, anchor_group);
#endif
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
                        DualAt(vertex, remaining_original));
    };
    std::vector<OrdinaryRow> ordinary(subset_count);
    std::vector<double> ordinary_minimum(subset_count, fp::kInf);
#if defined(GST_TEST80_ANCHOR_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_FACILITY)
    std::vector<double> anchor_attachment(subset_count, fp::kInf);
    std::vector<double> anchor_facility(subset_count, fp::kInf);
    for (int bit = 0; bit < nonanchor_count; ++bit)
    {
        const int mask = 1 << bit;
        const auto& distance = group_distance[bit_to_group[bit]];
        for (int vertex = 1; vertex <= graph.n; ++vertex)
        {
            anchor_attachment[mask] = std::min(
                anchor_attachment[mask],
                distance[vertex] + anchor_path_distance[vertex]);
        }
    }
    auto EvaluateAnchorFacility = [&](int size)
    {
        const auto facility_start = Clock::now();
        long long facility_probes = 0;
        std::fill(anchor_facility.begin(), anchor_facility.end(), fp::kInf);
        anchor_facility[0] = 0.0;
        for (int remaining = 1; remaining < subset_count; ++remaining)
        {
            const int first = remaining & -remaining;
            for (int block = remaining; block; block = (block - 1) & remaining)
            {
                if (!(block & first) || anchor_attachment[block] >= fp::kInf ||
                    anchor_facility[remaining ^ block] >= fp::kInf)
                    continue;
                ++facility_probes;
                anchor_facility[remaining] = std::min(
                    anchor_facility[remaining],
                    anchor_facility[remaining ^ block] + anchor_attachment[block]);
            }
        }
        const double facility_upper =
            group_distance[anchor_group][root_star_root] +
            anchor_facility[full_mask];
        stats.anchor_facility_upper_by_size[size] = facility_upper;
        stats.anchor_facility_probes_by_size[size] = facility_probes;
        stats.anchor_facility_ms_by_size[size] =
            std::chrono::duration<double, std::milli>(Clock::now() - facility_start)
                .count();
#ifdef GST_TEST80_ANCHOR_FACILITY
        best = std::min(best, facility_upper);
#endif
    };
#endif
    auto Available = [&](int mask)
    {
        return mask && (popcount[mask] == 1 || ordinary[mask].ready);
    };
#if defined(GST_TEST80_ANCHOR_TREE_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_TREE_FACILITY)
#ifdef GST_TEST80_ANCHOR_TREE_AMORTIZED
    long long anchor_tree_rent = 0;
    long long tree_merge_probes = 1;
    for (int bit = 0; bit < nonanchor_count; ++bit)
        tree_merge_probes *= 3;
    const long long tree_local_probes = (tree_merge_probes - 1) / 2;
    stats.anchor_tree_buy_work =
        static_cast<long long>(anchor_tree.vertices.size()) *
        (tree_local_probes + tree_merge_probes);
    stats.anchor_tree_upper_by_size.fill(fp::kInf);
#endif
    auto EvaluateAnchorTreeFacility = [&](int size)
    {
        const auto tree_start = Clock::now();
        const AnchorTreeFacilityValue value = ComputeAnchorTreeFacility(
            anchor_tree,
            subset_count,
            popcount,
            bit_to_group,
            group_distance,
            ordinary,
            group_distance[anchor_group][root_star_root]);
        stats.anchor_tree_upper_by_size[size] = value.upper;
        stats.anchor_tree_probes_by_size[size] = value.probes;
        stats.anchor_tree_vertices_by_size[size] =
            static_cast<int>(anchor_tree.vertices.size());
        stats.anchor_tree_evaluated_by_size[size] = 1;
        ++stats.anchor_tree_evaluations;
        stats.anchor_tree_ms_by_size[size] =
            std::chrono::duration<double, std::milli>(Clock::now() - tree_start)
                .count();
#ifdef GST_TEST80_ANCHOR_TREE_FACILITY
        best = std::min(best, stats.anchor_tree_upper_by_size[size]);
#endif
    };
#endif
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
        if (row.bitmap)
        {
            size_t index = 0;
            for (size_t word = 0; word < row.vertex_bits.size(); ++word)
            {
                std::uint64_t bits = row.vertex_bits[word];
                while (bits)
                {
                    const int bit = FirstBit64(bits);
                    use(static_cast<int>((word << 6) + bit), row.distances[index++]);
                    bits &= bits - 1;
                }
            }
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
            if (row.dense)
            {
                use(static_cast<int>(index), row.distances[index]);
                return;
            }
            if (row.bitmap)
            {
                const int vertex = static_cast<int>(index);
                use(vertex, BitmapValue(row, vertex));
                return;
            }
            use(row.vertices[index], row.distances[index]);
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
        if (a.bitmap && b.bitmap)
        {
            for (size_t word = 0; word < a.vertex_bits.size(); ++word)
            {
                std::uint64_t bits = a.vertex_bits[word] & b.vertex_bits[word];
                while (bits)
                {
                    const int bit = FirstBit64(bits);
                    const int vertex = static_cast<int>((word << 6) + bit);
                    use(vertex, BitmapValue(a, vertex), BitmapValue(b, vertex));
                    bits &= bits - 1;
                }
            }
            return;
        }
        if (a.bitmap)
        {
            if (b.dense)
            {
                ForEachValue(left, [&](int vertex, double value)
                {
                    if (b.distances[vertex] < fp::kInf)
                        use(vertex, value, b.distances[vertex]);
                });
            }
            else
            {
                for (size_t index = 0; index < b.vertices.size(); ++index)
                {
                    const int vertex = b.vertices[index];
                    const double value = BitmapValue(a, vertex);
                    if (value < fp::kInf)
                        use(vertex, value, b.distances[index]);
                }
            }
            return;
        }
        if (b.bitmap)
        {
            if (a.dense)
            {
                ForEachValue(right, [&](int vertex, double value)
                {
                    if (a.distances[vertex] < fp::kInf)
                        use(vertex, a.distances[vertex], value);
                });
            }
            else
            {
                for (size_t index = 0; index < a.vertices.size(); ++index)
                {
                    const int vertex = a.vertices[index];
                    const double value = BitmapValue(b, vertex);
                    if (value < fp::kInf)
                        use(vertex, a.distances[index], value);
                }
            }
            return;
        }
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
        if (a.bitmap && (b.dense || b.bitmap))
        {
            ++stats.ordinary_join_direct_calls;
            stats.ordinary_join_direct_work +=
                ForEachBitmapBranchPair(a, b, use);
            return;
        }
        if (a.dense || a.bitmap)
        {
            ++stats.ordinary_join_direct_calls;
            stats.ordinary_join_direct_work += BranchScanWork(b);
            ForEachBranchIndex(b, [&](size_t index)
            {
                const int vertex = (b.dense || b.bitmap)
                                       ? static_cast<int>(index)
                                       : b.vertices[index];
                const double a_value = RowValueAt(a, vertex);
                const double b_value = b.bitmap ? BitmapValue(b, vertex)
                                                : b.distances[index];
                if (a_value < fp::kInf)
                    use(vertex, a_value, b_value);
            });
            return;
        }
        if (b.dense || b.bitmap)
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
                        const double b_value =
                            b.bitmap ? BitmapValue(b, vertex) : b.distances[index];
                        use(vertex, a.distances[a_index], b_value);
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
                    if (b.IsBranchVertex(vertex))
                    {
                        const double b_value =
                            b.bitmap ? BitmapValue(b, vertex) : b.distances[vertex];
                        use(vertex, a.distances[index], b_value);
                    }
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
                if (b.IsBranchIndex(b_index))
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
                if (b.IsBranchIndex(b_index))
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
    std::vector<unsigned char> heuristic_state(graph.n + 1);
    constexpr unsigned char kCutReady = 1;
    constexpr unsigned char kFarthestSource = 2;
    constexpr unsigned char kDualSource = 3;
    constexpr unsigned char kTourReady = 4;
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
        const int size = popcount[mask];
        double minimum = fp::kInf;
#if defined(GST_TEST80_ANCHOR_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_FACILITY)
        double attachment = fp::kInf;
        auto UpdateAttachment = [&](int vertex, double tree_cost)
        {
            const double candidate =
                tree_cost + anchor_path_distance[vertex];
            attachment = std::min(attachment, candidate);
        };
#endif
        const size_t word_count = (static_cast<size_t>(graph.n + 1) + 63) / 64;
        const size_t sparse_bytes =
            vertices.size() * (sizeof(int) + sizeof(double)) +
            ((vertices.size() + 63) / 64) * sizeof(std::uint64_t);
        const size_t dense_bytes =
            static_cast<size_t>(graph.n + 1) * sizeof(double) +
            word_count * sizeof(std::uint64_t);
        const size_t bitmap_bytes =
            vertices.size() * sizeof(double) +
            word_count *
                (2 * sizeof(std::uint64_t) + sizeof(std::uint32_t));
        row.bitmap = bitmap_bytes < sparse_bytes && bitmap_bytes < dense_bytes;
        row.dense = !row.bitmap && dense_bytes < sparse_bytes;
        if (row.dense)
        {
            row.distances.assign(graph.n + 1, fp::kInf);
            for (int vertex : vertices)
            {
                row.distances[vertex] = row_distance[vertex];
                minimum = std::min(minimum, row_distance[vertex]);
#if defined(GST_TEST80_ANCHOR_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_FACILITY)
                UpdateAttachment(vertex, row_distance[vertex]);
#endif
            }
        }
        else if (row.bitmap)
        {
            row.vertex_bits.assign(word_count, 0);
            row.distances.reserve(vertices.size());
            for (int vertex : vertices)
            {
                row.vertex_bits[static_cast<size_t>(vertex) >> 6] |=
                    1ULL << (vertex & 63);
                row.distances.push_back(row_distance[vertex]);
                minimum = std::min(minimum, row_distance[vertex]);
#if defined(GST_TEST80_ANCHOR_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_FACILITY)
                UpdateAttachment(vertex, row_distance[vertex]);
#endif
            }
            row.rank_before_word.resize(word_count);
            std::uint32_t rank = 0;
            for (size_t word = 0; word < word_count; ++word)
            {
                row.rank_before_word[word] = rank;
                rank += static_cast<std::uint32_t>(Popcount64(row.vertex_bits[word]));
            }
        }
        else
        {
            row.vertices = vertices;
            row.distances.reserve(vertices.size());
            for (int vertex : vertices)
            {
                row.distances.push_back(row_distance[vertex]);
                minimum = std::min(minimum, row_distance[vertex]);
#if defined(GST_TEST80_ANCHOR_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_FACILITY)
                UpdateAttachment(vertex, row_distance[vertex]);
#endif
            }
        }
#if defined(GST_TEST80_ANCHOR_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_FACILITY)
        anchor_attachment[mask] = attachment;
#endif
        ordinary_minimum[mask] = minimum;
        const size_t branch_domain =
            (row.dense || row.bitmap) ? static_cast<size_t>(graph.n + 1)
                                      : vertices.size();
        row.branch_count = branch_vertices.size();
        row.branch_bits.assign((branch_domain + 63) / 64, 0);
        if (row.dense || row.bitmap)
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
#ifdef GST_TEST157_VALIDATE_ROWS
        ValidateStoredRow(row, graph.n, true, "ordinary");
#endif
        if (row.dense)
            ++stats.ordinary_dense_rows_by_size[size];
        else if (row.bitmap)
            ++stats.ordinary_bitmap_rows_by_size[size];
        else
            ++stats.ordinary_sparse_rows_by_size[size];
        stats.ordinary_row_bytes_by_size[size] += RowStorageBytes(row);
    };
    auto ForEachTripleValue = [&](int first, int second, int third, auto&& use)
    {
        const int masks[3] = {first, second, third};
        const OrdinaryRow* rows[3] = {nullptr, nullptr, nullptr};
        int driver_mask = 0;
        size_t driver_size = static_cast<size_t>(graph.n) + 1;
        for (int index = 0; index < 3; ++index)
        {
            const int mask = masks[index];
            if (mask && popcount[mask] > 1)
            {
                rows[index] = &ordinary[mask];
                const size_t size = rows[index]->dense
                                        ? static_cast<size_t>(graph.n)
                                        : rows[index]->distances.size();
                if (size < driver_size)
                {
                    driver_mask = mask;
                    driver_size = size;
                }
            }
        }

        size_t positions[3] = {0, 0, 0};
        auto Visit = [&](int vertex)
        {
            if (!kIwataM0)
                ++stats.early_upper_probes;
            double total = kIwataM0 ? 0.0 : group_distance[anchor_group][vertex];
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
                if (row.bitmap)
                {
                    const double value = BitmapValue(row, vertex);
                    if (value >= fp::kInf)
                        return;
                    total += value;
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

        if (driver_mask)
            ForEachValue(driver_mask, [&](int vertex, double) { Visit(vertex); });
        else
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                Visit(vertex);
    };
    const int early_block_limit = (nonanchor_count + 2) / 3;
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
    bool packing_built = !dual_available;
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
    const double ordinary_dual_before = stats.dual_ms;
#ifdef GST_TEST80_PROGRESSIVE_DUAL
    long long dual_rent_rows = 0;
    long long dual_bank_work = 0;
#endif
    for (int size = 1; size <= stats.half; ++size)
    {
        const auto layer_start = Clock::now();
        const double layer_dual_before = stats.dual_ms;
        if (size == stats.half)
            stats.streamed_a0_before = best;
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
            const long long dual_rent_before =
                stats.ordinary_seed_candidates_by_size[size] +
                stats.ordinary_pops_by_size[size] +
                stats.ordinary_relax_attempts_by_size[size];
            const int remaining_original = original_full_mask ^ original_mask[mask];
            auto CutH = [&](int vertex)
            {
                if (heuristic_stamp[vertex] != stamp)
                {
                    heuristic_stamp[vertex] = stamp;
                    heuristic[vertex] = DualAt(vertex, remaining_original);
                    heuristic_state[vertex] = kCutReady;
                    ++stats.ordinary_h_evals_by_size[size];
                }
                return heuristic[vertex];
            };
            auto CheapH = [&](int vertex)
            {
                const double cut = CutH(vertex);
                if (heuristic_state[vertex] == kCutReady)
                {
                    double farthest = 0.0;
                    for (int bits = remaining_original; bits; bits &= bits - 1)
                        farthest = std::max(
                            farthest, group_distance[FirstBit(bits & -bits)][vertex]);
                    heuristic[vertex] = std::max(farthest, cut);
                    ++stats.ordinary_h_farthest_evals_by_size[size];
                    if (farthest >= cut)
                    {
                        heuristic_state[vertex] = kFarthestSource;
                        ++stats.ordinary_h_farthest_by_size[size];
                    }
                    else
                    {
                        heuristic_state[vertex] = kDualSource;
                        ++stats.ordinary_h_dual_by_size[size];
                    }
                }
                return heuristic[vertex];
            };
            auto H = [&](int vertex)
            {
                const double cheap = CheapH(vertex);
                if (heuristic_state[vertex] != kTourReady)
                {
                    ++stats.ordinary_h_tour_evals_by_size[size];
                    const double tour =
                        lower_bound.At(vertex, remaining_original, group_distance);
                    const bool dual_source = heuristic_state[vertex] == kDualSource;
                    const bool dominates = dual_source ? tour >= cheap : tour > cheap;
                    if (dominates)
                    {
                        if (dual_source)
                            --stats.ordinary_h_dual_by_size[size];
                        else if (heuristic_state[vertex] == kFarthestSource)
                            --stats.ordinary_h_farthest_by_size[size];
                        ++stats.ordinary_h_tour_by_size[size];
                        heuristic[vertex] = tour;
                    }
                    heuristic_state[vertex] = kTourReady;
                }
                return heuristic[vertex];
            };
            auto CanImprove = [&](int vertex, double value)
            {
                if (value + CutH(vertex) > best)
                    return false;
                if (value + CheapH(vertex) > best)
                    return false;
                return value + H(vertex) <= best;
            };
            auto Set = [&](int vertex, double value)
            {
                ++stats.ordinary_seed_candidates_by_size[size];
                if (value >= row_distance[vertex])
                {
                    ++stats.ordinary_seed_reject_old_by_size[size];
                    return;
                }
                if (!CanImprove(vertex, value))
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
                    if (!CanImprove(edge.to, next))
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
#ifdef GST_TEST158_ALL_BRANCHES
                branch_vertices.push_back(vertex);
#else
                if (row_distance[vertex] < split_distance[vertex])
                    branch_vertices.push_back(vertex);
#endif
            StoreOrdinaryRow(mask, settled, branch_vertices);
            stats.ordinary_values += settled.size();
            stats.ordinary_values_by_size[size] += settled.size();
            stats.ordinary_branch_values += branch_vertices.size();
            stats.ordinary_branch_values_by_size[size] += branch_vertices.size();
#ifdef GST_TEST80_PROGRESSIVE_DUAL
            if (!dual_built)
            {
                const long long dual_rent_after =
                    stats.ordinary_seed_candidates_by_size[size] +
                    stats.ordinary_pops_by_size[size] +
                    stats.ordinary_relax_attempts_by_size[size];
                stats.dual_rent_work += dual_rent_after - dual_rent_before;
                dual_bank_work += dual_rent_after - dual_rent_before;
                ++dual_rent_rows;
                if (dual_available)
                {
                    while (!dual_built &&
                           dual_bank_work >= stats.dual_step_buy_work)
                    {
                        if (!stats.dual_trigger_size)
                        {
                            stats.dual_trigger_size = size;
                            stats.dual_trigger_rows = dual_rent_rows;
                        }
                        dual_bank_work -= stats.dual_step_buy_work;
                        AdvanceDual();
                    }
                    stats.dual_bank_work = dual_bank_work;
                    if (size > 2)
                    {
                        if (dual_built)
                        {
                            dual.ReleaseResidual();
                            packing_built = true;
                        }
                    }
                }
            }
#endif
            if (size == 2)
            {
                const int first_bit = FirstBit(mask);
                const int second_bit = FirstBit(mask ^ (1 << first_bit));
                const int first_group = std::min(bit_to_group[first_bit],
                                                 bit_to_group[second_bit]);
                const int second_group = std::max(bit_to_group[first_bit],
                                                  bit_to_group[second_bit]);
                stats.ordinary_pair_values[first_group * 16 + second_group] =
                    static_cast<long long>(settled.size());
            }
            if (!kIwataM0 && size == stats.half)
            {
                const int complement = full_mask ^ mask;
                const int complement_size = popcount[complement];
                const bool closes_partition =
                    complement_size < size ||
                    (complement_size == size && complement < mask);
                if (closes_partition && Available(complement))
                {
                    const auto completion_start = Clock::now();
                    ++stats.streamed_a0_partitions;
                    const int first = std::min(mask, complement);
                    const int second = std::max(mask, complement);
                    ForEachTripleValue(first, second, 0, [&](int, double value)
                    {
                        ++stats.streamed_a0_checks;
                        if (value < best)
                        {
                            best = value;
                            ++stats.streamed_a0_updates;
                        }
                    });
                    stats.streamed_a0_ms +=
                        std::chrono::duration<double, std::milli>(
                            Clock::now() - completion_start)
                            .count();
                }
            }
            if (!kIwataM0 && size == early_block_limit)
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
                if (!packing_built && dual_built &&
                    completed_pair_rows < pair_row_count &&
                    stats.pair_work >= stats.packing_budget)
                    BuildPacking();
            }
        }

        if (size == 2 && !packing_built && dual_built)
        {
            dual.ReleaseResidual();
            packing_built = true;
        }

        stats.ordinary_ms_by_size[size] =
            std::chrono::duration<double, std::milli>(Clock::now() - layer_start).count() -
            (stats.dual_ms - layer_dual_before);
        if (!kIwataM0 && size == early_block_limit)
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
#if defined(GST_TEST80_ANCHOR_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_FACILITY)
        EvaluateAnchorFacility(size);
#ifdef GST_TEST80_PROGRESS
        std::cerr << "PROBE anchor_facility size=" << size
                  << " upper=" << stats.anchor_facility_upper_by_size[size]
                  << " incumbent=" << best
                  << " probes=" << stats.anchor_facility_probes_by_size[size]
                  << " ms=" << stats.anchor_facility_ms_by_size[size] << '\n';
#endif
#endif
#if defined(GST_TEST80_ANCHOR_TREE_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_TREE_FACILITY)
#ifdef GST_TEST80_ANCHOR_TREE_AMORTIZED
        // A tighter incumbent cannot avoid enumerating the current layer's
        // merge candidates.  It can avoid queue work and graph scans, so only
        // those incumbent-sensitive operations pay for another tree DP.
        anchor_tree_rent += stats.ordinary_pops_by_size[size] +
                            stats.ordinary_relax_attempts_by_size[size];
        stats.anchor_tree_rent_by_size[size] = anchor_tree_rent;
        stats.anchor_tree_vertices_by_size[size] =
            static_cast<int>(anchor_tree.vertices.size());
        if (anchor_tree_rent >= stats.anchor_tree_buy_work)
        {
            EvaluateAnchorTreeFacility(size);
            anchor_tree_rent = 0;
        }
#else
        EvaluateAnchorTreeFacility(size);
#endif
#ifdef GST_TEST80_PROGRESS
        std::cerr << "PROBE anchor_tree_facility size=" << size
                  << " evaluated="
                  << stats.anchor_tree_evaluated_by_size[size]
                  << " upper=" << stats.anchor_tree_upper_by_size[size]
                  << " scalar_upper="
                  << stats.anchor_facility_upper_by_size[size]
                  << " incumbent=" << best
                  << " vertices=" << stats.anchor_tree_vertices_by_size[size]
                  << " rent=" << stats.anchor_tree_rent_by_size[size]
                  << " buy=" << stats.anchor_tree_buy_work
                  << " probes=" << stats.anchor_tree_probes_by_size[size]
                  << " ms=" << stats.anchor_tree_ms_by_size[size] << '\n';
#endif
#endif
        stats.best_after_ordinary_size[size] = best;
#ifdef GST_TEST80_PROGRESS
        std::cerr << "PROGRESS phase=D size=" << size
                  << " masks=" << stats.ordinary_masks_by_size[size]
                  << " values=" << stats.ordinary_values_by_size[size]
                  << " branches=" << stats.ordinary_branch_values_by_size[size]
                  << " pops=" << stats.ordinary_pops_by_size[size]
                  << " row_bytes=" << stats.ordinary_row_bytes_by_size[size]
                  << " best=" << best
                  << " closure_ms=" << stats.ordinary_ms_by_size[size] << '\n';
#endif
#if defined(GST_TEST80_D1_PROBE) || defined(GST_TEST80_D2_PROBE) || \
    defined(GST_TEST80_D3_PROBE) || defined(GST_TEST80_D4_PROBE)
#ifdef GST_TEST80_D1_PROBE
        constexpr int ordinary_probe_limit = 1;
#elif defined(GST_TEST80_D2_PROBE)
        constexpr int ordinary_probe_limit = 2;
#elif defined(GST_TEST80_D3_PROBE)
        constexpr int ordinary_probe_limit = 3;
#else
        constexpr int ordinary_probe_limit = 4;
#endif
        if (size == ordinary_probe_limit)
        {
            stats.streamed_a0_after = best;
            stats.ordinary_ms = std::chrono::duration<double, std::milli>(
                                    Clock::now() - ordinary_start)
                                    .count() -
                                (stats.dual_ms - ordinary_dual_before);
            stats.total_ms =
                std::chrono::duration<double, std::milli>(Clock::now() - total_start)
                    .count();
            result.best_weight = best;
            result.feasible = false;
            return result;
        }
#endif
    }
    dual.ReleaseResidual();
#if defined(GST_TEST80_ANCHOR_FACILITY_PROBE) || \
    defined(GST_TEST80_ANCHOR_FACILITY)
    std::vector<double>().swap(anchor_path_distance);
    std::vector<double>().swap(anchor_attachment);
    std::vector<double>().swap(anchor_facility);
#endif
    stats.streamed_a0_after = best;
    stats.ordinary_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - ordinary_start).count() -
        (stats.dual_ms - ordinary_dual_before);
    if (kIwataM0)
    {
        const auto completion_start = Clock::now();
        for (int first = full_mask;; first = (first - 1) & full_mask)
        {
            const int remaining = full_mask ^ first;
            for (int second = remaining;; second = (second - 1) & remaining)
            {
                const int third = remaining ^ second;
                if (first <= second && second <= third &&
                    popcount[first] <= stats.half &&
                    popcount[second] <= stats.half &&
                    popcount[third] <= stats.half &&
                    (!first || Available(first)) &&
                    (!second || Available(second)) &&
                    (!third || Available(third)))
                {
                    ++stats.completion_partitions;
                    ForEachTripleValue(first, second, third, [&](int, double value)
                    {
                        ++stats.completion_checks;
                        best = std::min(best, value);
                    });
                }
                if (!second)
                    break;
            }
            if (!first)
                break;
        }
        stats.completion_ms =
            std::chrono::duration<double, std::milli>(
                Clock::now() - completion_start)
                .count();
        stats.total_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - total_start)
                .count();
        result.best_weight = best;
        result.feasible = best < fp::kInf;
        return result;
    }
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
        if (row.bitmap)
        {
            size_t index = 0;
            for (size_t word = 0; word < row.vertex_bits.size(); ++word)
            {
                std::uint64_t bits = row.vertex_bits[word];
                while (bits)
                {
                    const int bit = FirstBit64(bits);
                    use(static_cast<int>((word << 6) + bit), row.distances[index++]);
                    bits &= bits - 1;
                }
            }
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
        if (a.bitmap && (d.dense || d.bitmap))
        {
            ++stats.anchored_join_direct_calls;
            stats.anchored_join_direct_work +=
                ForEachBitmapBranchPair(a, d,
                                        [&](int vertex,
                                            double a_value,
                                            double d_value)
                                        {
                                            use(vertex, a_value + d_value);
                                        });
            return;
        }
        if (a.dense || a.bitmap)
        {
            ++stats.anchored_join_direct_calls;
            stats.anchored_join_direct_work += BranchScanWork(d);
            ForEachBranchIndex(d, [&](size_t index)
            {
                const int vertex = (d.dense || d.bitmap)
                                       ? static_cast<int>(index)
                                       : d.vertices[index];
                const double a_value = RowValueAt(a, vertex);
                const double d_value = d.bitmap ? BitmapValue(d, vertex)
                                                : d.distances[index];
                if (a_value < fp::kInf)
                    use(vertex, a_value + d_value);
            });
            return;
        }
        if (d.dense || d.bitmap)
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
                        const double d_value =
                            d.bitmap ? BitmapValue(d, vertex) : d.distances[index];
                        use(vertex, a.distances[a_index] + d_value);
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
                    if (d.IsBranchVertex(vertex))
                    {
                        const double d_value =
                            d.bitmap ? BitmapValue(d, vertex) : d.distances[vertex];
                        use(vertex, a.distances[index] + d_value);
                    }
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
                if (d.IsBranchIndex(d_index))
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
                if (d.IsBranchIndex(d_index))
                    use(a.vertices[a_index], a.distances[a_index] + d.distances[d_index]);
                ++a_index;
                ++d_index;
            }
        }
    };

    int active_anchored_size = 0;
    std::vector<std::uint64_t> completion_root_bits(
        (static_cast<size_t>(graph.n + 1) + 63) / 64);
    auto CompleteRoots = [&](int mask,
                             const std::vector<int>& roots,
                             double anchored_minimum)
    {
        const auto begin = Clock::now();
        ++stats.completion_rows;
        ++stats.completion_rows_by_size[active_anchored_size];
        stats.completion_vertices += roots.size();
        bool root_bits_ready = false;
        auto EnsureRootBits = [&]
        {
            if (root_bits_ready)
                return;
            std::fill(completion_root_bits.begin(), completion_root_bits.end(), 0);
            for (int vertex : roots)
                completion_root_bits[static_cast<size_t>(vertex) >> 6] |=
                    1ULL << (vertex & 63);
            root_bits_ready = true;
            ++stats.completion_bitmap_root_rows;
        };
        auto OrdinaryMinimum = [&](int side)
        {
            return !side || popcount[side] == 1 ? 0.0 : ordinary_minimum[side];
        };
        const int remaining = full_mask ^ mask;
        for (int left = remaining;; left = (left - 1) & remaining)
        {
            const int right = remaining ^ left;
            if (left <= right && popcount[left] <= stats.half &&
                popcount[right] <= stats.half &&
                (!left || Available(left)) && (!right || Available(right)))
            {
                ++stats.completion_partitions;
                ++stats.completion_partitions_by_size[active_anchored_size];
                const double left_minimum = OrdinaryMinimum(left);
                const double right_minimum = OrdinaryMinimum(right);
                if (anchored_minimum + left_minimum + right_minimum > best)
                {
                    ++stats.completion_partition_min_rejects;
                    ++stats.completion_partition_min_rejects_by_size[
                        active_anchored_size];
                }
                else
                {
                    const double side_minimum = left_minimum + right_minimum;
                    const OrdinaryRow* left_row =
                        left && popcount[left] > 1 ? &ordinary[left] : nullptr;
                    const OrdinaryRow* right_row =
                        right && popcount[right] > 1 ? &ordinary[right] : nullptr;
                    const OrdinaryRow* left_bitmap =
                        left_row && left_row->bitmap ? left_row : nullptr;
                    const OrdinaryRow* right_bitmap =
                        right_row && right_row->bitmap ? right_row : nullptr;
                    const bool bitmap_compatible =
                        (left_bitmap || right_bitmap) &&
                        (!left_row || left_bitmap) && (!right_row || right_bitmap);
                    if (bitmap_compatible)
                    {
                        EnsureRootBits();
                        ++stats.completion_bitmap_calls;
                        ++stats.completion_bitmap_calls_by_size[active_anchored_size];
                        stats.completion_bitmap_words += completion_root_bits.size();
                        stats.completion_bitmap_words_by_size[active_anchored_size] +=
                            completion_root_bits.size();
                        const DenseRow* left_singleton =
                            left && popcount[left] == 1
                                ? &group_distance[bit_to_group[FirstBit(left)]]
                                : nullptr;
                        const DenseRow* right_singleton =
                            right && popcount[right] == 1
                                ? &group_distance[bit_to_group[FirstBit(right)]]
                                : nullptr;
                        const CompletionBitmapResult bitmap_result =
                            CompleteBitmapIntersection(completion_root_bits,
                                                       left_bitmap,
                                                       right_bitmap,
                                                       left_singleton,
                                                       right_singleton,
                                                       row_distance,
                                                       left_minimum,
                                                       right_minimum,
                                                       best);
                        best = bitmap_result.best;
                        stats.completion_scan_vertices += bitmap_result.visits;
                        stats.completion_scan_vertices_by_size[active_anchored_size] +=
                            bitmap_result.visits;
                        stats.completion_checks += bitmap_result.checks;
                        stats.completion_checks_by_size[active_anchored_size] +=
                            bitmap_result.checks;
                        stats.completion_root_min_rejects +=
                            bitmap_result.root_rejects;
                        stats.completion_root_min_rejects_by_size[active_anchored_size] +=
                            bitmap_result.root_rejects;
                        stats.completion_component_min_rejects +=
                            bitmap_result.component_rejects;
                        stats.completion_component_min_rejects_by_size[
                            active_anchored_size] += bitmap_result.component_rejects;
                    }
                    else
                    {
                        int driver_mask = 0;
                        size_t driver_size = roots.size();
                        if (left_row)
                        {
                            const size_t size = left_row->dense
                                                    ? static_cast<size_t>(graph.n)
                                                    : left_row->distances.size();
                            if (size < driver_size)
                            {
                                driver_mask = left;
                                driver_size = size;
                            }
                        }
                        if (right_row)
                        {
                            const size_t size = right_row->dense
                                                    ? static_cast<size_t>(graph.n)
                                                    : right_row->distances.size();
                            if (size < driver_size)
                            {
                                driver_mask = right;
                                driver_size = size;
                            }
                        }

                        size_t root_index = 0;
                        size_t left_index = 0;
                        size_t right_index = 0;
                        auto Visit = [&](int vertex, double driver_value)
                        {
                            ++stats.completion_scan_vertices;
                            ++stats.completion_scan_vertices_by_size[active_anchored_size];
                            while (root_index < roots.size() && roots[root_index] < vertex)
                                ++root_index;
                            if (root_index == roots.size() || roots[root_index] != vertex)
                                return;
                            if (row_distance[vertex] + side_minimum > best)
                            {
                                ++stats.completion_root_min_rejects;
                                ++stats.completion_root_min_rejects_by_size[
                                    active_anchored_size];
                                return;
                            }
                            if (left && right && driver_mask == left &&
                                row_distance[vertex] + driver_value + right_minimum > best)
                            {
                                ++stats.completion_component_min_rejects;
                                ++stats.completion_component_min_rejects_by_size[
                                    active_anchored_size];
                                return;
                            }
                            if (left && right && driver_mask == right &&
                                row_distance[vertex] + left_minimum + driver_value > best)
                            {
                                ++stats.completion_component_min_rejects;
                                ++stats.completion_component_min_rejects_by_size[
                                    active_anchored_size];
                                return;
                            }

                            double a = 0.0;
                            if (left)
                            {
                                if (popcount[left] == 1)
                                    a = group_distance[bit_to_group[FirstBit(left)]][vertex];
                                else if (driver_mask == left)
                                    a = driver_value;
                                else if (left_row->dense)
                                    a = left_row->distances[vertex];
                                else if (left_row->bitmap)
                                {
                                    a = BitmapValue(*left_row, vertex);
                                    if (a >= fp::kInf)
                                        return;
                                }
                                else
                                {
                                    while (left_index < left_row->vertices.size() &&
                                           left_row->vertices[left_index] < vertex)
                                        ++left_index;
                                    if (left_index == left_row->vertices.size() ||
                                        left_row->vertices[left_index] != vertex)
                                        return;
                                    a = left_row->distances[left_index];
                                }
                            }
                            if (left && right && !driver_mask &&
                                row_distance[vertex] + a + right_minimum > best)
                            {
                                ++stats.completion_component_min_rejects;
                                ++stats.completion_component_min_rejects_by_size[
                                    active_anchored_size];
                                return;
                            }

                            double b = 0.0;
                            if (right)
                            {
                                if (popcount[right] == 1)
                                    b = group_distance[bit_to_group[FirstBit(right)]][vertex];
                                else if (driver_mask == right)
                                    b = driver_value;
                                else if (right_row->dense)
                                    b = right_row->distances[vertex];
                                else if (right_row->bitmap)
                                {
                                    b = BitmapValue(*right_row, vertex);
                                    if (b >= fp::kInf)
                                        return;
                                }
                                else
                                {
                                    while (right_index < right_row->vertices.size() &&
                                           right_row->vertices[right_index] < vertex)
                                        ++right_index;
                                    if (right_index == right_row->vertices.size() ||
                                        right_row->vertices[right_index] != vertex)
                                        return;
                                    b = right_row->distances[right_index];
                                }
                            }
                            best = std::min(best, row_distance[vertex] + a + b);
                            ++stats.completion_checks;
                            ++stats.completion_checks_by_size[active_anchored_size];
                        };
                        if (driver_mask)
                            ForEachValue(driver_mask, [&](int vertex, double value)
                            {
                                Visit(vertex, value);
                            });
                        else
                            for (int vertex : roots)
                                Visit(vertex, 0.0);

                    }

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
    if (stats.half < 2)
        CompleteRoots(0, all_vertices, 0.0);
    stats.best_after_anchored_size[0] = best;
    stats.anchored_ms_by_size[0] =
        std::chrono::duration<double, std::milli>(Clock::now() - anchored_zero_start).count();
    for (int vertex : all_vertices)
        row_distance[vertex] = fp::kInf;

    auto ProcessAnchoredMask = [&](int mask, int size)
    {
        const auto mask_start = Clock::now();
        active_anchored_size = size;
        ++stats.anchored_masks;
        ++stats.anchored_masks_by_size[size];
        ++stamp;
        touched.clear();
        settled.clear();
        double anchored_minimum = fp::kInf;
        const int remaining_nonanchor = full_mask ^ mask;
        const int remaining_original = nonanchor_original_mask ^ original_mask[mask];
        auto CutBaseH = [&](int vertex)
        {
            if (heuristic_stamp[vertex] != stamp)
            {
                heuristic_stamp[vertex] = stamp;
                heuristic[vertex] = DualAt(vertex, remaining_original);
                heuristic_state[vertex] = kCutReady;
                ++stats.anchored_h_evals_by_size[size];
            }
            return heuristic[vertex];
        };
        auto CheapBaseH = [&](int vertex)
        {
            const double cut = CutBaseH(vertex);
            if (heuristic_state[vertex] == kCutReady)
            {
                double farthest = 0.0;
                for (int bits = remaining_nonanchor; bits; bits &= bits - 1)
                    farthest = std::max(
                        farthest,
                        group_distance[bit_to_group[FirstBit(bits & -bits)]][vertex]);
                heuristic[vertex] = std::max(farthest, cut);
                ++stats.anchored_h_farthest_evals_by_size[size];
                if (farthest >= cut)
                {
                    heuristic_state[vertex] = kFarthestSource;
                    ++stats.anchored_h_farthest_by_size[size];
                }
                else
                {
                    heuristic_state[vertex] = kDualSource;
                    ++stats.anchored_h_dual_by_size[size];
                }
            }
            return heuristic[vertex];
        };
        auto BaseH = [&](int vertex)
        {
            const double cheap = CheapBaseH(vertex);
            if (heuristic_state[vertex] != kTourReady)
            {
                ++stats.anchored_h_tour_evals_by_size[size];
                const double tour =
                    lower_bound.At(vertex, remaining_original, group_distance);
                const bool dual_source = heuristic_state[vertex] == kDualSource;
                const bool dominates = dual_source ? tour >= cheap : tour > cheap;
                if (dominates)
                {
                    if (dual_source)
                        --stats.anchored_h_dual_by_size[size];
                    else if (heuristic_state[vertex] == kFarthestSource)
                        --stats.anchored_h_farthest_by_size[size];
                    ++stats.anchored_h_tour_by_size[size];
                    heuristic[vertex] = tour;
                }
                heuristic_state[vertex] = kTourReady;
            }
            return heuristic[vertex];
        };
        auto CanImprove = [&](int vertex, double value)
        {
            if (value + CutBaseH(vertex) > best)
                return false;
            if (value + CheapBaseH(vertex) > best)
                return false;
            return value + BaseH(vertex) <= best;
        };
        auto Set = [&](int vertex, double value)
        {
            ++stats.anchored_seed_candidates_by_size[size];
            if (value >= row_distance[vertex])
            {
                ++stats.anchored_seed_reject_old_by_size[size];
                return;
            }
            if (!CanImprove(vertex, value))
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

        if (touched.empty())
        {
            stats.anchored_ms_by_size[size] +=
                std::chrono::duration<double, std::milli>(Clock::now() - mask_start)
                    .count();
            return;
        }

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
            anchored_minimum = std::min(anchored_minimum, node.distance);
            for (const auto& edge : graph.adj[node.vertex])
            {
                const double next = node.distance + edge.w;
                ++stats.anchored_relax_attempts_by_size[size];
                if (next >= row_distance[edge.to])
                {
                    ++stats.anchored_relax_reject_old_by_size[size];
                    continue;
                }
                if (!CanImprove(edge.to, next))
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
        CompleteRoots(mask, settled, anchored_minimum);
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
            const size_t word_count = (static_cast<size_t>(graph.n + 1) + 63) / 64;
            const size_t bitmap_bytes =
                settled.size() * sizeof(double) +
                word_count * (sizeof(std::uint64_t) + sizeof(std::uint32_t));
            row.bitmap = bitmap_bytes < sparse_bytes && bitmap_bytes < dense_bytes;
            row.dense = !row.bitmap && dense_bytes < sparse_bytes;
            if (row.dense)
            {
                row.distances.assign(graph.n + 1, fp::kInf);
                for (int vertex : settled)
                    row.distances[vertex] = row_distance[vertex];
            }
            else if (row.bitmap)
            {
                row.vertex_bits.assign(word_count, 0);
                row.distances.reserve(settled.size());
                for (int vertex : settled)
                {
                    row.vertex_bits[static_cast<size_t>(vertex) >> 6] |=
                        1ULL << (vertex & 63);
                    row.distances.push_back(row_distance[vertex]);
                }
                row.rank_before_word.resize(word_count);
                std::uint32_t rank = 0;
                for (size_t word = 0; word < word_count; ++word)
                {
                    row.rank_before_word[word] = rank;
                    rank += static_cast<std::uint32_t>(Popcount64(row.vertex_bits[word]));
                }
            }
            else
            {
                row.vertices = settled;
                row.distances.reserve(settled.size());
                for (int vertex : settled)
                    row.distances.push_back(row_distance[vertex]);
            }
            row.ready = true;
#ifdef GST_TEST157_VALIDATE_ROWS
            ValidateStoredRow(row, graph.n, false, "anchored");
#endif
            if (row.dense)
                ++stats.anchored_dense_rows_by_size[size];
            else if (row.bitmap)
                ++stats.anchored_bitmap_rows_by_size[size];
            else
                ++stats.anchored_sparse_rows_by_size[size];
            stats.anchored_row_bytes_by_size[size] += RowStorageBytes(row);
        }
        for (int vertex : touched)
            row_distance[vertex] = fp::kInf;
        stats.anchored_ms_by_size[size] +=
            std::chrono::duration<double, std::milli>(Clock::now() - mask_start)
                .count();
    };

    const int final_anchored_size = stats.half - 1;
#ifdef GST_TEST80_ADJOINT_ANCHOR
    const int regular_last_size = std::max(0, final_anchored_size / 2);
    stats.adjoint_cut = regular_last_size;
#else
    const int streamed_producer_size = final_anchored_size - 1;
    const int regular_last_size = std::max(0, streamed_producer_size - 1);
#endif
    for (int size = 1; size <= regular_last_size; ++size)
    {
        for (int mask = 1; mask < subset_count; ++mask)
            if (popcount[mask] == size)
                ProcessAnchoredMask(mask, size);
        stats.best_after_anchored_size[size] = best;
#ifdef GST_TEST80_PROGRESS
        std::cerr << "PROGRESS phase=A size=" << size
                  << " masks=" << stats.anchored_masks_by_size[size]
                  << " values=" << stats.anchored_values_by_size[size]
                  << " pops=" << stats.anchored_pops_by_size[size]
                  << " row_bytes=" << stats.anchored_row_bytes_by_size[size]
                  << " completion_ms=" << stats.completion_ms_by_size[size]
                  << " best=" << best
                  << " layer_ms=" << stats.anchored_ms_by_size[size] << '\n';
#endif
    }

#ifdef GST_TEST80_ADJOINT_ANCHOR
    const auto adjoint_start = Clock::now();
    std::vector<OrdinaryRow> backward(subset_count);
    auto ForEachBackwardValue = [&](const OrdinaryRow& row, auto&& use)
    {
        if (row.dense)
        {
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                if (row.distances[vertex] < fp::kInf)
                    use(vertex, row.distances[vertex]);
            return;
        }
        if (row.bitmap)
        {
            size_t index = 0;
            for (size_t word = 0; word < row.vertex_bits.size(); ++word)
            {
                std::uint64_t bits = row.vertex_bits[word];
                while (bits)
                {
                    const int bit = FirstBit64(bits);
                    use(static_cast<int>((word << 6) + bit), row.distances[index++]);
                    bits &= bits - 1;
                }
            }
            return;
        }
        for (size_t index = 0; index < row.vertices.size(); ++index)
            use(row.vertices[index], row.distances[index]);
    };
    auto ForEachBackwardBranchSum = [&](int block,
                                        const OrdinaryRow& backward_row,
                                        auto&& use)
    {
        if (popcount[block] == 1)
        {
            const auto& block_distance =
                group_distance[bit_to_group[FirstBit(block)]];
            ForEachBackwardValue(backward_row, [&](int vertex, double value)
            {
                use(vertex, value + block_distance[vertex]);
            });
            return;
        }

        const OrdinaryRow& ordinary_row = ordinary[block];
        if ((ordinary_row.dense || ordinary_row.bitmap) &&
            !backward_row.dense && !backward_row.bitmap)
        {
            for (size_t index = 0; index < backward_row.vertices.size(); ++index)
            {
                const int vertex = backward_row.vertices[index];
                if (!ordinary_row.IsBranchVertex(vertex))
                    continue;
                const double ordinary_value = ordinary_row.bitmap
                                                  ? BitmapValue(ordinary_row, vertex)
                                                  : ordinary_row.distances[vertex];
                use(vertex, backward_row.distances[index] + ordinary_value);
            }
            return;
        }
        if (ordinary_row.dense || ordinary_row.bitmap ||
            backward_row.dense || backward_row.bitmap)
        {
            ForEachBranchValue(block, [&](int vertex, double ordinary_value)
            {
                const double backward_value = RowValueAt(backward_row, vertex);
                if (backward_value < fp::kInf)
                    use(vertex, backward_value + ordinary_value);
            });
            return;
        }

        size_t backward_index = 0;
        size_t ordinary_index = 0;
        while (backward_index < backward_row.vertices.size() &&
               ordinary_index < ordinary_row.vertices.size())
        {
            if (backward_row.vertices[backward_index] <
                ordinary_row.vertices[ordinary_index])
                ++backward_index;
            else if (ordinary_row.vertices[ordinary_index] <
                     backward_row.vertices[backward_index])
                ++ordinary_index;
            else
            {
                if (ordinary_row.IsBranchIndex(ordinary_index))
                    use(backward_row.vertices[backward_index],
                        backward_row.distances[backward_index] +
                            ordinary_row.distances[ordinary_index]);
                ++backward_index;
                ++ordinary_index;
            }
        }
    };
    auto StoreBackwardRow = [&](OrdinaryRow& row, const std::vector<int>& vertices)
    {
        const size_t sparse_bytes = vertices.size() * (sizeof(int) + sizeof(double));
        const size_t dense_bytes = static_cast<size_t>(graph.n + 1) * sizeof(double);
        const size_t word_count = (static_cast<size_t>(graph.n + 1) + 63) / 64;
        const size_t bitmap_bytes =
            vertices.size() * sizeof(double) +
            word_count * (sizeof(std::uint64_t) + sizeof(std::uint32_t));
        row.bitmap = bitmap_bytes < sparse_bytes && bitmap_bytes < dense_bytes;
        row.dense = !row.bitmap && dense_bytes < sparse_bytes;
        if (row.dense)
        {
            row.distances.assign(graph.n + 1, fp::kInf);
            for (int vertex : vertices)
                row.distances[vertex] = row_distance[vertex];
        }
        else if (row.bitmap)
        {
            row.vertex_bits.assign(word_count, 0);
            row.distances.reserve(vertices.size());
            for (int vertex : vertices)
            {
                row.vertex_bits[static_cast<size_t>(vertex) >> 6] |=
                    1ULL << (vertex & 63);
                row.distances.push_back(row_distance[vertex]);
            }
            row.rank_before_word.resize(word_count);
            std::uint32_t rank = 0;
            for (size_t word = 0; word < word_count; ++word)
            {
                row.rank_before_word[word] = rank;
                rank += static_cast<std::uint32_t>(Popcount64(row.vertex_bits[word]));
            }
        }
        else
        {
            row.vertices = vertices;
            row.distances.reserve(vertices.size());
            for (int vertex : vertices)
                row.distances.push_back(row_distance[vertex]);
        }
        row.ready = true;
#ifdef GST_TEST157_VALIDATE_ROWS
        ValidateStoredRow(row, graph.n, false, "backward");
#endif
    };
    auto FarthestAnchoredLower = [&](int mask, int vertex)
    {
        double lower = group_distance[anchor_group][vertex];
        for (int bits = mask; bits; bits &= bits - 1)
            lower = std::max(
                lower,
                group_distance[bit_to_group[FirstBit(bits & -bits)]][vertex]);
        return lower;
    };
#ifdef GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL
    struct TransposedValue
    {
        int mask = 0;
        double value = fp::kInf;
        double reduced = fp::kInf;
    };
    const auto transpose_start = Clock::now();
    std::vector<std::vector<int>> terminal_vertices(subset_count);
    std::vector<std::vector<double>> terminal_distances(subset_count);
    std::array<std::vector<TransposedValue>, 64> values_by_offset;
    std::vector<size_t> sparse_cursor(subset_count);
    std::vector<double> mask_potential(subset_count);
    std::vector<int> mask_potential_stamp(subset_count);
    std::vector<double> vertex_value(subset_count);
    std::vector<double> vertex_reduced(subset_count);
    std::vector<int> vertex_value_stamp(subset_count);
    int potential_epoch = 0;
    int value_epoch = 0;
    std::vector<double> terminal_best(subset_count, fp::kInf);
    std::vector<int> terminal_touched;
    const size_t word_count = (static_cast<size_t>(graph.n + 1) + 63) / 64;
    for (size_t word = 0; word < word_count; ++word)
    {
        for (auto& values : values_by_offset)
            values.clear();
        const int first_vertex = std::max(1, static_cast<int>(word << 6));
        const int end_vertex =
            std::min(graph.n + 1, static_cast<int>((word + 1) << 6));
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (popcount[mask] > stats.half || !Available(mask))
                continue;
            auto Add = [&](int vertex, double value)
            {
                values_by_offset[vertex & 63].push_back({mask, value, 0.0});
                ++stats.adjoint_transpose_values;
            };
            if (popcount[mask] == 1)
            {
                const auto& distance =
                    group_distance[bit_to_group[FirstBit(mask)]];
                for (int vertex = first_vertex; vertex < end_vertex; ++vertex)
                    Add(vertex, distance[vertex]);
                continue;
            }

            const OrdinaryRow& row = ordinary[mask];
            if (row.dense)
            {
                for (int vertex = first_vertex; vertex < end_vertex; ++vertex)
                    if (row.distances[vertex] < fp::kInf)
                        Add(vertex, row.distances[vertex]);
                continue;
            }
            if (row.bitmap)
            {
                std::uint64_t bits = row.vertex_bits[word];
                size_t index = row.rank_before_word[word];
                while (bits)
                {
                    const int bit = FirstBit64(bits);
                    Add(static_cast<int>((word << 6) + bit),
                        row.distances[index++]);
                    bits &= bits - 1;
                }
                continue;
            }
            size_t& index = sparse_cursor[mask];
            while (index < row.vertices.size() && row.vertices[index] < end_vertex)
            {
                Add(row.vertices[index], row.distances[index]);
                ++index;
            }
        }

        for (int vertex = first_vertex; vertex < end_vertex; ++vertex)
        {
            auto& values = values_by_offset[vertex & 63];
            if (values.empty())
                continue;
            ++potential_epoch;
            mask_potential_stamp[0] = potential_epoch;
            mask_potential[0] = 0.0;
            auto Potential = [&](auto&& self, int mask) -> double
            {
                if (mask_potential_stamp[mask] == potential_epoch)
                    return mask_potential[mask];
                const int bit = mask & -mask;
                const double value =
                    self(self, mask ^ bit) +
                    DualGroupAt(vertex, bit_to_group[FirstBit(bit)]);
                mask_potential_stamp[mask] = potential_epoch;
                mask_potential[mask] = value;
                return value;
            };
            const double full_potential =
                DualGroupAt(vertex, anchor_group) + Potential(Potential, full_mask);
            const double reduced_budget = best - full_potential;
            if (reduced_budget < 0.0)
                continue;
            for (TransposedValue& entry : values)
                entry.reduced = entry.value - Potential(Potential, entry.mask);
            std::sort(values.begin(), values.end(), [](const auto& left, const auto& right)
            {
                if (left.reduced != right.reduced)
                    return left.reduced < right.reduced;
                return left.mask < right.mask;
            });
            ++value_epoch;
            long long submask_pair_work = 0;
            for (const TransposedValue& entry : values)
            {
                vertex_value_stamp[entry.mask] = value_epoch;
                vertex_value[entry.mask] = entry.value;
                vertex_reduced[entry.mask] = entry.reduced;
                submask_pair_work +=
                    (static_cast<long long>(subset_count) >> popcount[entry.mask]) - 1;
            }
            long long global_pair_work = 0;
            size_t right_limit = values.size();
            for (size_t left = 0; left + 1 < values.size(); ++left)
            {
                while (right_limit > left + 1 &&
                       values[left].reduced + values[right_limit - 1].reduced >
                           reduced_budget)
                    --right_limit;
                if (right_limit <= left + 1)
                    break;
                global_pair_work +=
                    static_cast<long long>(right_limit - left - 1);
            }

            terminal_touched.clear();
            auto UpdateTerminal = [&](int target, double value)
            {
                const int target_size = popcount[target];
                if (target_size <= regular_last_size ||
                    target_size > final_anchored_size)
                    return;
                if (terminal_best[target] == fp::kInf)
                    terminal_touched.push_back(target);
                terminal_best[target] = std::min(terminal_best[target], value);
            };
            for (const TransposedValue& entry : values)
            {
                if (entry.reduced > reduced_budget)
                    break;
                UpdateTerminal(full_mask ^ entry.mask, entry.value);
            }
            // Exact probe counts choose the faster route while preserving O(3^k).
            if (global_pair_work <= submask_pair_work)
            {
                ++stats.adjoint_transpose_global_vertices;
                for (size_t left = 0; left < values.size(); ++left)
                {
                    if (left + 1 == values.size() ||
                        values[left].reduced + values[left + 1].reduced >
                            reduced_budget)
                        break;
                    for (size_t right = left + 1; right < values.size(); ++right)
                    {
                        if (values[left].reduced + values[right].reduced >
                            reduced_budget)
                            break;
                        ++stats.adjoint_transpose_pair_probes;
                        ++stats.adjoint_transpose_global_pair_probes;
                        if (values[left].mask & values[right].mask)
                            continue;
                        ++stats.adjoint_transpose_disjoint_pairs;
                        const int covered =
                            values[left].mask | values[right].mask;
                        UpdateTerminal(
                            full_mask ^ covered,
                            values[left].value + values[right].value);
                    }
                }
            }
            else
            {
                ++stats.adjoint_transpose_submask_vertices;
                for (const TransposedValue& left : values)
                {
                    const int complement = full_mask ^ left.mask;
                    for (int right = complement; right;
                         right = (right - 1) & complement)
                    {
                        ++stats.adjoint_transpose_pair_probes;
                        ++stats.adjoint_transpose_submask_pair_probes;
                        if (right <= left.mask ||
                            vertex_value_stamp[right] != value_epoch ||
                            left.reduced + vertex_reduced[right] > reduced_budget)
                            continue;
                        ++stats.adjoint_transpose_disjoint_pairs;
                        UpdateTerminal(full_mask ^ (left.mask | right),
                                       left.value + vertex_value[right]);
                    }
                }
            }

            for (int target : terminal_touched)
            {
                const int included_original = anchor_bit | original_mask[target];
                const double prefix = std::max(
                    FarthestAnchoredLower(target, vertex),
                    std::max(lower_bound.At(vertex,
                                            included_original,
                                            group_distance),
                             DualGroupAt(vertex, anchor_group) +
                                 Potential(Potential, target)));
                if (terminal_best[target] + prefix <= best)
                {
                    terminal_vertices[target].push_back(vertex);
                    terminal_distances[target].push_back(terminal_best[target]);
                    ++stats.adjoint_transpose_events;
                }
                terminal_best[target] = fp::kInf;
            }
        }
    }
    for (int mask = 1; mask < subset_count; ++mask)
        stats.adjoint_transpose_row_bytes +=
            static_cast<long long>(terminal_vertices[mask].capacity()) * sizeof(int) +
            static_cast<long long>(terminal_distances[mask].capacity()) * sizeof(double);
    stats.adjoint_transpose_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - transpose_start).count();
#endif
#ifdef GST_TEST80_ADJOINT_EAGER_BOUNDARY
    auto EvaluateBackwardBoundary = [&](int successor, int successor_size)
    {
        const auto boundary_start = Clock::now();
        for (int mask = 0; mask < subset_count; ++mask)
        {
            if (popcount[mask] > regular_last_size || (mask & ~successor) ||
                (mask && !anchored[mask].ready))
                continue;
            const int block = successor ^ mask;
            if (!block)
                continue;
            ForEachBackwardBranchSum(block,
                                     backward[successor],
                                     [&](int vertex, double value)
            {
                const double anchored_value =
                    mask ? RowValueAt(anchored[mask], vertex)
                         : group_distance[anchor_group][vertex];
                if (anchored_value >= fp::kInf)
                    return;
                ++stats.adjoint_boundary_checks;
                ++stats.adjoint_boundary_checks_by_size[successor_size];
                const double candidate = anchored_value + value;
                if (candidate < best)
                {
                    best = candidate;
                    ++stats.adjoint_boundary_updates;
                }
            });
        }
        stats.adjoint_boundary_ms_by_size[successor_size] +=
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       boundary_start)
                .count();
    };
#endif

    for (int size = final_anchored_size; size > regular_last_size; --size)
    {
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (popcount[mask] != size)
                continue;
            ++stats.adjoint_rows;
            touched.clear();
            settled.clear();
            ++stamp;
            const int included_original = anchor_bit | original_mask[mask];
            auto PrefixLower = [&](int vertex)
            {
                if (heuristic_stamp[vertex] != stamp)
                {
                    heuristic_stamp[vertex] = stamp;
                    heuristic[vertex] = std::max(
                        FarthestAnchoredLower(mask, vertex),
                        std::max(lower_bound.At(vertex,
                                                included_original,
                                                group_distance),
                                 DualAt(vertex, included_original)));
                }
                return heuristic[vertex];
            };
            auto Set = [&](int vertex, double value, bool terminal)
            {
                ++stats.adjoint_join_checks;
                if (terminal)
                {
                    ++stats.adjoint_terminal_checks;
                    ++stats.adjoint_terminal_checks_by_size[size];
                }
                else
                {
                    ++stats.adjoint_transition_checks;
                    ++stats.adjoint_transition_checks_by_size[size];
                }
                if (value >= row_distance[vertex] ||
                    value + PrefixLower(vertex) > best)
                    return;
                if (row_distance[vertex] == fp::kInf)
                    touched.push_back(vertex);
                row_distance[vertex] = value;
            };

            const int remaining = full_mask ^ mask;
#ifdef GST_TEST80_ADJOINT_TRANSPOSED_TERMINAL
            const auto terminal_start = Clock::now();
            for (size_t index = 0; index < terminal_vertices[mask].size(); ++index)
                Set(terminal_vertices[mask][index],
                    terminal_distances[mask][index],
                    true);
            stats.adjoint_terminal_ms_by_size[size] +=
                std::chrono::duration<double, std::milli>(Clock::now() -
                                                           terminal_start)
                    .count();
#else
            const auto terminal_start = Clock::now();
            for (int left = remaining;; left = (left - 1) & remaining)
            {
                const int right = remaining ^ left;
                if (left <= right && popcount[left] <= stats.half &&
                    popcount[right] <= stats.half &&
                    (!left || Available(left)) && (!right || Available(right)))
                    ForEachPairValues(left, right, [&](int vertex,
                                                       double left_value,
                                                       double right_value)
                    {
                        Set(vertex, left_value + right_value, true);
                    });
                if (!left)
                    break;
            }
            stats.adjoint_terminal_ms_by_size[size] +=
                std::chrono::duration<double, std::milli>(Clock::now() -
                                                           terminal_start)
                    .count();
#endif

            const int outside = full_mask ^ mask;
            const auto transition_start = Clock::now();
            for (int block = outside; block; block = (block - 1) & outside)
            {
                const int successor = mask | block;
                if (popcount[successor] > final_anchored_size ||
                    !backward[successor].ready)
                    continue;
                ForEachBackwardBranchSum(block,
                                         backward[successor],
                                         [&](int vertex, double value)
                {
                    Set(vertex, value, false);
                });
            }
            stats.adjoint_transition_ms_by_size[size] +=
                std::chrono::duration<double, std::milli>(Clock::now() -
                                                           transition_start)
                    .count();

            const auto closure_start = Clock::now();
            std::priority_queue<RowHeapNode,
                                std::vector<RowHeapNode>,
                                std::greater<RowHeapNode>> queue;
            for (int vertex : touched)
                queue.push({row_distance[vertex] + PrefixLower(vertex),
                            row_distance[vertex],
                            vertex});
            while (!queue.empty())
            {
                const RowHeapNode node = queue.top();
                queue.pop();
                ++stats.adjoint_pops;
                if (node.distance != row_distance[node.vertex] || node.key > best)
                    continue;
                settled.push_back(node.vertex);
                for (const auto& edge : graph.adj[node.vertex])
                {
                    const double next = node.distance + edge.w;
                    if (next >= row_distance[edge.to] ||
                        next + PrefixLower(edge.to) > best)
                        continue;
                    if (row_distance[edge.to] == fp::kInf)
                        touched.push_back(edge.to);
                    row_distance[edge.to] = next;
                    queue.push({next + PrefixLower(edge.to), next, edge.to});
                }
            }
            std::sort(settled.begin(), settled.end());
            settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
            StoreBackwardRow(backward[mask], settled);
            stats.adjoint_values += settled.size();
            stats.adjoint_row_bytes += RowStorageBytes(backward[mask]);
            stats.adjoint_closure_ms_by_size[size] +=
                std::chrono::duration<double, std::milli>(Clock::now() -
                                                           closure_start)
                    .count();
#ifdef GST_TEST80_ADJOINT_EAGER_BOUNDARY
            EvaluateBackwardBoundary(mask, size);
#endif
            for (int vertex : touched)
                row_distance[vertex] = fp::kInf;
        }
        stats.best_after_adjoint_size[size] = best;
#ifdef GST_TEST80_PROGRESS
        std::cerr << "PROGRESS phase=H size=" << size
                  << " rows=" << stats.adjoint_rows
                  << " values=" << stats.adjoint_values
                  << " pops=" << stats.adjoint_pops
                  << " join_checks=" << stats.adjoint_join_checks
                  << " terminal_checks=" << stats.adjoint_terminal_checks
                  << " transition_checks=" << stats.adjoint_transition_checks
                  << " row_bytes=" << stats.adjoint_row_bytes
                  << " best=" << best << '\n';
#endif
    }

#ifndef GST_TEST80_ADJOINT_EAGER_BOUNDARY
    for (int size = 0; size <= regular_last_size; ++size)
    {
        for (int mask = 0; mask < subset_count; ++mask)
        {
            if (popcount[mask] != size || (mask && !anchored[mask].ready))
                continue;
            const int outside = full_mask ^ mask;
            for (int block = outside; block; block = (block - 1) & outside)
            {
                const int successor = mask | block;
                if (popcount[successor] <= regular_last_size ||
                    popcount[successor] > final_anchored_size ||
                    !backward[successor].ready)
                    continue;
                ForEachBackwardBranchSum(block,
                                         backward[successor],
                                         [&](int vertex, double value)
                {
                    const double anchored_value =
                        mask ? RowValueAt(anchored[mask], vertex)
                             : group_distance[anchor_group][vertex];
                    if (anchored_value >= fp::kInf)
                        return;
                    ++stats.adjoint_boundary_checks;
                    best = std::min(best, anchored_value + value);
                });
            }
        }
    }
#endif
    for (int size = regular_last_size + 1; size <= final_anchored_size; ++size)
        stats.best_after_anchored_size[size] = best;
    stats.adjoint_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - adjoint_start).count();
#ifdef GST_TEST80_PROGRESS
    std::cerr << "PROGRESS phase=H boundary_checks="
              << stats.adjoint_boundary_checks
              << " boundary_updates=" << stats.adjoint_boundary_updates
              << " best=" << best
              << " ms=" << stats.adjoint_ms << '\n';
#endif
#else
    if (streamed_producer_size < 1)
    {
        for (int size = regular_last_size + 1; size <= final_anchored_size; ++size)
        {
            for (int mask = 1; mask < subset_count; ++mask)
                if (popcount[mask] == size)
                    ProcessAnchoredMask(mask, size);
            stats.best_after_anchored_size[size] = best;
#ifdef GST_TEST80_PROGRESS
            std::cerr << "PROGRESS phase=A size=" << size
                      << " masks=" << stats.anchored_masks_by_size[size]
                      << " values=" << stats.anchored_values_by_size[size]
                      << " pops=" << stats.anchored_pops_by_size[size]
                      << " row_bytes=" << stats.anchored_row_bytes_by_size[size]
                      << " completion_ms=" << stats.completion_ms_by_size[size]
                      << " best=" << best
                      << " layer_ms=" << stats.anchored_ms_by_size[size] << '\n';
#endif
        }
    }
    else
    {
        // Preserve the old numeric producer order so every producer sees an
        // incumbent at least as strong as before.  A final-layer consumer runs
        // as soon as all of its immediate producer subsets have been built.
        std::vector<int> producers;
        for (int mask = 1; mask < subset_count; ++mask)
            if (popcount[mask] == streamed_producer_size)
                producers.push_back(mask);

        std::vector<int> ready_producers(subset_count);
        std::vector<int> remaining_consumers(subset_count);
        long long live_rows = 0;
        long long live_bytes = 0;
        auto ReleaseProducer = [&](int mask)
        {
            OrdinaryRow& row = anchored[mask];
            if (!row.ready)
                return;
            --live_rows;
            live_bytes -= RowStorageBytes(row);
            std::vector<int>().swap(row.vertices);
            std::vector<double>().swap(row.distances);
            std::vector<std::uint64_t>().swap(row.vertex_bits);
            std::vector<std::uint32_t>().swap(row.rank_before_word);
            std::vector<std::uint64_t>().swap(row.branch_bits);
            row.branch_count = 0;
            row.ready = false;
            row.dense = false;
            row.bitmap = false;
            ++stats.anchored_stream_released_rows;
        };

        for (int producer : producers)
        {
            remaining_consumers[producer] = nonanchor_count - streamed_producer_size;
            ProcessAnchoredMask(producer, streamed_producer_size);
            if (anchored[producer].ready)
            {
                ++live_rows;
                live_bytes += RowStorageBytes(anchored[producer]);
                stats.anchored_stream_peak_rows =
                    std::max(stats.anchored_stream_peak_rows, live_rows);
                stats.anchored_stream_peak_bytes =
                    std::max(stats.anchored_stream_peak_bytes, live_bytes);
            }

            for (int bits = full_mask ^ producer; bits; bits &= bits - 1)
            {
                const int top_mask = producer | (bits & -bits);
                if (++ready_producers[top_mask] != final_anchored_size)
                    continue;
                ProcessAnchoredMask(top_mask, final_anchored_size);
                ++stats.anchored_stream_consumers;
                for (int top_bits = top_mask; top_bits; top_bits &= top_bits - 1)
                {
                    const int consumed = top_mask ^ (top_bits & -top_bits);
                    // No later final-layer mask can read this producer after
                    // its last immediate superset has completed.
                    if (--remaining_consumers[consumed] == 0)
                        ReleaseProducer(consumed);
                }
            }
        }
        if (live_rows != 0 || live_bytes != 0)
            throw std::runtime_error("Anchored stream left an unconsumed producer row.");
        stats.best_after_anchored_size[streamed_producer_size] = best;
        stats.best_after_anchored_size[final_anchored_size] = best;
#ifdef GST_TEST80_PROGRESS
        for (int size = streamed_producer_size; size <= final_anchored_size; ++size)
            std::cerr << "PROGRESS phase=A size=" << size
                      << " masks=" << stats.anchored_masks_by_size[size]
                      << " values=" << stats.anchored_values_by_size[size]
                      << " pops=" << stats.anchored_pops_by_size[size]
                      << " row_bytes=" << stats.anchored_row_bytes_by_size[size]
                      << " completion_ms=" << stats.completion_ms_by_size[size]
                      << " best=" << stats.best_after_anchored_size[size]
                      << " layer_ms=" << stats.anchored_ms_by_size[size] << '\n';
#endif
    }
#endif
    stats.anchored_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - anchored_start).count();

    stats.total_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - total_start).count();
    result.best_weight = best;
    result.feasible = best < fp::kInf / 4;
    return result;
}


}  // namespace gst::methods::test80_anchor_progressive

#undef GST_NOINLINE
