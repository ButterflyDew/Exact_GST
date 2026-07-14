#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "float_compare.h"
#include "graph_io.h"
#include "methods/Test/test19_future_bounds.h"
#include "query_io.h"

namespace
{
using Clock = std::chrono::steady_clock;
using Item = std::pair<double, int>;
using Heap = std::priority_queue<Item, std::vector<Item>, std::greater<Item>>;
using Matrix = std::vector<std::vector<double>>;
using ParentForest = std::vector<std::vector<int>>;
using SettleOrders = std::vector<std::vector<int>>;

int FirstBit(int mask)
{
    int bit = 0;
    while ((mask & (1 << bit)) == 0)
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

struct GroupData
{
    Matrix distance;
    ParentForest parent;
    SettleOrders order;
};

GroupData BuildGroupData(const gst::Graph& graph, const gst::Query& query)
{
    const int g = static_cast<int>(query.groups.size());
    GroupData data;
    data.distance.assign(g, std::vector<double>(graph.n + 1, gst::fp::kInf));
    data.parent.assign(g, std::vector<int>(graph.n + 1, -1));
    data.order.resize(g);
    for (int group = 0; group < g; ++group)
    {
        Heap heap;
        std::vector<char> settled(graph.n + 1);
        for (int vertex : query.groups[group])
        {
            data.distance[group][vertex] = 0.0;
            data.parent[group][vertex] = 0;
            heap.push({0.0, vertex});
        }
        while (!heap.empty())
        {
            const auto [distance, vertex] = heap.top();
            heap.pop();
            if (distance != data.distance[group][vertex] || settled[vertex])
                continue;
            settled[vertex] = 1;
            data.order[group].push_back(vertex);
            for (const auto& edge : graph.adj[vertex])
            {
                const double next = distance + edge.w;
                if (next < data.distance[group][edge.to])
                {
                    data.distance[group][edge.to] = next;
                    data.parent[group][edge.to] = vertex;
                    heap.push({next, edge.to});
                }
            }
        }
    }
    return data;
}

Matrix BuildGroupMetric(const gst::Query& query, const Matrix& group_distance)
{
    const int g = static_cast<int>(query.groups.size());
    Matrix metric(g, std::vector<double>(g, gst::fp::kInf));
    for (int first = 0; first < g; ++first)
        for (int second = 0; second < g; ++second)
            for (int vertex : query.groups[second])
                metric[first][second] =
                    std::min(metric[first][second], group_distance[first][vertex]);
    return metric;
}

struct HalfRows
{
    int half = 0;
    int full_mask = 0;
    std::vector<int> group_to_bit;
    std::vector<std::vector<double>> row;
    long long seed_probes = 0;
    long long queue_pops = 0;
    double milliseconds = 0.0;
};

HalfRows BuildHalfRows(const gst::Graph& graph, const Matrix& group_distance)
{
    const auto begin = Clock::now();
    const int g = static_cast<int>(group_distance.size());
    const int subset_count = 1 << g;
    HalfRows result;
    result.half = g / 2;
    result.full_mask = subset_count - 1;
    result.group_to_bit.assign(g, -1);
    result.row.resize(subset_count);

    for (int group = 0; group < g; ++group)
        result.group_to_bit[group] = group;

    std::vector<int> popcount(subset_count);
    for (int mask = 1; mask < subset_count; ++mask)
        popcount[mask] = popcount[mask >> 1] + (mask & 1);
    for (int bit = 0; bit < g; ++bit)
        result.row[1 << bit] = group_distance[bit];

    for (int size = 2; size <= result.half; ++size)
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (popcount[mask] != size)
                continue;
            std::vector<double>& distance = result.row[mask];
            distance.assign(graph.n + 1, gst::fp::kInf);
            const int pivot = FirstBit(mask);
            const int branch_domain = mask ^ (1 << pivot);
            for (int right = branch_domain; right; right = (right - 1) & branch_domain)
            {
                const int left = mask ^ right;
                const auto& left_row = result.row[left];
                const auto& right_row = result.row[right];
                for (int vertex = 1; vertex <= graph.n; ++vertex)
                {
                    distance[vertex] =
                        std::min(distance[vertex], left_row[vertex] + right_row[vertex]);
                    ++result.seed_probes;
                }
            }

            Heap heap;
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                heap.push({distance[vertex], vertex});
            while (!heap.empty())
            {
                const auto [value, vertex] = heap.top();
                heap.pop();
                ++result.queue_pops;
                if (value != distance[vertex])
                    continue;
                for (const auto& edge : graph.adj[vertex])
                {
                    const double next = value + edge.w;
                    if (next < distance[edge.to])
                    {
                        distance[edge.to] = next;
                        heap.push({next, edge.to});
                    }
                }
            }
        }
    result.milliseconds =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return result;
}

void BuildPathMinimum(const std::vector<double>& values,
                      const std::vector<int>& parent,
                      const std::vector<int>& order,
                      std::vector<double>& output)
{
    for (int vertex : order)
    {
        output[vertex] = values[vertex];
        if (parent[vertex] > 0)
            output[vertex] = std::min(output[vertex], output[parent[vertex]]);
    }
}

struct PairResult
{
    long long settled = 0;
    long long pushes = 0;
    long long profile_columns = 0;
    long long profile_values = 0;
    long long completion_probes = 0;
    double pair_upper = gst::fp::kInf;
    double anchor_skyline_pair_upper = gst::fp::kInf;
    int anchor_skyline_best_root = 0;
    int anchor_skyline_best_left = 0;
    int anchor_skyline_best_right = 0;
    double anchored_pair_upper = gst::fp::kInf;
    double search_ms = 0.0;
    double profile_ms = 0.0;
    std::vector<double> settled_costs;
    struct PlanResult
    {
        int pair = 0;
        int left = 0;
        int right = 0;
        double best = gst::fp::kInf;
    };
    std::vector<PlanResult> plans;
};

PairResult ProbePair(const gst::Graph& graph,
                     int first,
                     int second,
                     int anchor_group,
                     int original_full_mask,
                     double known_optimum,
                     const GroupData& group,
                     const gst::methods::test19::MetricTspLowerBound& lower,
                     const HalfRows& half_rows)
{
    const auto search_begin = Clock::now();
    const int remaining_original =
        original_full_mask ^ (1 << first) ^ (1 << second);
    std::vector<double> distance(graph.n + 1, gst::fp::kInf);
    std::vector<double> heuristic(graph.n + 1);
    std::vector<int> parent(graph.n + 1, -1);
    std::vector<char> settled(graph.n + 1);
    std::vector<int> order;
    Heap heap;
    PairResult result;

    for (int vertex = 1; vertex <= graph.n; ++vertex)
    {
        heuristic[vertex] = lower.TourHalf(remaining_original, vertex, group.distance);
        const double seed = group.distance[first][vertex] + group.distance[second][vertex];
        if (seed + heuristic[vertex] <= known_optimum + gst::fp::kEps)
        {
            distance[vertex] = seed;
            parent[vertex] = 0;
            heap.push({seed, vertex});
            ++result.pushes;
        }
    }
    while (!heap.empty())
    {
        const auto [value, vertex] = heap.top();
        heap.pop();
        if (value != distance[vertex] || settled[vertex] ||
            value + heuristic[vertex] > known_optimum + gst::fp::kEps)
            continue;
        settled[vertex] = 1;
        order.push_back(vertex);
        ++result.settled;
        for (const auto& edge : graph.adj[vertex])
        {
            const double next = value + edge.w;
            if (next >= distance[edge.to] ||
                next + heuristic[edge.to] > known_optimum + gst::fp::kEps)
                continue;
            distance[edge.to] = next;
            parent[edge.to] = vertex;
            heap.push({next, edge.to});
            ++result.pushes;
        }
    }
    result.search_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - search_begin).count();
    result.settled_costs.reserve(order.size());
    for (int vertex : order)
        result.settled_costs.push_back(distance[vertex]);

    std::vector<char> anchor_skyline(graph.n + 1);
    if (first != anchor_group && second != anchor_group)
    {
        std::vector<int> cost_order = order;
        std::sort(cost_order.begin(), cost_order.end(), [&](int a, int b)
        {
            if (distance[a] != distance[b])
                return distance[a] < distance[b];
            if (group.distance[anchor_group][a] != group.distance[anchor_group][b])
                return group.distance[anchor_group][a] <
                       group.distance[anchor_group][b];
            return a < b;
        });
        double best_anchor_distance = gst::fp::kInf;
        for (int vertex : cost_order)
            if (group.distance[anchor_group][vertex] + gst::fp::kEps <
                best_anchor_distance)
            {
                best_anchor_distance = group.distance[anchor_group][vertex];
                anchor_skyline[vertex] = 1;
            }
    }

    const auto profile_begin = Clock::now();
    std::vector<double> first_path(graph.n + 1);
    std::vector<double> second_path(graph.n + 1);
    std::vector<double> anchor_path(graph.n + 1);
    std::vector<double> pair_path(graph.n + 1);
    const std::vector<double> zero(graph.n + 1);
    auto Complete = [&](bool attach_anchor)
    {
        int remaining = half_rows.full_mask;
        remaining ^= 1 << half_rows.group_to_bit[first];
        remaining ^= 1 << half_rows.group_to_bit[second];
        if (attach_anchor)
            remaining ^= 1 << half_rows.group_to_bit[anchor_group];
        std::vector<std::pair<int, int>> partitions;
        std::vector<char> needed(half_rows.row.size());
        for (int left = remaining;; left = (left - 1) & remaining)
        {
            const int right = remaining ^ left;
            if (left <= right && CountBits(left) <= half_rows.half &&
                CountBits(right) <= half_rows.half)
            {
                partitions.push_back({left, right});
                needed[left] = 1;
                needed[right] = 1;
            }
            if (!left)
                break;
        }

        std::vector<std::vector<double>> tree_profile(half_rows.row.size());
        for (int mask = 0; mask < static_cast<int>(needed.size()); ++mask)
        {
            if (!needed[mask])
                continue;
            const auto& values = mask ? half_rows.row[mask] : zero;
            BuildPathMinimum(values, group.parent[first], group.order[first], first_path);
            BuildPathMinimum(values, group.parent[second], group.order[second], second_path);
            if (attach_anchor)
                BuildPathMinimum(values, group.parent[anchor_group],
                                 group.order[anchor_group], anchor_path);
            std::vector<double>& profile = tree_profile[mask];
            profile.assign(graph.n + 1, gst::fp::kInf);
            for (int vertex : order)
            {
                if (parent[vertex] == 0)
                    pair_path[vertex] = std::min(first_path[vertex], second_path[vertex]);
                else
                    pair_path[vertex] =
                        std::min(pair_path[parent[vertex]], values[vertex]);
                profile[vertex] = attach_anchor
                                      ? std::min(pair_path[vertex], anchor_path[vertex])
                                      : pair_path[vertex];
            }
            ++result.profile_columns;
            result.profile_values += order.size();
        }

        double& best = attach_anchor ? result.anchored_pair_upper : result.pair_upper;
        std::vector<double> partition_best(partitions.size(), gst::fp::kInf);
        for (int vertex : order)
        {
            const double paid_cost =
                distance[vertex] +
                (attach_anchor ? group.distance[anchor_group][vertex] : 0.0);
            for (size_t partition_id = 0;
                 partition_id < partitions.size(); ++partition_id)
            {
                const auto [left, right] = partitions[partition_id];
                const double candidate =
                    paid_cost + tree_profile[left][vertex] +
                    tree_profile[right][vertex];
                best = std::min(
                    best, candidate);
                partition_best[partition_id] =
                    std::min(partition_best[partition_id], candidate);
                if (!attach_anchor && anchor_skyline[vertex])
                {
                    if (candidate < result.anchor_skyline_pair_upper)
                    {
                        result.anchor_skyline_pair_upper = candidate;
                        result.anchor_skyline_best_root = vertex;
                        result.anchor_skyline_best_left = left;
                        result.anchor_skyline_best_right = right;
                    }
                }
                ++result.completion_probes;
            }
        }
        if (!attach_anchor)
            for (size_t partition_id = 0;
                 partition_id < partitions.size(); ++partition_id)
            {
                const auto [left, right] = partitions[partition_id];
                result.plans.push_back(
                    {(1 << first) | (1 << second), left, right,
                     partition_best[partition_id]});
            }
    };
    Complete(false);
    if (first != anchor_group && second != anchor_group)
        Complete(true);
    result.profile_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - profile_begin).count();
    return result;
}

void Probe(const gst::Graph& graph,
           const gst::Query& query,
           int query_id,
           double known_optimum)
{
    const auto begin = Clock::now();
    const int g = static_cast<int>(query.groups.size());
    if (g < 3 || g > 16)
        throw std::runtime_error("paid block probe requires 3 <= g <= 16");
    const int full_mask = (1 << g) - 1;
    const GroupData group = BuildGroupData(graph, query);

    int root = 1;
    double root_star = gst::fp::kInf;
    for (int vertex = 1; vertex <= graph.n; ++vertex)
    {
        double value = 0.0;
        for (int group_id = 0; group_id < g; ++group_id)
            value += group.distance[group_id][vertex];
        if (value < root_star)
        {
            root_star = value;
            root = vertex;
        }
    }
    int anchor_group = 0;
    for (int group_id = 1; group_id < g; ++group_id)
        if (group.distance[group_id][root] > group.distance[anchor_group][root])
            anchor_group = group_id;

    const HalfRows half_rows = BuildHalfRows(graph, group.distance);
    const Matrix metric = BuildGroupMetric(query, group.distance);
    gst::methods::test19::MetricTspLowerBound lower;
    lower.Build(metric, full_mask);

    PairResult total;
    std::vector<PairResult::PlanResult> priced_plans;
    std::vector<long long> pair_settled(1 << g);
    std::vector<std::vector<double>> pair_settled_costs(1 << g);
    double anchor_declared_pair_upper = gst::fp::kInf;
    double nonanchor_pair_upper = gst::fp::kInf;
    int anchor_skyline_best_first = -1;
    int anchor_skyline_best_second = -1;
    int anchor_skyline_best_root = 0;
    int anchor_skyline_best_left = 0;
    int anchor_skyline_best_right = 0;
    int pairs = 0;
    for (int first = 0; first < g; ++first)
    {
        for (int second = first + 1; second < g; ++second)
        {
            const PairResult current =
                ProbePair(graph, first, second, anchor_group, full_mask,
                          known_optimum, group, lower, half_rows);
            pair_settled[(1 << first) | (1 << second)] = current.settled;
            pair_settled_costs[(1 << first) | (1 << second)] =
                current.settled_costs;
            ++pairs;
            total.settled += current.settled;
            total.pushes += current.pushes;
            total.profile_columns += current.profile_columns;
            total.profile_values += current.profile_values;
            total.completion_probes += current.completion_probes;
            total.pair_upper = std::min(total.pair_upper, current.pair_upper);
            if (current.anchor_skyline_pair_upper <
                total.anchor_skyline_pair_upper)
            {
                total.anchor_skyline_pair_upper =
                    current.anchor_skyline_pair_upper;
                anchor_skyline_best_first = first;
                anchor_skyline_best_second = second;
                anchor_skyline_best_root = current.anchor_skyline_best_root;
                anchor_skyline_best_left = current.anchor_skyline_best_left;
                anchor_skyline_best_right = current.anchor_skyline_best_right;
            }
            total.anchored_pair_upper =
                std::min(total.anchored_pair_upper, current.anchored_pair_upper);
            total.search_ms += current.search_ms;
            total.profile_ms += current.profile_ms;
            priced_plans.insert(priced_plans.end(),
                                current.plans.begin(), current.plans.end());
            if (first == anchor_group || second == anchor_group)
                anchor_declared_pair_upper =
                    std::min(anchor_declared_pair_upper, current.pair_upper);
            else
                nonanchor_pair_upper =
                    std::min(nonanchor_pair_upper, current.pair_upper);
        }
    }

    struct PlanScore
    {
        double score = gst::fp::kInf;
        int pair = 0;
        int left = 0;
        int right = 0;
    };
    std::vector<PlanScore> root_star_plans;
    std::vector<PlanScore> low_root_star_plans;
    std::vector<PlanScore> component_lower_plans;
    std::vector<PlanScore> strong_component_lower_plans;
    auto RootRowValue = [&](int mask)
    {
        return mask ? half_rows.row[mask][root] : 0.0;
    };
    const int quarter = (half_rows.half + 1) / 2;
    auto LowTopSeed = [&](int mask)
    {
        if (!mask)
            return 0.0;
        if (CountBits(mask) <= quarter)
            return RootRowValue(mask);
        double value = gst::fp::kInf;
        const int pivot = FirstBit(mask);
        const int branch_domain = mask ^ (1 << pivot);
        for (int right = branch_domain; right;
             right = (right - 1) & branch_domain)
        {
            const int left = mask ^ right;
            if (CountBits(left) <= quarter && CountBits(right) <= quarter)
                value = std::min(
                    value, RootRowValue(left) + RootRowValue(right));
        }
        return value;
    };
    std::vector<double> block_optimum(1 << g, gst::fp::kInf);
    block_optimum[0] = 0.0;
    for (int mask = 1; mask <= half_rows.full_mask; ++mask)
        if (CountBits(mask) <= half_rows.half)
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                block_optimum[mask] =
                    std::min(block_optimum[mask], half_rows.row[mask][vertex]);
    auto RootFreeOptimum = [&](int mask)
    {
        if (block_optimum[mask] < gst::fp::kInf)
            return block_optimum[mask];
        double value = gst::fp::kInf;
        const int pivot = FirstBit(mask);
        const int branch_domain = mask ^ (1 << pivot);
        for (int right = branch_domain; right;
             right = (right - 1) & branch_domain)
        {
            const int left = mask ^ right;
            if (CountBits(left) > half_rows.half - 1 ||
                CountBits(right) > half_rows.half - 1)
                continue;
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                value = std::min(
                    value,
                    half_rows.row[left][vertex] +
                        half_rows.row[right][vertex]);
        }
        block_optimum[mask] = value;
        return value;
    };
    for (int first = 0; first < g; ++first)
    {
        if (first == anchor_group)
            continue;
        for (int second = first + 1; second < g; ++second)
        {
            if (second == anchor_group)
                continue;
            const int pair = (1 << first) | (1 << second);
            const int remaining = half_rows.full_mask ^ pair;
            for (int left = remaining;; left = (left - 1) & remaining)
            {
                const int right = remaining ^ left;
                if (left <= right && CountBits(left) <= half_rows.half &&
                    CountBits(right) <= half_rows.half)
                {
                    root_star_plans.push_back(
                        {RootRowValue(pair) + RootRowValue(left) +
                             RootRowValue(right),
                         pair, left, right});
                    low_root_star_plans.push_back(
                        {LowTopSeed(pair) + LowTopSeed(left) + LowTopSeed(right),
                         pair, left, right});
                    component_lower_plans.push_back(
                        {block_optimum[pair] + block_optimum[left] +
                             block_optimum[right],
                         pair, left, right});
                    const double component_sum =
                        block_optimum[pair] + block_optimum[left] +
                        block_optimum[right];
                    const double pair_left = RootFreeOptimum(pair | left);
                    const double pair_right = RootFreeOptimum(pair | right);
                    strong_component_lower_plans.push_back(
                        {std::max(
                             component_sum,
                             std::max(
                                 std::max(
                                     pair_left + block_optimum[right],
                                     pair_right + block_optimum[left]),
                                 0.5 * (pair_left + pair_right +
                                        block_optimum[left] +
                                        block_optimum[right]))),
                         pair, left, right});
                }
                if (!left)
                    break;
            }
        }
    }
    std::sort(root_star_plans.begin(), root_star_plans.end(),
              [](const PlanScore& a, const PlanScore& b)
    {
        if (a.score != b.score)
            return a.score < b.score;
        if (a.pair != b.pair)
            return a.pair < b.pair;
        if (a.left != b.left)
            return a.left < b.left;
        return a.right < b.right;
    });
    std::sort(low_root_star_plans.begin(), low_root_star_plans.end(),
              [](const PlanScore& a, const PlanScore& b)
    {
        if (a.score != b.score)
            return a.score < b.score;
        if (a.pair != b.pair)
            return a.pair < b.pair;
        if (a.left != b.left)
            return a.left < b.left;
        return a.right < b.right;
    });
    std::sort(component_lower_plans.begin(), component_lower_plans.end(),
              [](const PlanScore& a, const PlanScore& b)
    {
        if (a.score != b.score)
            return a.score < b.score;
        if (a.pair != b.pair)
            return a.pair < b.pair;
        if (a.left != b.left)
            return a.left < b.left;
        return a.right < b.right;
    });
    std::sort(strong_component_lower_plans.begin(),
              strong_component_lower_plans.end(),
              [](const PlanScore& a, const PlanScore& b)
    {
        if (a.score != b.score)
            return a.score < b.score;
        if (a.pair != b.pair)
            return a.pair < b.pair;
        if (a.left != b.left)
            return a.left < b.left;
        return a.right < b.right;
    });
    int winner_root_star_rank = 0;
    const int winner_pair =
        anchor_skyline_best_first >= 0
            ? (1 << anchor_skyline_best_first) |
                  (1 << anchor_skyline_best_second)
            : 0;
    for (size_t index = 0; index < root_star_plans.size(); ++index)
    {
        const PlanScore& plan = root_star_plans[index];
        if (plan.pair == winner_pair &&
            ((plan.left == anchor_skyline_best_left &&
              plan.right == anchor_skyline_best_right) ||
             (plan.left == anchor_skyline_best_right &&
              plan.right == anchor_skyline_best_left)))
        {
            winner_root_star_rank = static_cast<int>(index) + 1;
            break;
        }
    }
    int winner_low_root_star_rank = 0;
    for (size_t index = 0; index < low_root_star_plans.size(); ++index)
    {
        const PlanScore& plan = low_root_star_plans[index];
        if (plan.pair == winner_pair &&
            ((plan.left == anchor_skyline_best_left &&
              plan.right == anchor_skyline_best_right) ||
             (plan.left == anchor_skyline_best_right &&
              plan.right == anchor_skyline_best_left)))
        {
            winner_low_root_star_rank = static_cast<int>(index) + 1;
            break;
        }
    }
    std::sort(priced_plans.begin(), priced_plans.end(),
              [](const PairResult::PlanResult& a,
                 const PairResult::PlanResult& b)
    {
        if (a.pair != b.pair)
            return a.pair < b.pair;
        if (a.left != b.left)
            return a.left < b.left;
        return a.right < b.right;
    });
    auto PricedValue = [&](const PlanScore& score)
    {
        const auto found = std::lower_bound(
            priced_plans.begin(), priced_plans.end(), score,
            [](const PairResult::PlanResult& plan, const PlanScore& key)
            {
                if (plan.pair != key.pair)
                    return plan.pair < key.pair;
                if (plan.left != key.left)
                    return plan.left < key.left;
                return plan.right < key.right;
            });
        return found != priced_plans.end() && found->pair == score.pair &&
                       found->left == score.left && found->right == score.right
                   ? found->best
                   : gst::fp::kInf;
    };
    int first_exact_low_rank = 0;
    for (size_t index = 0; index < low_root_star_plans.size(); ++index)
        if (PricedValue(low_root_star_plans[index]) <=
            known_optimum + gst::fp::kEps)
        {
            first_exact_low_rank = static_cast<int>(index) + 1;
            break;
        }
    int first_exact_component_rank = 0;
    int component_certificate_calls = 0;
    double component_best = gst::fp::kInf;
    for (size_t index = 0; index < component_lower_plans.size(); ++index)
    {
        const double priced = PricedValue(component_lower_plans[index]);
        component_best = std::min(component_best, priced);
        if (!first_exact_component_rank &&
            priced <= known_optimum + gst::fp::kEps)
            first_exact_component_rank = static_cast<int>(index) + 1;
        const double next_lower = index + 1 < component_lower_plans.size()
            ? component_lower_plans[index + 1].score
            : gst::fp::kInf;
        if (next_lower + gst::fp::kEps >= component_best)
        {
            component_certificate_calls = static_cast<int>(index) + 1;
            break;
        }
    }
    int first_exact_strong_rank = 0;
    int strong_certificate_calls = 0;
    double strong_best = gst::fp::kInf;
    for (size_t index = 0; index < strong_component_lower_plans.size(); ++index)
    {
        const double priced = PricedValue(strong_component_lower_plans[index]);
        strong_best = std::min(strong_best, priced);
        if (!first_exact_strong_rank &&
            priced <= known_optimum + gst::fp::kEps)
            first_exact_strong_rank = static_cast<int>(index) + 1;
        const double next_lower =
            index + 1 < strong_component_lower_plans.size()
                ? strong_component_lower_plans[index + 1].score
                : gst::fp::kInf;
        if (next_lower + gst::fp::kEps >= strong_best)
        {
            strong_certificate_calls = static_cast<int>(index) + 1;
            break;
        }
    }
    std::vector<std::pair<int, int>> strong_columns;
    std::vector<int> strong_pair_plan_count(1 << g);
    for (int index = 0; index < strong_certificate_calls; ++index)
    {
        const PlanScore& plan = strong_component_lower_plans[index];
        ++strong_pair_plan_count[plan.pair];
        strong_columns.push_back({plan.pair, plan.left});
        strong_columns.push_back({plan.pair, plan.right});
    }
    std::sort(strong_columns.begin(), strong_columns.end());
    strong_columns.erase(
        std::unique(strong_columns.begin(), strong_columns.end()),
        strong_columns.end());
    int strong_certificate_pairs = 0;
    int strong_max_plans_per_pair = 0;
    int strong_max_columns_per_pair = 0;
    long long strong_profile_values = 0;
    long long strong_completion_probes = 0;
    long long strong_bound_profile_values = 0;
    long long strong_bound_completion_probes = 0;
    long long strong_bound_max_roots = 0;
    struct BoundedColumn
    {
        int pair = 0;
        int block = 0;
        long long roots = 0;
    };
    std::vector<BoundedColumn> strong_bounded_columns;
    for (int pair = 0; pair < (1 << g); ++pair)
    {
        if (!strong_pair_plan_count[pair])
            continue;
        ++strong_certificate_pairs;
        strong_max_plans_per_pair =
            std::max(strong_max_plans_per_pair,
                     strong_pair_plan_count[pair]);
        strong_completion_probes +=
            pair_settled[pair] * strong_pair_plan_count[pair];
    }
    for (size_t begin = 0; begin < strong_columns.size();)
    {
        size_t end = begin + 1;
        while (end < strong_columns.size() &&
               strong_columns[end].first == strong_columns[begin].first)
            ++end;
        const int pair = strong_columns[begin].first;
        const int column_count = static_cast<int>(end - begin);
        strong_max_columns_per_pair =
            std::max(strong_max_columns_per_pair, column_count);
        strong_profile_values += pair_settled[pair] * column_count;
        begin = end;
    }
    for (int index = 0; index < strong_certificate_calls; ++index)
    {
        const PlanScore& plan = strong_component_lower_plans[index];
        const double root_limit =
            strong_best - block_optimum[plan.left] - block_optimum[plan.right];
        const auto& costs = pair_settled_costs[plan.pair];
        const long long roots = static_cast<long long>(
            std::lower_bound(costs.begin(), costs.end(),
                             root_limit + gst::fp::kEps) - costs.begin());
        strong_bound_completion_probes += roots;
        strong_bound_max_roots = std::max(strong_bound_max_roots, roots);
        strong_bounded_columns.push_back({plan.pair, plan.left, roots});
        strong_bounded_columns.push_back({plan.pair, plan.right, roots});
    }
    std::sort(strong_bounded_columns.begin(), strong_bounded_columns.end(),
              [](const BoundedColumn& a, const BoundedColumn& b)
    {
        if (a.pair != b.pair)
            return a.pair < b.pair;
        if (a.block != b.block)
            return a.block < b.block;
        return a.roots > b.roots;
    });
    for (size_t index = 0; index < strong_bounded_columns.size();)
    {
        strong_bound_profile_values += strong_bounded_columns[index].roots;
        size_t next = index + 1;
        while (next < strong_bounded_columns.size() &&
               strong_bounded_columns[next].pair ==
                   strong_bounded_columns[index].pair &&
               strong_bounded_columns[next].block ==
                   strong_bounded_columns[index].block)
            ++next;
        index = next;
    }

    const double total_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    std::cout << std::fixed << std::setprecision(10)
              << "paid_block_completion query=" << query_id
              << " n=" << graph.n
              << " m=" << graph.m
              << " g=" << g
              << " anchor_group=" << (anchor_group + 1)
              << " root=" << root
              << " known_optimum=" << known_optimum
              << " pairs=" << pairs
              << " settled=" << total.settled
              << " profile_columns=" << total.profile_columns
              << " profile_values=" << total.profile_values
              << " completion_probes=" << total.completion_probes
              << " paid_pair_block_upper=" << total.pair_upper
              << " paid_pair_block_gap_pct="
              << (100.0 * (total.pair_upper - known_optimum) / known_optimum)
              << " anchor_skyline_pair_upper=" << total.anchor_skyline_pair_upper
              << " anchor_skyline_pair_gap_pct="
              << (100.0 * (total.anchor_skyline_pair_upper - known_optimum) /
                  known_optimum)
              << " anchor_skyline_best_first=" << (anchor_skyline_best_first + 1)
              << " anchor_skyline_best_second=" << (anchor_skyline_best_second + 1)
              << " anchor_skyline_best_root=" << anchor_skyline_best_root
              << " anchor_skyline_best_left=" << anchor_skyline_best_left
              << " anchor_skyline_best_right=" << anchor_skyline_best_right
              << " root_star_plan_count=" << root_star_plans.size()
              << " winner_root_star_rank=" << winner_root_star_rank
              << " winner_low_root_star_rank=" << winner_low_root_star_rank
              << " first_exact_low_rank=" << first_exact_low_rank
              << " first_exact_component_rank="
              << first_exact_component_rank
              << " component_certificate_calls="
              << component_certificate_calls
              << " component_certified_best=" << component_best
              << " first_exact_strong_rank=" << first_exact_strong_rank
              << " strong_certificate_calls=" << strong_certificate_calls
              << " strong_certified_best=" << strong_best
              << " strong_certificate_pairs=" << strong_certificate_pairs
              << " strong_unique_columns=" << strong_columns.size()
              << " strong_max_plans_per_pair=" << strong_max_plans_per_pair
              << " strong_max_columns_per_pair=" << strong_max_columns_per_pair
              << " strong_profile_values=" << strong_profile_values
              << " strong_completion_probes=" << strong_completion_probes
              << " strong_bound_profile_values="
              << strong_bound_profile_values
              << " strong_bound_completion_probes="
              << strong_bound_completion_probes
              << " strong_bound_max_roots=" << strong_bound_max_roots
              << " anchor_declared_pair_upper=" << anchor_declared_pair_upper
              << " anchor_declared_pair_gap_pct="
              << (100.0 * (anchor_declared_pair_upper - known_optimum) /
                  known_optimum)
              << " nonanchor_pair_upper=" << nonanchor_pair_upper
              << " paid_anchor_pair_block_upper=" << total.anchored_pair_upper
              << " paid_anchor_pair_block_gap_pct="
              << (100.0 * (total.anchored_pair_upper - known_optimum) / known_optimum)
              << " half_seed_probes=" << half_rows.seed_probes
              << " half_queue_pops=" << half_rows.queue_pops
              << " half_ms=" << half_rows.milliseconds
              << " pair_ms=" << total.search_ms
              << " profile_ms=" << total.profile_ms
              << " total_ms=" << total_ms << '\n';
}
}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 5 || argc > 6)
        {
            std::cerr << "usage: " << argv[0]
                      << " <data_root> <graph_selector> <query_selector>"
                      << " <known_optimum> [query_begin_1based=1]\n";
            return 2;
        }
        const std::string graph_folder = gst::ResolveGraphFolder(argv[1], argv[2]);
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const std::vector<gst::Query> queries =
            gst::LoadQueriesFromFolder(graph_folder, argv[3]);
        const double known_optimum = std::stod(argv[4]);
        const int query_id = argc == 6 ? std::stoi(argv[5]) : 1;
        if (query_id < 1 || query_id > static_cast<int>(queries.size()))
            throw std::runtime_error("query index out of range");
        Probe(graph, queries[query_id - 1], query_id, known_optimum);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
