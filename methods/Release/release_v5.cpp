#include "release_v5.h"

#include <algorithm>
#include <array>
#include <cstdint>
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

#if defined(_MSC_VER)
#define GST_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define GST_NOINLINE __attribute__((noinline))
#else
#define GST_NOINLINE
#endif

namespace gst::methods::release_v5
{
namespace
{
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
GST_NOINLINE void ForEachBitmapBranchPair(const OrdinaryRow& values,
                                          const OrdinaryRow& branches,
                                          Use&& use)
{
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
            bits &= bits - 1;
        }
    }
}

GST_NOINLINE double CompleteBitmapIntersection(
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
            if (anchored_values[vertex] + left_minimum + right_minimum > best)
            {
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
                anchored_values[vertex] + left_value + right_minimum > best)
            {
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
            best = std::min(best,
                            anchored_values[vertex] + left_value + right_value);
            bits &= bits - 1;
        }
    }
    return best;
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

double ComputeAnchorTreeFacility(
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
    return anchor_path_cost + dp[root][subset_count - 1];
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
    const int group_count = static_cast<int>(query.groups.size());

    if (!group_count)
        return {0.0, true};
    if (group_count > 16)
        throw std::runtime_error("ReleaseV5 supports group count <= 16.");
    if (!IsQueryFeasible(graph, query))
        return result;

    // Shared preprocessing: group distances, incumbent, anchor, and lower bounds.
    const std::vector<std::vector<double>> group_distance = GroupDistances(graph, query);

    int root_star_root = 1;
    double best = RootStarUpper(group_distance, graph.n, root_star_root);
    if (group_count == 1)
    {
        result.best_weight = 0.0;
        result.feasible = true;
        return result;
    }
    int anchor_group = 0;
    for (int group = 1; group < group_count; ++group)
        if (group_distance[group][root_star_root] >
            group_distance[anchor_group][root_star_root])
            anchor_group = group;
    const int half = group_count / 2;

    std::vector<std::vector<double>> group_metric(
        group_count, std::vector<double>(group_count, fp::kInf));
    for (int left = 0; left < group_count; ++left)
        for (int right = 0; right < group_count; ++right)
            for (int vertex : query.groups[right])
                group_metric[left][right] =
                    std::min(group_metric[left][right], group_distance[left][vertex]);
    TourLowerBound lower_bound;
    lower_bound.Build(group_metric);

    std::vector<int> bit_to_group;
    bit_to_group.reserve(group_count - 1);
    for (int group = 0; group < group_count; ++group)
        if (group != anchor_group)
            bit_to_group.push_back(group);

    const int nonanchor_count = static_cast<int>(bit_to_group.size());
    const int subset_count = 1 << nonanchor_count;
    const int full_mask = subset_count - 1;
    const int original_full_mask = (1 << group_count) - 1;
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

    gst::methods::dual_cut::DualCutPotential dual;
    dual.BuildKeepingResidualChangedArcs(
        graph, query, group_distance, root_star_root);
    best = std::min(best, dual.PrimalUpper());
    auto DualAt = [&](int vertex, int mask) { return dual.At(vertex, mask); };
    auto DualGroupAt = [&](int vertex, int group)
    {
        return dual.GroupAt(vertex, group);
    };
    std::vector<double> anchor_path_distance;
    gst::methods::anchor_junction::AnchorTree anchor_tree;
    const auto junction = gst::methods::anchor_junction::BuildUpper(
        graph,
        query,
        group_distance,
        root_star_root,
        anchor_group,
        &anchor_path_distance,
        &anchor_tree);
    best = std::min(best, junction.upper);

    auto FutureBound = [&](int vertex, int remaining_original)
    {
        return std::max(lower_bound.At(vertex, remaining_original, group_distance),
                        DualAt(vertex, remaining_original));
    };
    // D(S,v): anchor-free rows, generated offline in increasing mask size.
    std::vector<OrdinaryRow> ordinary(subset_count);
    std::vector<double> ordinary_minimum(subset_count, fp::kInf);
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
    auto EvaluateAnchorFacility = [&]()
    {
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
                anchor_facility[remaining] = std::min(
                    anchor_facility[remaining],
                    anchor_facility[remaining ^ block] + anchor_attachment[block]);
            }
        }
        const double facility_upper =
            group_distance[anchor_group][root_star_root] +
            anchor_facility[full_mask];
        best = std::min(best, facility_upper);
    };
    auto Available = [&](int mask)
    {
        return mask && (popcount[mask] == 1 || ordinary[mask].ready);
    };
    long long anchor_tree_rent = 0;
    long long tree_merge_probes = 1;
    for (int bit = 0; bit < nonanchor_count; ++bit)
        tree_merge_probes *= 3;
    const long long tree_local_probes = (tree_merge_probes - 1) / 2;
    const long long anchor_tree_buy_work =
        static_cast<long long>(anchor_tree.vertices.size()) *
        (tree_local_probes + tree_merge_probes);
    auto EvaluateAnchorTreeFacility = [&]()
    {
        best = std::min(
            best,
            ComputeAnchorTreeFacility(
                anchor_tree,
                subset_count,
                popcount,
                bit_to_group,
                group_distance,
                ordinary,
                group_distance[anchor_group][root_star_root]));
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
            ForEachBitmapBranchPair(a, b, use);
            return;
        }
        if (a.dense || a.bitmap)
        {
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
    constexpr unsigned char kCheapReady = 2;
    constexpr unsigned char kTourReady = 3;
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
        double attachment = fp::kInf;
        auto UpdateAttachment = [&](int vertex, double tree_cost)
        {
            const double candidate =
                tree_cost + anchor_path_distance[vertex];
            attachment = std::min(attachment, candidate);
        };
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
                UpdateAttachment(vertex, row_distance[vertex]);
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
                UpdateAttachment(vertex, row_distance[vertex]);
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
                UpdateAttachment(vertex, row_distance[vertex]);
            }
        }
        anchor_attachment[mask] = attachment;
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
    const int quarter_block_limit = (nonanchor_count + 3) / 4;
    double witness_value = fp::kInf;
    int witness_masks[3] = {0, 0, 0};
    auto EvaluateEarlyPartition = [&](int first, int second, int third)
    {
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
    long long pair_work = 0;
    const long long packing_budget =
        static_cast<long long>(group_count) * (2LL * graph.m + graph.n);
    if (!pair_row_count || half < 2)
        dual.ReleaseResidual();

    auto BuildPacking = [&]()
    {
        dual.StrengthenAlongPath(graph, query, junction.anchor_path);
        packing_built = true;
    };
    // Ordinary phase: merge root-irreducible branches, then close each row on G.
    for (int size = 1; size <= half; ++size)
    {
        long long layer_queue_work = 0;
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (popcount[mask] != size)
                continue;
            if (size == 1)
            {
                continue;
            }

            ++stamp;
            touched.clear();
            settled.clear();
            const int remaining_original = original_full_mask ^ original_mask[mask];
            auto CutH = [&](int vertex)
            {
                if (heuristic_stamp[vertex] != stamp)
                {
                    heuristic_stamp[vertex] = stamp;
                    heuristic[vertex] = DualAt(vertex, remaining_original);
                    heuristic_state[vertex] = kCutReady;
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
                    heuristic_state[vertex] = kCheapReady;
                }
                return heuristic[vertex];
            };
            auto H = [&](int vertex)
            {
                const double cheap = CheapH(vertex);
                if (heuristic_state[vertex] != kTourReady)
                {
                    const double tour =
                        lower_bound.At(vertex, remaining_original, group_distance);
                    heuristic[vertex] = std::max(cheap, tour);
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
                if (value >= row_distance[vertex])
                {
                    return;
                }
                if (!CanImprove(vertex, value))
                {
                    return;
                }
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
                pair_work += graph.n;
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
            for (int vertex : touched)
            {
                queue.push({row_distance[vertex] + H(vertex), row_distance[vertex], vertex});
                if (size == 2)
                    ++pair_work;
            }
            while (!queue.empty())
            {
                const RowHeapNode node = queue.top();
                queue.pop();
                ++layer_queue_work;
                if (size == 2)
                    ++pair_work;
                if (node.distance != row_distance[node.vertex])
                {
                    continue;
                }
                if (node.key > best)
                {
                    continue;
                }
                settled.push_back(node.vertex);
                if (size == 2)
                    pair_work += graph.adj[node.vertex].size();
                for (const auto& edge : graph.adj[node.vertex])
                {
                    const double next = node.distance + edge.w;
                    ++layer_queue_work;
                    if (next >= row_distance[edge.to])
                    {
                        continue;
                    }
                    if (!CanImprove(edge.to, next))
                    {
                        continue;
                    }
                    if (row_distance[edge.to] == fp::kInf)
                        touched.push_back(edge.to);
                    row_distance[edge.to] = next;
                    queue.push({next + H(edge.to), next, edge.to});
                    if (size == 2)
                        ++pair_work;
                }
            }

            std::sort(settled.begin(), settled.end());
            settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
            branch_vertices.clear();
            for (int vertex : settled)
                if (row_distance[vertex] < split_distance[vertex])
                    branch_vertices.push_back(vertex);
            StoreOrdinaryRow(mask, settled, branch_vertices);
            if (size == 2)
            {
                const int first_bit = FirstBit(mask);
                const int second_bit = FirstBit(mask ^ (1 << first_bit));
                const int first_group = std::min(bit_to_group[first_bit],
                                                 bit_to_group[second_bit]);
                const int second_group = std::max(bit_to_group[first_bit],
                                                  bit_to_group[second_bit]);
            }
            if (size == half)
            {
                const int complement = full_mask ^ mask;
                const int complement_size = popcount[complement];
                const bool closes_partition =
                    complement_size < size ||
                    (complement_size == size && complement < mask);
                if (closes_partition && Available(complement))
                {
                    const int first = std::min(mask, complement);
                    const int second = std::max(mask, complement);
                    ForEachTripleValue(first, second, 0, [&](int, double value)
                    {
                        if (value < best)
                        {
                            best = value;
                        }
                    });
                }
            }
            if (size == early_block_limit)
            {
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
                    pair_work >= packing_budget)
                    BuildPacking();
            }
        }

        if (size == 2 && !packing_built)
            dual.ReleaseResidual();

        if (size == quarter_block_limit && quarter_block_limit < early_block_limit)
        {
            const double quarter_upper_before = best;
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
                if (row.bitmap)
                    return BitmapValue(row, root_star_root);
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
                for (int first = mask;; first = (first - 1) & mask)
                {
                    const int second = mask ^ first;
                    if (first <= second && popcount[first] <= size &&
                        popcount[second] <= size)
                    {
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
            if (best < quarter_upper_before && quarter_witness < fp::kInf)
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
                        if (node.distance != row_distance[node.vertex] || node.key > best)
                            continue;
                        if (quarter_target[node.vertex] < fp::kInf)
                            best = std::min(best,
                                            node.distance + quarter_target[node.vertex]);
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
        }
        if (size == early_block_limit)
        {
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
                for (int vertex : touched)
                    row_distance[vertex] = fp::kInf;
                for (int vertex : witness_target_vertices)
                    witness_target[vertex] = fp::kInf;

            }
        }
        EvaluateAnchorFacility();
        // A tighter incumbent cannot avoid enumerating the current layer's
        // merge candidates.  It can avoid queue work and graph scans, so only
        // those incumbent-sensitive operations pay for another tree DP.
        anchor_tree_rent += layer_queue_work;
        if (anchor_tree_rent >= anchor_tree_buy_work)
        {
            EvaluateAnchorTreeFacility();
            anchor_tree_rent = 0;
        }
    }
    std::vector<double>().swap(anchor_path_distance);
    std::vector<double>().swap(anchor_attachment);
    std::vector<double>().swap(anchor_facility);
    // A(S,v): forward anchor-containing rows below the balanced adjoint cut.
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
            const auto& anchor_distance = group_distance[anchor_group];
            ForEachBranchValue(ordinary_side, [&](int vertex, double value)
            {
                use(vertex, anchor_distance[vertex] + value);
            });
            return;
        }
        if (popcount[ordinary_side] == 1)
        {
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
            ForEachBitmapBranchPair(a, d, [&](int vertex,
                                              double a_value,
                                              double d_value)
            {
                use(vertex, a_value + d_value);
            });
            return;
        }
        if (a.dense || a.bitmap)
        {
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

    std::vector<std::uint64_t> completion_root_bits(
        (static_cast<size_t>(graph.n + 1) + 63) / 64);
    auto CompleteRoots = [&](int mask,
                             const std::vector<int>& roots,
                             double anchored_minimum)
    {
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
        };
        auto OrdinaryMinimum = [&](int side)
        {
            return !side || popcount[side] == 1 ? 0.0 : ordinary_minimum[side];
        };
        const int remaining = full_mask ^ mask;
        for (int left = remaining;; left = (left - 1) & remaining)
        {
            const int right = remaining ^ left;
            if (left <= right && popcount[left] <= half &&
                popcount[right] <= half &&
                (!left || Available(left)) && (!right || Available(right)))
            {
                const double left_minimum = OrdinaryMinimum(left);
                const double right_minimum = OrdinaryMinimum(right);
                if (anchored_minimum + left_minimum + right_minimum <= best)
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
                        const DenseRow* left_singleton =
                            left && popcount[left] == 1
                                ? &group_distance[bit_to_group[FirstBit(left)]]
                                : nullptr;
                        const DenseRow* right_singleton =
                            right && popcount[right] == 1
                                ? &group_distance[bit_to_group[FirstBit(right)]]
                                : nullptr;
                        best = CompleteBitmapIntersection(completion_root_bits,
                                                          left_bitmap,
                                                          right_bitmap,
                                                          left_singleton,
                                                          right_singleton,
                                                          row_distance,
                                                          left_minimum,
                                                          right_minimum,
                                                          best);
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
                            while (root_index < roots.size() && roots[root_index] < vertex)
                                ++root_index;
                            if (root_index == roots.size() || roots[root_index] != vertex)
                                return;
                            if (row_distance[vertex] + side_minimum > best)
                            {
                                return;
                            }
                            if (left && right && driver_mask == left &&
                                row_distance[vertex] + driver_value + right_minimum > best)
                            {
                                return;
                            }
                            if (left && right && driver_mask == right &&
                                row_distance[vertex] + left_minimum + driver_value > best)
                            {
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
    };

    std::vector<int> all_vertices(graph.n);
    std::iota(all_vertices.begin(), all_vertices.end(), 1);
    for (int vertex : all_vertices)
        row_distance[vertex] = group_distance[anchor_group][vertex];
    if (half < 2)
        CompleteRoots(0, all_vertices, 0.0);
    for (int vertex : all_vertices)
        row_distance[vertex] = fp::kInf;

    auto ProcessAnchoredMask = [&](int mask, int size)
    {
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
                heuristic_state[vertex] = kCheapReady;
            }
            return heuristic[vertex];
        };
        auto BaseH = [&](int vertex)
        {
            const double cheap = CheapBaseH(vertex);
            if (heuristic_state[vertex] != kTourReady)
            {
                const double tour =
                    lower_bound.At(vertex, remaining_original, group_distance);
                heuristic[vertex] = std::max(cheap, tour);
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
            if (value >= row_distance[vertex])
            {
                return;
            }
            if (!CanImprove(vertex, value))
            {
                return;
            }
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
                Set(vertex, value);
            });
        }

        if (touched.empty())
        {
            return;
        }

        std::priority_queue<RowHeapNode,
                            std::vector<RowHeapNode>,
                            std::greater<RowHeapNode>> queue;
        for (int vertex : touched)
            queue.push(
                {row_distance[vertex] + BaseH(vertex), row_distance[vertex], vertex});

        while (!queue.empty())
        {
            const RowHeapNode node = queue.top();
            queue.pop();
            if (node.distance != row_distance[node.vertex])
            {
                continue;
            }
            if (node.key > best)
            {
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
                if (next >= row_distance[edge.to])
                {
                    continue;
                }
                if (!CanImprove(edge.to, next))
                {
                    continue;
                }
                if (row_distance[edge.to] == fp::kInf)
                {
                    touched.push_back(edge.to);
                }
                row_distance[edge.to] = next;
                queue.push({next + BaseH(edge.to), next, edge.to});
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
        if (size < half - 1)
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
        }
        for (int vertex : touched)
            row_distance[vertex] = fp::kInf;
    };

    const int final_anchored_size = half - 1;
    const int regular_last_size = std::max(0, final_anchored_size / 2);
    // The lower A rows are materialized; the upper rows are evaluated adjointly.
    for (int size = 1; size <= regular_last_size; ++size)
    {
        for (int mask = 1; mask < subset_count; ++mask)
            if (popcount[mask] == size)
                ProcessAnchoredMask(mask, size);
    }

    // H(S,v): adjoint rows replacing the upper half of the forward A lattice.
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
    struct TransposedValue
    {
        int mask = 0;
        double value = fp::kInf;
        double reduced = fp::kInf;
    };
    // Transpose every A+D+D terminal by root before the descending H pass.
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
    // Stream ordinary rows in 64-root batches and emit sparse terminal events.
    for (size_t word = 0; word < word_count; ++word)
    {
        for (auto& values : values_by_offset)
            values.clear();
        const int first_vertex = std::max(1, static_cast<int>(word << 6));
        const int end_vertex =
            std::min(graph.n + 1, static_cast<int>((word + 1) << 6));
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (popcount[mask] > half || !Available(mask))
                continue;
            auto Add = [&](int vertex, double value)
            {
                values_by_offset[vertex & 63].push_back({mask, value, 0.0});
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
                        if (values[left].mask & values[right].mask)
                            continue;
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
                for (const TransposedValue& left : values)
                {
                    const int complement = full_mask ^ left.mask;
                    for (int right = complement; right;
                         right = (right - 1) & complement)
                    {
                        if (right <= left.mask ||
                            vertex_value_stamp[right] != value_epoch ||
                            left.reduced + vertex_reduced[right] > reduced_budget)
                            continue;
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
                }
                terminal_best[target] = fp::kInf;
            }
        }
    }
    auto EvaluateBackwardBoundary = [&](int successor)
    {
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
                const double candidate = anchored_value + value;
                if (candidate < best)
                {
                    best = candidate;
                }
            });
        }
    };

    // Descend through the complete upper A dependency DAG using adjoint rows H.
    for (int size = final_anchored_size; size > regular_last_size; --size)
    {
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (popcount[mask] != size)
                continue;
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
            auto Set = [&](int vertex, double value)
            {
                if (value >= row_distance[vertex] ||
                    value + PrefixLower(vertex) > best)
                    return;
                if (row_distance[vertex] == fp::kInf)
                    touched.push_back(vertex);
                row_distance[vertex] = value;
            };

            for (size_t index = 0; index < terminal_vertices[mask].size(); ++index)
                Set(terminal_vertices[mask][index],
                    terminal_distances[mask][index]);

            const int outside = full_mask ^ mask;
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
                    Set(vertex, value);
                });
            }

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
            EvaluateBackwardBoundary(mask);
            for (int vertex : touched)
                row_distance[vertex] = fp::kInf;
        }
    }

    result.best_weight = best;
    result.feasible = best < fp::kInf / 4;
    return result;
}

}  // namespace gst::methods::release_v5

#undef GST_NOINLINE
