#include "release_v4.h"

#include <algorithm>
#include <cstdint>
#include <functional>
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

namespace gst::methods::release_v4
{
namespace
{
using HeapItem = std::pair<double, int>;
using Heap = std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>>;

int FirstBit(int mask)
{
#ifdef _MSC_VER
    unsigned long index = 0;
    _BitScanForward(&index, static_cast<unsigned long>(mask));
    return static_cast<int>(index);
#else
    return __builtin_ctz(static_cast<unsigned int>(mask));
#endif
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

struct Row
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

struct QueueNode
{
    double key = 0.0;
    double distance = 0.0;
    int vertex = 0;

    bool operator>(const QueueNode& other) const
    {
        if (key != other.key)
            return key > other.key;
        if (distance != other.distance)
            return distance > other.distance;
        return vertex > other.vertex;
    }
};

std::vector<std::vector<double>> GroupDistances(const Graph& graph, const Query& query)
{
    std::vector<std::vector<double>> distance(
        query.groups.size(), std::vector<double>(graph.n + 1, fp::kInf));
    for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
    {
        Heap heap;
        for (int vertex : query.groups[group])
        {
            if (distance[group][vertex] == 0.0)
                continue;
            distance[group][vertex] = 0.0;
            heap.push({0.0, vertex});
        }
        while (!heap.empty())
        {
            const auto [value, vertex] = heap.top();
            heap.pop();
            if (value != distance[group][vertex])
                continue;
            for (const auto& edge : graph.adj[vertex])
            {
                const double next = value + edge.w;
                if (next < distance[group][edge.to])
                {
                    distance[group][edge.to] = next;
                    heap.push({next, edge.to});
                }
            }
        }
    }
    return distance;
}

double RootStarUpper(const std::vector<std::vector<double>>& distance,
                     int vertex_count,
                     int& root)
{
    double best = fp::kInf;
    for (int vertex = 1; vertex <= vertex_count; ++vertex)
    {
        double value = 0.0;
        for (const auto& row : distance)
            value += row[vertex];
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
            return (static_cast<size_t>(mask) * group_count_ + start) * group_count_ +
                   last;
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

// One reusable A* closure owns the dense scratch arrays used by every D/A row.
class RowSearch
{
public:
    RowSearch(const Graph& graph,
              const std::vector<std::vector<double>>& group_distance,
              const TourLowerBound& tour,
              const dual_cut::DualCutPotential& dual,
              double& best)
        : graph_(graph),
          group_distance_(group_distance),
          tour_(tour),
          dual_(dual),
          best_(best),
          distance_(graph.n + 1, fp::kInf),
          heuristic_(graph.n + 1),
          heuristic_stamp_(graph.n + 1)
    {
    }

    void Begin(int remaining_mask)
    {
        ++stamp_;
        remaining_mask_ = remaining_mask;
        touched_.clear();
        settled_.clear();
    }

    void Seed(int vertex, double value)
    {
        if (value >= distance_[vertex] || value + Bound(vertex) > best_)
            return;
        if (distance_[vertex] == fp::kInf)
            touched_.push_back(vertex);
        distance_[vertex] = value;
    }

    template <class Settle>
    const std::vector<int>& Run(Settle&& settle, long long* work = nullptr)
    {
        std::priority_queue<QueueNode,
                            std::vector<QueueNode>,
                            std::greater<QueueNode>> queue;
        for (int vertex : touched_)
        {
            queue.push({distance_[vertex] + Bound(vertex), distance_[vertex], vertex});
            if (work)
                ++*work;
        }

        while (!queue.empty())
        {
            const QueueNode node = queue.top();
            queue.pop();
            if (work)
                ++*work;
            if (node.distance != distance_[node.vertex] || node.key > best_)
                continue;

            settled_.push_back(node.vertex);
            settle(node.vertex, node.distance);
            if (work)
                *work += static_cast<long long>(graph_.adj[node.vertex].size());
            for (const auto& edge : graph_.adj[node.vertex])
            {
                const double next = node.distance + edge.w;
                if (next >= distance_[edge.to] || next + Bound(edge.to) > best_)
                    continue;
                if (distance_[edge.to] == fp::kInf)
                    touched_.push_back(edge.to);
                distance_[edge.to] = next;
                queue.push({next + Bound(edge.to), next, edge.to});
                if (work)
                    ++*work;
            }
        }

        std::sort(settled_.begin(), settled_.end());
        settled_.erase(std::unique(settled_.begin(), settled_.end()), settled_.end());
        return settled_;
    }

    double Bound(int vertex)
    {
        if (heuristic_stamp_[vertex] == stamp_)
            return heuristic_[vertex];
        heuristic_stamp_[vertex] = stamp_;
        double farthest = 0.0;
        for (int bits = remaining_mask_; bits; bits &= bits - 1)
        {
            const int group = FirstBit(bits & -bits);
            farthest = std::max(farthest, group_distance_[group][vertex]);
        }
        heuristic_[vertex] =
            std::max(farthest,
                     std::max(tour_.At(vertex, remaining_mask_, group_distance_),
                              dual_.At(vertex, remaining_mask_)));
        return heuristic_[vertex];
    }

    double Distance(int vertex) const
    {
        return distance_[vertex];
    }

    const std::vector<int>& Touched() const
    {
        return touched_;
    }

    void Reset()
    {
        for (int vertex : touched_)
            distance_[vertex] = fp::kInf;
        touched_.clear();
        settled_.clear();
    }

private:
    const Graph& graph_;
    const std::vector<std::vector<double>>& group_distance_;
    const TourLowerBound& tour_;
    const dual_cut::DualCutPotential& dual_;
    double& best_;
    std::vector<double> distance_;
    std::vector<double> heuristic_;
    std::vector<int> heuristic_stamp_;
    std::vector<int> touched_;
    std::vector<int> settled_;
    int remaining_mask_ = 0;
    int stamp_ = 0;
};

class OrderedAnchorSolver
{
public:
    OrderedAnchorSolver(const Graph& graph, const Query& query)
        : graph_(graph), query_(query), group_count_(static_cast<int>(query.groups.size()))
    {
    }

    SolveResult Run()
    {
        if (!group_count_)
            return {0.0, true};
        if (group_count_ > 16)
            throw std::runtime_error("ReleaseV4 supports group count <= 16.");
        if (!IsQueryFeasible(graph_, query_))
            return {};

        group_distance_ = GroupDistances(graph_, query_);
        best_ = RootStarUpper(group_distance_, graph_.n, root_star_root_);
        if (group_count_ == 1)
            return {0.0, true};

        SelectAnchorAndBuildMasks();
        BuildTourLowerBound();
        dual_.BuildKeepingResidual(graph_, query_, group_distance_, root_star_root_);
        best_ = std::min(best_, dual_.PrimalUpper());

        const auto junction = anchor_junction::BuildUpper(
            graph_, query_, group_distance_, root_star_root_, anchor_group_);
        best_ = std::min(best_, junction.upper);

        ordinary_.resize(subset_count_);
        anchored_.resize(subset_count_);
        split_distance_.assign(graph_.n + 1, fp::kInf);
        RowSearch search(graph_, group_distance_, tour_, dual_, best_);
        search_ = &search;
        BuildOrdinaryRows(junction.anchor_path);
        BuildAnchoredRows();
        search_ = nullptr;
        return {best_, best_ < fp::kInf / 4};
    }

private:
    void SelectAnchorAndBuildMasks()
    {
        anchor_group_ = 0;
        for (int group = 1; group < group_count_; ++group)
        {
            if (group_distance_[group][root_star_root_] >
                group_distance_[anchor_group_][root_star_root_])
                anchor_group_ = group;
        }

        for (int group = 0; group < group_count_; ++group)
            if (group != anchor_group_)
                bit_to_group_.push_back(group);

        nonanchor_count_ = static_cast<int>(bit_to_group_.size());
        subset_count_ = 1 << nonanchor_count_;
        full_mask_ = subset_count_ - 1;
        original_full_mask_ = (1 << group_count_) - 1;
        nonanchor_original_mask_ = original_full_mask_ ^ (1 << anchor_group_);
        half_ = group_count_ / 2;
        popcount_.resize(subset_count_);
        original_mask_.resize(subset_count_);
        for (int mask = 1; mask < subset_count_; ++mask)
        {
            popcount_[mask] = popcount_[mask >> 1] + (mask & 1);
            const int bit = FirstBit(mask & -mask);
            original_mask_[mask] =
                original_mask_[mask ^ (1 << bit)] | (1 << bit_to_group_[bit]);
        }
    }

    void BuildTourLowerBound()
    {
        std::vector<std::vector<double>> metric(
            group_count_, std::vector<double>(group_count_, fp::kInf));
        for (int left = 0; left < group_count_; ++left)
        {
            for (int right = 0; right < group_count_; ++right)
            {
                for (int vertex : query_.groups[right])
                {
                    metric[left][right] =
                        std::min(metric[left][right], group_distance_[left][vertex]);
                }
            }
        }
        tour_.Build(metric);
    }

    bool Available(int mask) const
    {
        return mask && (popcount_[mask] == 1 || ordinary_[mask].ready);
    }

    template <class Use>
    void ForEachValue(int mask, Use&& use) const
    {
        if (popcount_[mask] == 1)
        {
            const auto& values = group_distance_[bit_to_group_[FirstBit(mask)]];
            for (int vertex = 1; vertex <= graph_.n; ++vertex)
                use(vertex, values[vertex]);
            return;
        }
        const Row& row = ordinary_[mask];
        if (row.dense)
        {
            for (int vertex = 1; vertex <= graph_.n; ++vertex)
                if (row.distances[vertex] < fp::kInf)
                    use(vertex, row.distances[vertex]);
            return;
        }
        for (size_t index = 0; index < row.vertices.size(); ++index)
            use(row.vertices[index], row.distances[index]);
    }

    template <class Use>
    void ForEachBranchIndex(const Row& row, Use&& use) const
    {
        for (size_t word = 0; word < row.branch_bits.size(); ++word)
        {
            std::uint64_t bits = row.branch_bits[word];
            while (bits)
            {
                const int bit = FirstBit64(bits);
                use((word << 6) + static_cast<size_t>(bit));
                bits &= bits - 1;
            }
        }
    }

    template <class Use>
    void ForEachBranchValue(int mask, Use&& use) const
    {
        if (popcount_[mask] == 1)
        {
            const auto& values = group_distance_[bit_to_group_[FirstBit(mask)]];
            for (int vertex = 1; vertex <= graph_.n; ++vertex)
                use(vertex, values[vertex]);
            return;
        }
        const Row& row = ordinary_[mask];
        ForEachBranchIndex(row, [&](size_t index)
        {
            const int vertex = row.dense ? static_cast<int>(index) : row.vertices[index];
            use(vertex, row.distances[index]);
        });
    }

    template <class Use>
    void ForEachPair(int left, int right, Use&& use) const
    {
        if (!left || !right)
        {
            ForEachValue(left | right, [&](int vertex, double value)
            {
                use(vertex, left ? value : 0.0, right ? value : 0.0);
            });
            return;
        }
        if (popcount_[left] == 1 || popcount_[right] == 1)
        {
            const int singleton = popcount_[left] == 1 ? left : right;
            const int other = singleton == left ? right : left;
            const auto& singleton_distance =
                group_distance_[bit_to_group_[FirstBit(singleton)]];
            ForEachValue(other, [&](int vertex, double value)
            {
                if (singleton == left)
                    use(vertex, singleton_distance[vertex], value);
                else
                    use(vertex, value, singleton_distance[vertex]);
            });
            return;
        }

        const Row& a = ordinary_[left];
        const Row& b = ordinary_[right];
        if (a.dense && b.dense)
        {
            for (int vertex = 1; vertex <= graph_.n; ++vertex)
                if (a.distances[vertex] < fp::kInf && b.distances[vertex] < fp::kInf)
                    use(vertex, a.distances[vertex], b.distances[vertex]);
            return;
        }
        if (a.dense || b.dense)
        {
            const Row& dense = a.dense ? a : b;
            const Row& sparse = a.dense ? b : a;
            for (size_t index = 0; index < sparse.vertices.size(); ++index)
            {
                const int vertex = sparse.vertices[index];
                if (dense.distances[vertex] >= fp::kInf)
                    continue;
                if (a.dense)
                    use(vertex, dense.distances[vertex], sparse.distances[index]);
                else
                    use(vertex, sparse.distances[index], dense.distances[vertex]);
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
                use(a.vertices[a_index], a.distances[a_index], b.distances[b_index]);
                ++a_index;
                ++b_index;
            }
        }
    }

    template <class Use>
    void ForEachRowBranchPair(const Row& a, const Row& b, Use&& use) const
    {
        if (a.dense)
        {
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
            const long long search =
                static_cast<long long>(b.branch_count) * BinarySearchCost(a.vertices.size());
            if (search < static_cast<long long>(a.vertices.size()))
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
                        use(vertex, a.distances[a_index], b.distances[index]);
                    }
                });
            }
            else
            {
                for (size_t index = 0; index < a.vertices.size(); ++index)
                {
                    const int vertex = a.vertices[index];
                    if (b.IsBranch(vertex))
                        use(vertex, a.distances[index], b.distances[vertex]);
                }
            }
            return;
        }

        const long long linear =
            static_cast<long long>(a.vertices.size() + b.vertices.size());
        const long long search_b =
            static_cast<long long>(b.branch_count) * BinarySearchCost(a.vertices.size());
        const long long search_a =
            static_cast<long long>(a.vertices.size()) * BinarySearchCost(b.vertices.size());
        if (search_b < linear && search_b <= search_a)
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
        if (search_a < linear)
        {
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
                    use(a.vertices[a_index], a.distances[a_index], b.distances[b_index]);
                ++a_index;
                ++b_index;
            }
        }
    }

    template <class Use>
    void ForEachPivotBranchPair(int accumulator, int branch, Use&& use) const
    {
        if (popcount_[branch] == 1)
        {
            const auto& branch_distance =
                group_distance_[bit_to_group_[FirstBit(branch)]];
            ForEachValue(accumulator, [&](int vertex, double value)
            {
                use(vertex, value, branch_distance[vertex]);
            });
            return;
        }
        if (popcount_[accumulator] == 1)
        {
            const auto& accumulator_distance =
                group_distance_[bit_to_group_[FirstBit(accumulator)]];
            ForEachBranchValue(branch, [&](int vertex, double value)
            {
                use(vertex, accumulator_distance[vertex], value);
            });
            return;
        }
        ForEachRowBranchPair(ordinary_[accumulator], ordinary_[branch],
                             std::forward<Use>(use));
    }

    void StoreCurrentRow(Row& row,
                         const std::vector<int>& vertices,
                         const std::vector<int>* branches)
    {
        const size_t sparse_bytes = vertices.size() * (sizeof(int) + sizeof(double));
        const size_t dense_bytes = static_cast<size_t>(graph_.n + 1) * sizeof(double);
        row.dense = dense_bytes < sparse_bytes;
        if (row.dense)
        {
            row.distances.assign(graph_.n + 1, fp::kInf);
            for (int vertex : vertices)
                row.distances[vertex] = search_->Distance(vertex);
        }
        else
        {
            row.vertices = vertices;
            row.distances.reserve(vertices.size());
            for (int vertex : vertices)
                row.distances.push_back(search_->Distance(vertex));
        }

        if (branches)
        {
            const size_t domain = row.dense ? static_cast<size_t>(graph_.n + 1)
                                            : vertices.size();
            row.branch_count = branches->size();
            row.branch_bits.assign((domain + 63) / 64, 0);
            if (row.dense)
            {
                for (int vertex : *branches)
                    row.branch_bits[static_cast<size_t>(vertex) >> 6] |=
                        1ULL << (vertex & 63);
            }
            else
            {
                size_t branch_index = 0;
                for (size_t index = 0;
                     index < vertices.size() && branch_index < branches->size();
                     ++index)
                {
                    if (vertices[index] != (*branches)[branch_index])
                        continue;
                    row.branch_bits[index >> 6] |= 1ULL << (index & 63);
                    ++branch_index;
                }
            }
        }
        row.ready = true;
    }

    template <class Use>
    void ForEachTriple(int first, int second, int third, Use&& use) const
    {
        const int masks[3] = {first, second, third};
        const Row* rows[3] = {nullptr, nullptr, nullptr};
        const std::vector<int>* driver = nullptr;
        for (int index = 0; index < 3; ++index)
        {
            const int mask = masks[index];
            if (mask && popcount_[mask] > 1)
            {
                rows[index] = &ordinary_[mask];
                if (!rows[index]->dense &&
                    (!driver || rows[index]->vertices.size() < driver->size()))
                    driver = &rows[index]->vertices;
            }
        }

        size_t positions[3] = {0, 0, 0};
        auto Visit = [&](int vertex)
        {
            double total = group_distance_[anchor_group_][vertex];
            for (int index = 0; index < 3; ++index)
            {
                const int mask = masks[index];
                if (!mask)
                    continue;
                if (popcount_[mask] == 1)
                {
                    total += group_distance_[bit_to_group_[FirstBit(mask)]][vertex];
                    continue;
                }
                const Row& row = *rows[index];
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
            for (int vertex = 1; vertex <= graph_.n; ++vertex)
                Visit(vertex);
    }

    void EvaluateThreeBlockUpper(int first, int second, int third)
    {
        ForEachTriple(first, second, third, [&](int, double value)
        {
            best_ = std::min(best_, value);
            if (value < witness_value_)
            {
                witness_value_ = value;
                witness_masks_[0] = first;
                witness_masks_[1] = second;
                witness_masks_[2] = third;
            }
        });
    }

    void BuildOrdinaryRows(const std::vector<int>& anchor_path)
    {
        const int pair_row_count = nonanchor_count_ * (nonanchor_count_ - 1) / 2;
        const long long packing_budget =
            static_cast<long long>(group_count_) * (2LL * graph_.m + graph_.n);
        long long pair_work = 0;
        int completed_pairs = 0;
        bool packed = false;
        if (!pair_row_count || half_ < 2)
            dual_.ReleaseResidual();

        const int three_block_size = (nonanchor_count_ + 2) / 3;
        const int quarter_size = (nonanchor_count_ + 3) / 4;
        for (int size = 1; size <= half_; ++size)
        {
            for (int mask = 1; mask < subset_count_; ++mask)
            {
                if (popcount_[mask] != size || size == 1)
                    continue;

                BuildOrdinaryRow(mask, size, pair_work);
                if (size == three_block_size)
                {
                    const int remaining = full_mask_ ^ mask;
                    for (int second = remaining;; second = (second - 1) & remaining)
                    {
                        const int third = remaining ^ second;
                        if (second <= third && popcount_[second] <= size &&
                            popcount_[third] <= size && (!second || Available(second)) &&
                            (!third || Available(third)))
                            EvaluateThreeBlockUpper(mask, second, third);
                        if (!second)
                            break;
                    }
                }

                if (size == 2)
                {
                    ++completed_pairs;
                    if (!packed && completed_pairs < pair_row_count &&
                        pair_work >= packing_budget)
                    {
                        dual_.StrengthenAlongPath(graph_, query_, anchor_path);
                        packed = true;
                    }
                }
            }

            if (size == 2 && !packed)
                dual_.ReleaseResidual();
            if (size == quarter_size && quarter_size < three_block_size)
                ImproveQuarterUpper(size);
            if (size == three_block_size)
                ImproveThreeBlockUpper(size);
        }
    }

    void BuildOrdinaryRow(int mask, int size, long long& pair_work)
    {
        const int remaining_original = original_full_mask_ ^ original_mask_[mask];
        search_->Begin(remaining_original);
        if (size == 2)
        {
            const int first = FirstBit(mask);
            const int second = FirstBit(mask ^ (1 << first));
            const auto& a = group_distance_[bit_to_group_[first]];
            const auto& b = group_distance_[bit_to_group_[second]];
            pair_work += graph_.n;
            for (int vertex = 1; vertex <= graph_.n; ++vertex)
                search_->Seed(vertex, a[vertex] + b[vertex]);
        }
        else
        {
            const int branches = mask ^ (mask & -mask);
            for (int right = branches; right; right = (right - 1) & branches)
            {
                const int left = mask ^ right;
                if (!Available(left) || !Available(right))
                    continue;
                ForEachPivotBranchPair(left, right, [&](int vertex, double a, double b)
                {
                    search_->Seed(vertex, a + b);
                });
            }
        }

        const std::vector<int> seeds = search_->Touched();
        for (int vertex : seeds)
            split_distance_[vertex] = search_->Distance(vertex);
        long long* work = size == 2 ? &pair_work : nullptr;
        const auto& settled = search_->Run([](int, double) {}, work);

        std::vector<int> branches;
        branches.reserve(settled.size());
        for (int vertex : settled)
            if (search_->Distance(vertex) < split_distance_[vertex])
                branches.push_back(vertex);
        StoreCurrentRow(ordinary_[mask], settled, &branches);
        for (int vertex : seeds)
            split_distance_[vertex] = fp::kInf;
        search_->Reset();
    }

    double RootValue(int mask) const
    {
        if (!mask)
            return 0.0;
        if (popcount_[mask] == 1)
            return group_distance_[bit_to_group_[FirstBit(mask)]][root_star_root_];
        const Row& row = ordinary_[mask];
        if (!row.ready)
            return fp::kInf;
        if (row.dense)
            return row.distances[root_star_root_];
        const auto found =
            std::lower_bound(row.vertices.begin(), row.vertices.end(), root_star_root_);
        if (found == row.vertices.end() || *found != root_star_root_)
            return fp::kInf;
        return row.distances[static_cast<size_t>(found - row.vertices.begin())];
    }

    void ImproveQuarterUpper(int block_size)
    {
        std::vector<double> pair_value(subset_count_, fp::kInf);
        std::vector<int> pair_split(subset_count_);
        pair_value[0] = 0.0;
        for (int mask = 1; mask < subset_count_; ++mask)
        {
            if (popcount_[mask] > 2 * block_size)
                continue;
            for (int first = mask;; first = (first - 1) & mask)
            {
                const int second = mask ^ first;
                if (first <= second && popcount_[first] <= block_size &&
                    popcount_[second] <= block_size)
                {
                    const double value = RootValue(first) + RootValue(second);
                    if (value < pair_value[mask])
                    {
                        pair_value[mask] = value;
                        pair_split[mask] = first;
                    }
                }
                if (!first)
                    break;
            }
        }

        const double old_best = best_;
        double witness = fp::kInf;
        int blocks[4] = {0, 0, 0, 0};
        for (int left = 1; left < full_mask_; ++left)
        {
            const int right = full_mask_ ^ left;
            if (left >= right || popcount_[left] > 2 * block_size ||
                popcount_[right] > 2 * block_size)
                continue;
            const double value = group_distance_[anchor_group_][root_star_root_] +
                                 pair_value[left] + pair_value[right];
            if (value < best_)
                best_ = value;
            if (value < witness)
            {
                witness = value;
                blocks[0] = pair_split[left];
                blocks[1] = left ^ blocks[0];
                blocks[2] = pair_split[right];
                blocks[3] = right ^ blocks[2];
            }
        }
        if (best_ < old_best && witness < fp::kInf)
            LiftQuarterWitness(blocks);
    }

    void LiftQuarterWitness(const int blocks[4])
    {
        std::vector<double> target(graph_.n + 1, fp::kInf);
        std::vector<int> target_vertices;
        for (int choice = 0; choice < 4; ++choice)
        {
            const int anchor_side = blocks[choice];
            const int first = blocks[(choice + 1) % 4];
            const int second = blocks[(choice + 2) % 4];
            const int third = blocks[(choice + 3) % 4];
            ForEachTriple(first, second, third, [&](int vertex, double value)
            {
                if (target[vertex] == fp::kInf)
                    target_vertices.push_back(vertex);
                target[vertex] = std::min(
                    target[vertex], value - group_distance_[anchor_group_][vertex]);
            });

            const int remaining_original =
                nonanchor_original_mask_ ^ original_mask_[anchor_side];
            search_->Begin(remaining_original);
            SeedAnchoredSide(anchor_side);
            search_->Run([&](int vertex, double value)
            {
                if (target[vertex] < fp::kInf)
                    best_ = std::min(best_, value + target[vertex]);
            });
            search_->Reset();
            for (int vertex : target_vertices)
                target[vertex] = fp::kInf;
            target_vertices.clear();
        }
    }

    void ImproveThreeBlockUpper(int block_size)
    {
        if (block_size == 1)
        {
            for (int first = full_mask_;; first = (first - 1) & full_mask_)
            {
                const int remaining = full_mask_ ^ first;
                for (int second = remaining;; second = (second - 1) & remaining)
                {
                    const int third = remaining ^ second;
                    if (first <= second && second <= third &&
                        popcount_[first] <= block_size &&
                        popcount_[second] <= block_size &&
                        popcount_[third] <= block_size &&
                        (!first || Available(first)) &&
                        (!second || Available(second)) &&
                        (!third || Available(third)))
                        EvaluateThreeBlockUpper(first, second, third);
                    if (!second)
                        break;
                }
                if (!first)
                    break;
            }
        }

        if (witness_value_ >= fp::kInf)
            return;
        std::vector<double> target(graph_.n + 1, fp::kInf);
        std::vector<int> target_vertices;
        for (int choice = 0; choice < 3; ++choice)
        {
            const int anchor_side = witness_masks_[choice];
            const int left = witness_masks_[(choice + 1) % 3];
            const int right = witness_masks_[(choice + 2) % 3];
            ForEachPair(left, right, [&](int vertex, double a, double b)
            {
                if (target[vertex] == fp::kInf)
                    target_vertices.push_back(vertex);
                target[vertex] = std::min(target[vertex], a + b);
            });

            const int remaining_original =
                nonanchor_original_mask_ ^ original_mask_[anchor_side];
            search_->Begin(remaining_original);
            SeedAnchoredSide(anchor_side);
            search_->Run([&](int vertex, double value)
            {
                if (target[vertex] < fp::kInf)
                    best_ = std::min(best_, value + target[vertex]);
            });
            search_->Reset();
            for (int vertex : target_vertices)
                target[vertex] = fp::kInf;
            target_vertices.clear();
        }
    }

    void SeedAnchoredSide(int mask)
    {
        if (!mask)
        {
            for (int vertex = 1; vertex <= graph_.n; ++vertex)
                search_->Seed(vertex, group_distance_[anchor_group_][vertex]);
            return;
        }
        ForEachValue(mask, [&](int vertex, double value)
        {
            search_->Seed(vertex, group_distance_[anchor_group_][vertex] + value);
        });
    }

    template <class Use>
    void ForEachAnchoredValue(int mask, Use&& use) const
    {
        if (!mask)
        {
            const auto& values = group_distance_[anchor_group_];
            for (int vertex = 1; vertex <= graph_.n; ++vertex)
                use(vertex, values[vertex]);
            return;
        }
        const Row& row = anchored_[mask];
        if (row.dense)
        {
            for (int vertex = 1; vertex <= graph_.n; ++vertex)
                if (row.distances[vertex] < fp::kInf)
                    use(vertex, row.distances[vertex]);
            return;
        }
        for (size_t index = 0; index < row.vertices.size(); ++index)
            use(row.vertices[index], row.distances[index]);
    }

    template <class Use>
    void ForEachAnchoredSum(int anchor_side, int ordinary_side, Use&& use) const
    {
        if (!anchor_side)
        {
            const auto& anchor_distance = group_distance_[anchor_group_];
            ForEachBranchValue(ordinary_side, [&](int vertex, double value)
            {
                use(vertex, anchor_distance[vertex] + value);
            });
            return;
        }
        if (popcount_[ordinary_side] == 1)
        {
            const auto& singleton =
                group_distance_[bit_to_group_[FirstBit(ordinary_side)]];
            ForEachAnchoredValue(anchor_side, [&](int vertex, double value)
            {
                use(vertex, value + singleton[vertex]);
            });
            return;
        }

        ForEachRowBranchPair(anchored_[anchor_side], ordinary_[ordinary_side],
                             [&](int vertex, double a, double d)
        {
            use(vertex, a + d);
        });
    }

    template <class AnchoredValue>
    void Complete(int mask,
                  const std::vector<int>& roots,
                  AnchoredValue&& anchored_value)
    {
        const int remaining = full_mask_ ^ mask;
        for (int left = remaining;; left = (left - 1) & remaining)
        {
            const int right = remaining ^ left;
            if (left <= right && popcount_[left] <= half_ && popcount_[right] <= half_ &&
                (!left || Available(left)) && (!right || Available(right)))
            {
                const Row* left_row =
                    left && popcount_[left] > 1 ? &ordinary_[left] : nullptr;
                const Row* right_row =
                    right && popcount_[right] > 1 ? &ordinary_[right] : nullptr;
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
                    while (root_index < roots.size() && roots[root_index] < vertex)
                        ++root_index;
                    if (root_index == roots.size() || roots[root_index] != vertex)
                        continue;

                    double a = 0.0;
                    if (left)
                    {
                        if (popcount_[left] == 1)
                            a = group_distance_[bit_to_group_[FirstBit(left)]][vertex];
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
                        if (popcount_[right] == 1)
                            b = group_distance_[bit_to_group_[FirstBit(right)]][vertex];
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
                    best_ = std::min(best_, anchored_value(vertex) + a + b);
                }
            }
            if (!left)
                break;
        }
    }

    void BuildAnchoredRows()
    {
        std::vector<int> all_vertices(graph_.n);
        std::iota(all_vertices.begin(), all_vertices.end(), 1);
        Complete(0, all_vertices, [&](int vertex)
        {
            return group_distance_[anchor_group_][vertex];
        });

        for (int size = 1; size <= half_ - 1; ++size)
        {
            for (int mask = 1; mask < subset_count_; ++mask)
            {
                if (popcount_[mask] != size)
                    continue;
                BuildAnchoredRow(mask, size);
            }
        }
    }

    void BuildAnchoredRow(int mask, int size)
    {
        const int remaining_nonanchor = full_mask_ ^ mask;
        const int remaining_original = nonanchor_original_mask_ ^ original_mask_[mask];
        search_->Begin(remaining_original);
        for (int ordinary_side = mask;
             ordinary_side;
             ordinary_side = (ordinary_side - 1) & mask)
        {
            const int anchor_side = mask ^ ordinary_side;
            if (!Available(ordinary_side) ||
                (anchor_side && !anchored_[anchor_side].ready))
                continue;
            ForEachAnchoredSum(anchor_side, ordinary_side, [&](int vertex, double value)
            {
                search_->Seed(vertex, value);
            });
        }
        if (search_->Touched().empty())
        {
            search_->Reset();
            return;
        }

        const auto& settled = search_->Run([&](int vertex, double value)
        {
            double star = value;
            for (int bits = remaining_nonanchor; bits; bits &= bits - 1)
                star += group_distance_[bit_to_group_[FirstBit(bits & -bits)]][vertex];
            best_ = std::min(best_, star);
        });
        Complete(mask, settled, [&](int vertex)
        {
            return search_->Distance(vertex);
        });

        std::vector<int> retained;
        retained.reserve(settled.size());
        for (int vertex : settled)
        {
            if (search_->Distance(vertex) + search_->Bound(vertex) <= best_)
                retained.push_back(vertex);
        }
        if (size < half_ - 1)
            StoreCurrentRow(anchored_[mask], retained, nullptr);
        search_->Reset();
    }

    const Graph& graph_;
    const Query& query_;
    int group_count_ = 0;
    int anchor_group_ = 0;
    int root_star_root_ = 1;
    int nonanchor_count_ = 0;
    int subset_count_ = 0;
    int full_mask_ = 0;
    int original_full_mask_ = 0;
    int nonanchor_original_mask_ = 0;
    int half_ = 0;
    double best_ = fp::kInf;
    double witness_value_ = fp::kInf;
    int witness_masks_[3] = {0, 0, 0};
    std::vector<int> bit_to_group_;
    std::vector<int> popcount_;
    std::vector<int> original_mask_;
    std::vector<std::vector<double>> group_distance_;
    std::vector<double> split_distance_;
    std::vector<Row> ordinary_;
    std::vector<Row> anchored_;
    TourLowerBound tour_;
    dual_cut::DualCutPotential dual_;
    RowSearch* search_ = nullptr;
};

}  // namespace

SolveResult SolveOneQuery(const Graph& graph, const Query& query)
{
    return OrderedAnchorSolver(graph, query).Run();
}

}  // namespace gst::methods::release_v4
