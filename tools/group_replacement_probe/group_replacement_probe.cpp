#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "float_compare.h"
#include "graph_io.h"
#include "methods/Release/release_v1.h"
#include "methods/Test/test19_future_bounds.h"
#include "query_io.h"

namespace
{
using Clock = std::chrono::steady_clock;
using HeapItem = std::pair<double, int>;
using Heap = std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>>;

struct PairResult
{
    int mask = 0;
    int portal_group = -1;
    double cap = gst::fp::kInf;
    long long all_states = 0;
    long long baseline_states = 0;
    long long capped_states = 0;
    long long baseline_capped_states = 0;
};

struct PairLayerResult
{
    int mask = 0;
    double cap = gst::fp::kInf;
    long long settled = 0;
    long long cap_kept = 0;
    long long queue_pushes = 0;
};

int FirstBit(int mask)
{
    int bit = 0;
    while (!((mask >> bit) & 1))
        ++bit;
    return bit;
}

int Popcount(int mask)
{
    int count = 0;
    while (mask)
    {
        mask &= mask - 1;
        ++count;
    }
    return count;
}

double Percent(long long part, long long whole)
{
    return whole ? 100.0 * static_cast<double>(part) / static_cast<double>(whole) : 0.0;
}

std::vector<std::vector<double>> ComputeGroupDistances(const gst::Graph& graph,
                                                       const gst::Query& query)
{
    std::vector<std::vector<double>> distance(
        query.groups.size(), std::vector<double>(graph.n + 1, gst::fp::kInf));
    for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
    {
        Heap heap;
        for (int v : query.groups[group])
        {
            if (distance[group][v] == 0.0)
                continue;
            distance[group][v] = 0.0;
            heap.push({0.0, v});
        }
        while (!heap.empty())
        {
            const auto [d, u] = heap.top();
            heap.pop();
            if (d != distance[group][u])
                continue;
            for (const auto& edge : graph.adj[u])
            {
                const double next = d + edge.w;
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

std::vector<std::vector<double>> BuildGroupMetric(
    const gst::Query& query,
    const std::vector<std::vector<double>>& group_distance)
{
    const int g = static_cast<int>(query.groups.size());
    std::vector<std::vector<double>> metric(g, std::vector<double>(g, gst::fp::kInf));
    for (int a = 0; a < g; ++a)
        for (int b = 0; b < g; ++b)
            for (int v : query.groups[b])
                metric[a][b] = std::min(metric[a][b], group_distance[a][v]);
    return metric;
}

std::vector<double> BuildRepresentativeDegreeBounds(
    const gst::Query& query,
    const std::vector<std::vector<double>>& group_distance)
{
    const int g = static_cast<int>(query.groups.size());
    const int subset_count = 1 << g;
    std::vector<double> two_edges(static_cast<size_t>(g) * g * g, gst::fp::kInf);
    auto TwoEdges = [&](int center, int left, int right) -> double&
    {
        return two_edges[(static_cast<size_t>(center) * g + left) * g + right];
    };

    for (int center = 0; center < g; ++center)
    {
        for (int left = 0; left < g; ++left)
        {
            if (left == center)
                continue;
            for (int right = left + 1; right < g; ++right)
            {
                if (right == center)
                    continue;
                double best = gst::fp::kInf;
                for (int x : query.groups[center])
                    best = std::min(best, group_distance[left][x] + group_distance[right][x]);
                TwoEdges(center, left, right) = best;
                TwoEdges(center, right, left) = best;
            }
        }
    }

    std::vector<double> bounds(subset_count);
    std::vector<double> charges;
    for (int mask = 1; mask < subset_count; ++mask)
    {
        if (Popcount(mask) < 3)
            continue;
        charges.clear();
        for (int center = 0; center < g; ++center)
        {
            if (!((mask >> center) & 1))
                continue;
            double charge = gst::fp::kInf;
            for (int left = 0; left < g; ++left)
            {
                if (left == center || !((mask >> left) & 1))
                    continue;
                for (int right = left + 1; right < g; ++right)
                {
                    if (right == center || !((mask >> right) & 1))
                        continue;
                    charge = std::min(charge, TwoEdges(center, left, right));
                }
            }
            charges.push_back(charge);
        }
        std::sort(charges.begin(), charges.end());
        double sum = 0.0;
        for (int i = 0; i + 2 < static_cast<int>(charges.size()); ++i)
            sum += charges[i];
        bounds[mask] = sum * 0.25;
    }
    return bounds;
}

class RepresentativePathLowerBound
{
public:
    void Build(const gst::Query& query,
               const std::vector<std::vector<double>>& group_distance)
    {
        g_ = static_cast<int>(query.groups.size());
        const int subset_count = 1 << g_;
        const int full_mask = subset_count - 1;
        std::vector<double> two_edge(
            static_cast<size_t>(g_) * g_ * g_, gst::fp::kInf);
        auto TwoEdge = [&](int center, int left, int right) -> double&
        {
            return two_edge[(static_cast<size_t>(center) * g_ + left) * g_ + right];
        };

        for (int center = 0; center < g_; ++center)
        {
            for (int left = 0; left < g_; ++left)
            {
                if (left == center)
                    continue;
                for (int right = left + 1; right < g_; ++right)
                {
                    if (right == center)
                        continue;
                    double best = gst::fp::kInf;
                    for (int x : query.groups[center])
                    {
                        best = std::min(
                            best, group_distance[left][x] + group_distance[right][x]);
                    }
                    TwoEdge(center, left, right) = best;
                    TwoEdge(center, right, left) = best;
                }
            }
        }

        std::vector<double> state(
            static_cast<size_t>(subset_count) * g_ * g_ * g_, gst::fp::kInf);
        for (int first = 0; first < g_; ++first)
        {
            for (int last = 0; last < g_; ++last)
            {
                if (first == last)
                    continue;
                state[StateIndex((1 << first) | (1 << last), first, first, last)] = 0.0;
            }
        }

        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (Popcount(mask) < 2)
                continue;
            const int remaining = full_mask ^ mask;
            for (int first = 0; first < g_; ++first)
            {
                if (!((mask >> first) & 1))
                    continue;
                for (int previous = 0; previous < g_; ++previous)
                {
                    if (!((mask >> previous) & 1))
                        continue;
                    for (int last = 0; last < g_; ++last)
                    {
                        if (last == previous || !((mask >> last) & 1))
                            continue;
                        const double current = state[StateIndex(mask, first, previous, last)];
                        if (current >= gst::fp::kInf / 4)
                            continue;
                        for (int bits = remaining; bits; bits &= bits - 1)
                        {
                            const int next = FirstBit(bits & -bits);
                            double& destination =
                                state[StateIndex(mask | (1 << next), first, last, next)];
                            destination = std::min(
                                destination, current + 0.5 * TwoEdge(last, previous, next));
                        }
                    }
                }
            }
        }

        endpoint_path_.assign(
            static_cast<size_t>(subset_count) * g_ * g_, gst::fp::kInf);
        peak_working_bytes_ =
            (state.size() + two_edge.size() + endpoint_path_.size()) * sizeof(double);
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (Popcount(mask) < 2)
                continue;
            for (int first = 0; first < g_; ++first)
            {
                if (!((mask >> first) & 1))
                    continue;
                for (int last = 0; last < g_; ++last)
                {
                    if (last == first || !((mask >> last) & 1))
                        continue;
                    double best = gst::fp::kInf;
                    for (int previous = 0; previous < g_; ++previous)
                    {
                        if (previous != last && ((mask >> previous) & 1))
                        {
                            best = std::min(
                                best, state[StateIndex(mask, first, previous, last)]);
                        }
                    }
                    endpoint_path_[PathIndex(mask, first, last)] = best;
                }
            }
        }
    }

    double TourHalf(int mask,
                    int root,
                    const std::vector<std::vector<double>>& group_distance) const
    {
        if (!mask)
            return 0.0;
        if (!(mask & (mask - 1)))
            return group_distance[FirstBit(mask)][root];

        double cycle_bound = gst::fp::kInf;
        for (int first = 0; first < g_; ++first)
        {
            if (!((mask >> first) & 1))
                continue;
            for (int last = 0; last < g_; ++last)
            {
                if (last == first || !((mask >> last) & 1))
                    continue;
                cycle_bound = std::min(
                    cycle_bound,
                    group_distance[first][root] +
                        endpoint_path_[PathIndex(mask, first, last)] +
                        group_distance[last][root]);
            }
        }
        return cycle_bound * 0.5;
    }

    size_t PeakWorkingBytes() const
    {
        return peak_working_bytes_;
    }

    size_t ResidentBytes() const
    {
        return endpoint_path_.size() * sizeof(double);
    }

private:
    size_t StateIndex(int mask, int first, int previous, int last) const
    {
        return ((static_cast<size_t>(mask) * g_ + first) * g_ + previous) * g_ + last;
    }

    size_t PathIndex(int mask, int first, int last) const
    {
        return (static_cast<size_t>(mask) * g_ + first) * g_ + last;
    }

    int g_ = 0;
    size_t peak_working_bytes_ = 0;
    std::vector<double> endpoint_path_;
};

std::pair<double, int> PairReplacementCap(
    int pair_mask,
    const gst::Query& query,
    const std::vector<std::vector<double>>& group_distance)
{
    const int g = static_cast<int>(query.groups.size());
    const int a = FirstBit(pair_mask);
    const int b = FirstBit(pair_mask ^ (1 << a));
    double best_cap = gst::fp::kInf;
    int best_group = -1;
    for (int portal_group = 0; portal_group < g; ++portal_group)
    {
        if ((pair_mask >> portal_group) & 1)
            continue;
        double worst_candidate = 0.0;
        for (int x : query.groups[portal_group])
        {
            const double star = group_distance[a][x] + group_distance[b][x];
            worst_candidate = std::max(worst_candidate, star);
        }
        if (worst_candidate < best_cap)
        {
            best_cap = worst_candidate;
            best_group = portal_group;
        }
    }
    return {best_cap, best_group};
}

std::vector<double> BuildReplacementCaps(
    const gst::Query& query,
    const std::vector<std::vector<double>>& group_distance)
{
    const int g = static_cast<int>(query.groups.size());
    const int subset_count = 1 << g;
    const int full_mask = subset_count - 1;
    std::vector<double> caps(subset_count, gst::fp::kInf);
    std::vector<double> subset_sum(subset_count);
    std::vector<double> worst(subset_count);

    for (int portal_group = 0; portal_group < g; ++portal_group)
    {
        std::fill(worst.begin(), worst.end(), 0.0);
        for (int x : query.groups[portal_group])
        {
            subset_sum[0] = 0.0;
            for (int mask = 1; mask < subset_count; ++mask)
            {
                const int bit = mask & -mask;
                subset_sum[mask] = subset_sum[mask ^ bit] + group_distance[FirstBit(bit)][x];
            }
            for (int mask = 1; mask < full_mask; ++mask)
            {
                if (!((mask >> portal_group) & 1))
                    worst[mask] = std::max(worst[mask], subset_sum[mask]);
            }
        }
        for (int mask = 1; mask < full_mask; ++mask)
        {
            if (!((mask >> portal_group) & 1))
                caps[mask] = std::min(caps[mask], worst[mask]);
        }
    }
    return caps;
}

double SolveDenseDp(const gst::Graph& graph, const gst::Query& query, bool use_caps)
{
    const int g = static_cast<int>(query.groups.size());
    const int subset_count = 1 << g;
    const int full_mask = subset_count - 1;
    const int stride = graph.n + 1;
    const auto group_distance = ComputeGroupDistances(graph, query);
    const std::vector<double> caps = BuildReplacementCaps(query, group_distance);
    std::vector<double> dp(static_cast<size_t>(subset_count) * stride, gst::fp::kInf);

    auto At = [&](int mask, int vertex) -> double&
    {
        return dp[static_cast<size_t>(mask) * stride + vertex];
    };
    for (int group = 0; group < g; ++group)
        for (int v = 1; v <= graph.n; ++v)
            At(1 << group, v) = group_distance[group][v];

    for (int mask = 1; mask < subset_count; ++mask)
    {
        if (Popcount(mask) == 1)
            continue;
        const double cap = use_caps && mask != full_mask ? caps[mask] : gst::fp::kInf;
        Heap heap;
        for (int left = (mask - 1) & mask; left; left = (left - 1) & mask)
        {
            const int right = mask ^ left;
            if (!right || left > right)
                continue;
            for (int v = 1; v <= graph.n; ++v)
            {
                const double candidate = At(left, v) + At(right, v);
                if (candidate < At(mask, v) && candidate <= cap + gst::fp::kEps)
                    At(mask, v) = candidate;
            }
        }
        for (int v = 1; v <= graph.n; ++v)
            if (At(mask, v) <= cap + gst::fp::kEps)
                heap.push({At(mask, v), v});

        while (!heap.empty())
        {
            const auto [distance, u] = heap.top();
            heap.pop();
            if (distance != At(mask, u))
                continue;
            for (const auto& edge : graph.adj[u])
            {
                const double next = distance + edge.w;
                if (next < At(mask, edge.to) && next <= cap + gst::fp::kEps)
                {
                    At(mask, edge.to) = next;
                    heap.push({next, edge.to});
                }
            }
        }
    }

    double answer = gst::fp::kInf;
    for (int v = 1; v <= graph.n; ++v)
        answer = std::min(answer, At(full_mask, v));
    return answer;
}

std::vector<double> ComputeDenseDpRows(
    const gst::Graph& graph,
    const gst::Query& query,
    const std::vector<std::vector<double>>& group_distance)
{
    const int g = static_cast<int>(query.groups.size());
    const int subset_count = 1 << g;
    const int stride = graph.n + 1;
    std::vector<double> dp(static_cast<size_t>(subset_count) * stride, gst::fp::kInf);
    auto At = [&](int mask, int vertex) -> double&
    {
        return dp[static_cast<size_t>(mask) * stride + vertex];
    };

    for (int group = 0; group < g; ++group)
        for (int v = 1; v <= graph.n; ++v)
            At(1 << group, v) = group_distance[group][v];

    for (int mask = 1; mask < subset_count; ++mask)
    {
        if (Popcount(mask) == 1)
            continue;
        Heap heap;
        for (int left = (mask - 1) & mask; left; left = (left - 1) & mask)
        {
            const int right = mask ^ left;
            if (!right || left > right)
                continue;
            for (int v = 1; v <= graph.n; ++v)
                At(mask, v) = std::min(At(mask, v), At(left, v) + At(right, v));
        }
        for (int v = 1; v <= graph.n; ++v)
            heap.push({At(mask, v), v});
        while (!heap.empty())
        {
            const auto [distance, u] = heap.top();
            heap.pop();
            if (distance != At(mask, u))
                continue;
            for (const auto& edge : graph.adj[u])
            {
                const double next = distance + edge.w;
                if (next < At(mask, edge.to))
                {
                    At(mask, edge.to) = next;
                    heap.push({next, edge.to});
                }
            }
        }
    }
    return dp;
}

void AddEdge(gst::Graph& graph, int u, int v, double weight)
{
    const int id = static_cast<int>(graph.edges.size());
    graph.edges.push_back({id, u, v, weight});
    graph.adj[u].push_back({v, id, weight});
    graph.adj[v].push_back({u, id, weight});
    graph.m = static_cast<int>(graph.edges.size());
}

std::pair<gst::Graph, gst::Query> RandomInstance(std::mt19937_64& random,
                                                int min_group_count,
                                                int max_group_count,
                                                int max_vertex_count)
{
    std::uniform_int_distribution<int> n_distribution(4, max_vertex_count);
    std::uniform_int_distribution<int> g_distribution(min_group_count, max_group_count);
    std::uniform_int_distribution<int> weight_distribution(1, 30);
    const int n = n_distribution(random);
    const int g = g_distribution(random);

    gst::Graph graph;
    graph.n = n;
    graph.adj.resize(n + 1);
    for (int v = 2; v <= n; ++v)
    {
        std::uniform_int_distribution<int> parent_distribution(1, v - 1);
        AddEdge(graph, v, parent_distribution(random), weight_distribution(random));
    }
    std::bernoulli_distribution extra_edge(0.32);
    for (int u = 1; u <= n; ++u)
    {
        for (int v = u + 1; v <= n; ++v)
        {
            bool exists = false;
            for (const auto& edge : graph.adj[u])
                exists = exists || edge.to == v;
            if (!exists && extra_edge(random))
                AddEdge(graph, u, v, weight_distribution(random));
        }
    }

    gst::Query query;
    query.groups.resize(g);
    std::uniform_int_distribution<int> vertex_distribution(1, n);
    std::uniform_int_distribution<int> size_distribution(1, std::min(3, n));
    for (auto& group : query.groups)
    {
        const int target_size = size_distribution(random);
        while (static_cast<int>(group.size()) < target_size)
        {
            const int v = vertex_distribution(random);
            if (std::find(group.begin(), group.end(), v) == group.end())
                group.push_back(v);
        }
    }
    return {std::move(graph), std::move(query)};
}

int RunSelfCheck(std::uint64_t seed,
                 int iterations,
                 int min_group_count,
                 int max_group_count,
                 int max_vertex_count)
{
    std::mt19937_64 random(seed);
    for (int iteration = 1; iteration <= iterations; ++iteration)
    {
        auto [graph, query] =
            RandomInstance(random, min_group_count, max_group_count, max_vertex_count);
        const double exact = SolveDenseDp(graph, query, false);
        const double capped = SolveDenseDp(graph, query, true);
        if (std::fabs(exact - capped) > 1e-6)
        {
            std::cout << std::setprecision(17)
                      << "MISMATCH seed=" << seed
                      << " iteration=" << iteration
                      << " n=" << graph.n
                      << " g=" << query.groups.size()
                      << " exact=" << exact
                      << " capped=" << capped << '\n';
            return 1;
        }
        if (iteration % 1000 == 0)
            std::cout << "ok " << iteration << '\n';
    }
    std::cout << "ALL_OK seed=" << seed << " iterations=" << iterations << '\n';
    return 0;
}

int RunSecondOrderSelfCheck(std::uint64_t seed,
                            int iterations,
                            int min_group_count,
                            int max_group_count,
                            int max_vertex_count)
{
    std::mt19937_64 random(seed);
    for (int iteration = 1; iteration <= iterations; ++iteration)
    {
        auto [graph, query] =
            RandomInstance(random, min_group_count, max_group_count, max_vertex_count);
        const int g = static_cast<int>(query.groups.size());
        const int subset_count = 1 << g;
        const int stride = graph.n + 1;
        const auto group_distance = ComputeGroupDistances(graph, query);
        RepresentativePathLowerBound bound;
        bound.Build(query, group_distance);
        const std::vector<double> exact = ComputeDenseDpRows(graph, query, group_distance);

        for (int mask = 1; mask < subset_count; ++mask)
        {
            for (int v = 1; v <= graph.n; ++v)
            {
                const double lower = bound.TourHalf(mask, v, group_distance);
                const double optimum = exact[static_cast<size_t>(mask) * stride + v];
                if (lower > optimum + 1e-6)
                {
                    std::cout << std::setprecision(17)
                              << "ADMISSIBILITY_FAIL seed=" << seed
                              << " iteration=" << iteration
                              << " mask=" << mask
                              << " vertex=" << v
                              << " lower=" << lower
                              << " optimum=" << optimum << '\n';
                    return 1;
                }
            }
            for (const auto& edge : graph.edges)
            {
                const double left = bound.TourHalf(mask, edge.u, group_distance);
                const double right = bound.TourHalf(mask, edge.v, group_distance);
                if (left > edge.w + right + 1e-6 ||
                    right > edge.w + left + 1e-6)
                {
                    std::cout << std::setprecision(17)
                              << "CONSISTENCY_FAIL seed=" << seed
                              << " iteration=" << iteration
                              << " mask=" << mask
                              << " edge=" << edge.u << ',' << edge.v
                              << " weight=" << edge.w
                              << " left=" << left
                              << " right=" << right << '\n';
                    return 1;
                }
            }
        }
        if (iteration % 100 == 0)
            std::cout << "second_order_ok " << iteration << '\n';
    }
    std::cout << "SECOND_ORDER_ALL_OK seed=" << seed
              << " iterations=" << iterations << '\n';
    return 0;
}

std::vector<double> ExactPairRow(const gst::Graph& graph,
                                 int pair_mask,
                                 const std::vector<std::vector<double>>& group_distance)
{
    const int a = FirstBit(pair_mask);
    const int b = FirstBit(pair_mask ^ (1 << a));
    std::vector<double> distance(graph.n + 1, gst::fp::kInf);
    Heap heap;
    for (int v = 1; v <= graph.n; ++v)
    {
        distance[v] = group_distance[a][v] + group_distance[b][v];
        heap.push({distance[v], v});
    }
    while (!heap.empty())
    {
        const auto [d, u] = heap.top();
        heap.pop();
        if (d != distance[u])
            continue;
        for (const auto& edge : graph.adj[u])
        {
            const double next = d + edge.w;
            if (next < distance[edge.to])
            {
                distance[edge.to] = next;
                heap.push({next, edge.to});
            }
        }
    }
    return distance;
}

PairResult ProbePair(const gst::Graph& graph,
                     const gst::Query& query,
                     int pair_mask,
                     double optimum,
                     const std::vector<std::vector<double>>& group_distance,
                     const gst::methods::test19::MetricTspLowerBound& tsp)
{
    PairResult result;
    result.mask = pair_mask;
    const auto [cap, portal_group] = PairReplacementCap(pair_mask, query, group_distance);
    result.cap = cap;
    result.portal_group = portal_group;

    const int full_mask = (1 << query.groups.size()) - 1;
    const int remaining = full_mask ^ pair_mask;
    const std::vector<double> row = ExactPairRow(graph, pair_mask, group_distance);
    for (int v = 1; v <= graph.n; ++v)
    {
        if (row[v] >= gst::fp::kInf)
            continue;
        ++result.all_states;
        const bool baseline =
            row[v] + tsp.TourHalf(remaining, v, group_distance) <= optimum + gst::fp::kEps;
        const bool capped = row[v] <= cap + gst::fp::kEps;
        result.baseline_states += baseline;
        result.capped_states += capped;
        result.baseline_capped_states += baseline && capped;
    }
    return result;
}

void ProbeQuery(const gst::Graph& graph, const gst::Query& query, int query_id)
{
    const int g = static_cast<int>(query.groups.size());
    if (g < 3 || g > 20)
        throw std::runtime_error("group replacement probe requires 3 <= g <= 20");

    const auto begin = Clock::now();
    const auto exact = gst::methods::release_v1::SolveOneQuery(graph, query);
    if (!exact.feasible)
        throw std::runtime_error("query is infeasible");

    const auto distance_begin = Clock::now();
    const auto group_distance = ComputeGroupDistances(graph, query);
    const double distance_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - distance_begin).count();
    const auto metric = BuildGroupMetric(query, group_distance);
    gst::methods::test19::MetricTspLowerBound tsp;
    tsp.Build(metric, (1 << g) - 1);

    std::vector<PairResult> pairs;
    long long all_states = 0;
    long long baseline_states = 0;
    long long capped_states = 0;
    long long baseline_capped_states = 0;
    for (int a = 0; a < g; ++a)
    {
        for (int b = a + 1; b < g; ++b)
        {
            PairResult result = ProbePair(
                graph, query, (1 << a) | (1 << b), exact.best_weight, group_distance, tsp);
            all_states += result.all_states;
            baseline_states += result.baseline_states;
            capped_states += result.capped_states;
            baseline_capped_states += result.baseline_capped_states;
            pairs.push_back(result);
        }
    }

    std::sort(pairs.begin(), pairs.end(), [](const PairResult& left, const PairResult& right)
    {
        const double left_ratio = left.baseline_states
                                      ? static_cast<double>(left.baseline_capped_states) /
                                            left.baseline_states
                                      : 1.0;
        const double right_ratio = right.baseline_states
                                       ? static_cast<double>(right.baseline_capped_states) /
                                             right.baseline_states
                                       : 1.0;
        return left_ratio < right_ratio;
    });

    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    std::cout << std::fixed << std::setprecision(6)
              << "query=" << query_id
              << " n=" << graph.n
              << " m=" << graph.m
              << " g=" << g
              << " optimum=" << exact.best_weight
              << " pairs=" << pairs.size()
              << " all=" << all_states
              << " baseline=" << baseline_states
              << " cap_only=" << capped_states
              << " baseline_cap=" << baseline_capped_states
              << " cap_pruned=" << (baseline_states - baseline_capped_states)
              << " cap_pruned_pct="
              << Percent(baseline_states - baseline_capped_states, baseline_states)
              << " group_dist_ms=" << distance_ms
              << " total_ms=" << elapsed_ms << '\n';

    const int report_count = std::min<int>(5, pairs.size());
    for (int i = 0; i < report_count; ++i)
    {
        const PairResult& pair = pairs[i];
        std::cout << "  strongest mask=" << pair.mask
                  << " portal_group=" << pair.portal_group
                  << " cap=" << pair.cap
                  << " baseline=" << pair.baseline_states
                  << " kept=" << pair.baseline_capped_states
                  << " pruned_pct="
                  << Percent(pair.baseline_states - pair.baseline_capped_states,
                             pair.baseline_states)
                  << '\n';
    }
}

void ProbeSeedSignal(const gst::Graph& graph,
                     const gst::Query& query,
                     int query_id,
                     double known_optimum)
{
    const int g = static_cast<int>(query.groups.size());
    if (g < 3 || g > 20)
        throw std::runtime_error("group replacement probe requires 3 <= g <= 20");

    const auto begin = Clock::now();
    const auto group_distance = ComputeGroupDistances(graph, query);
    const auto metric = BuildGroupMetric(query, group_distance);
    gst::methods::test19::MetricTspLowerBound tsp;
    tsp.Build(metric, (1 << g) - 1);
    const auto representative_degree = BuildRepresentativeDegreeBounds(query, group_distance);

    const int full_mask = (1 << g) - 1;
    long long all_seeds = 0;
    long long baseline_seeds = 0;
    long long capped_seeds = 0;
    long long baseline_capped_seeds = 0;
    long long representative_degree_seeds = 0;
    for (int a = 0; a < g; ++a)
    {
        for (int b = a + 1; b < g; ++b)
        {
            const int pair_mask = (1 << a) | (1 << b);
            const auto [cap, ignored_group] =
                PairReplacementCap(pair_mask, query, group_distance);
            const int remaining = full_mask ^ pair_mask;
            for (int v = 1; v <= graph.n; ++v)
            {
                const double seed = group_distance[a][v] + group_distance[b][v];
                const bool baseline =
                    seed + tsp.TourHalf(remaining, v, group_distance) <=
                    known_optimum + gst::fp::kEps;
                const bool capped = seed <= cap + gst::fp::kEps;
                const bool representative_kept =
                    seed + std::max(tsp.TourHalf(remaining, v, group_distance),
                                    representative_degree[remaining]) <=
                    known_optimum + gst::fp::kEps;
                ++all_seeds;
                baseline_seeds += baseline;
                capped_seeds += capped;
                baseline_capped_seeds += baseline && capped;
                representative_degree_seeds += representative_kept;
            }
        }
    }
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    std::cout << std::fixed << std::setprecision(6)
              << "seed_signal query=" << query_id
              << " n=" << graph.n
              << " m=" << graph.m
              << " g=" << g
              << " known_optimum=" << known_optimum
              << " all=" << all_seeds
              << " baseline=" << baseline_seeds
              << " cap_only=" << capped_seeds
              << " baseline_cap=" << baseline_capped_seeds
              << " cap_pruned=" << (baseline_seeds - baseline_capped_seeds)
              << " cap_pruned_pct="
              << Percent(baseline_seeds - baseline_capped_seeds, baseline_seeds)
              << " representative_degree=" << representative_degree_seeds
              << " representative_pruned=" << (baseline_seeds - representative_degree_seeds)
              << " representative_pruned_pct="
              << Percent(baseline_seeds - representative_degree_seeds, baseline_seeds)
              << " total_ms=" << elapsed_ms << '\n';
}

PairLayerResult ProbePairLayer(
    const gst::Graph& graph,
    int pair_mask,
    double known_optimum,
    double cap,
    const std::vector<std::vector<double>>& group_distance,
    const gst::methods::test19::MetricTspLowerBound& tsp)
{
    const int a = FirstBit(pair_mask);
    const int b = FirstBit(pair_mask ^ (1 << a));
    const int full_mask = (1 << group_distance.size()) - 1;
    const int remaining = full_mask ^ pair_mask;
    std::vector<double> distance(graph.n + 1, gst::fp::kInf);
    std::vector<double> heuristic(graph.n + 1);

    struct AstarItem
    {
        double key = 0.0;
        double distance = 0.0;
        int vertex = 0;
    };
    struct AstarGreater
    {
        bool operator()(const AstarItem& left, const AstarItem& right) const
        {
            if (left.key != right.key)
                return left.key > right.key;
            if (left.distance != right.distance)
                return left.distance > right.distance;
            return left.vertex > right.vertex;
        }
    };
    std::priority_queue<AstarItem, std::vector<AstarItem>, AstarGreater> heap;
    PairLayerResult result;
    result.mask = pair_mask;
    result.cap = cap;

    for (int v = 1; v <= graph.n; ++v)
    {
        heuristic[v] = tsp.TourHalf(remaining, v, group_distance);
        const double seed = group_distance[a][v] + group_distance[b][v];
        if (seed + heuristic[v] <= known_optimum + gst::fp::kEps)
        {
            distance[v] = seed;
            heap.push({seed + heuristic[v], seed, v});
            ++result.queue_pushes;
        }
    }
    while (!heap.empty())
    {
        const AstarItem current = heap.top();
        heap.pop();
        if (current.distance != distance[current.vertex] ||
            current.key > known_optimum + gst::fp::kEps)
            continue;
        ++result.settled;
        result.cap_kept += current.distance <= cap + gst::fp::kEps;
        for (const auto& edge : graph.adj[current.vertex])
        {
            const double next = current.distance + edge.w;
            if (next >= distance[edge.to] ||
                next + heuristic[edge.to] > known_optimum + gst::fp::kEps)
                continue;
            distance[edge.to] = next;
            heap.push({next + heuristic[edge.to], next, edge.to});
            ++result.queue_pushes;
        }
    }
    return result;
}

void ProbePairLayerSignal(const gst::Graph& graph,
                          const gst::Query& query,
                          int query_id,
                          double known_optimum)
{
    const int g = static_cast<int>(query.groups.size());
    if (g < 3 || g > 20)
        throw std::runtime_error("group replacement probe requires 3 <= g <= 20");

    const auto begin = Clock::now();
    const auto group_distance = ComputeGroupDistances(graph, query);
    const auto metric = BuildGroupMetric(query, group_distance);
    gst::methods::test19::MetricTspLowerBound tsp;
    tsp.Build(metric, (1 << g) - 1);

    long long settled = 0;
    long long cap_kept = 0;
    long long queue_pushes = 0;
    int completed_pairs = 0;
    for (int a = 0; a < g; ++a)
    {
        for (int b = a + 1; b < g; ++b)
        {
            const int pair_mask = (1 << a) | (1 << b);
            const auto [cap, ignored_group] =
                PairReplacementCap(pair_mask, query, group_distance);
            const PairLayerResult row = ProbePairLayer(
                graph, pair_mask, known_optimum, cap, group_distance, tsp);
            settled += row.settled;
            cap_kept += row.cap_kept;
            queue_pushes += row.queue_pushes;
            ++completed_pairs;
            std::cout << std::fixed << std::setprecision(6)
                      << "pair_progress=" << completed_pairs
                      << " mask=" << pair_mask
                      << " settled=" << row.settled
                      << " cap_kept=" << row.cap_kept
                      << " cap_pruned_pct="
                      << Percent(row.settled - row.cap_kept, row.settled) << std::endl;
        }
    }
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    std::cout << std::fixed << std::setprecision(6)
              << "pair_layer query=" << query_id
              << " n=" << graph.n
              << " m=" << graph.m
              << " g=" << g
              << " known_optimum=" << known_optimum
              << " pairs=" << completed_pairs
              << " settled=" << settled
              << " cap_kept=" << cap_kept
              << " cap_pruned=" << (settled - cap_kept)
              << " cap_pruned_pct=" << Percent(settled - cap_kept, settled)
              << " queue_pushes=" << queue_pushes
              << " total_ms=" << elapsed_ms << '\n';
}

void ProbeExactPairFutureSignal(const gst::Graph& graph,
                                const gst::Query& query,
                                int query_id,
                                double known_optimum)
{
    const int g = static_cast<int>(query.groups.size());
    if (g < 4 || g > 20)
        throw std::runtime_error("exact pair future probe requires 4 <= g <= 20");

    const auto begin = Clock::now();
    const auto group_distance = ComputeGroupDistances(graph, query);
    const auto metric = BuildGroupMetric(query, group_distance);
    gst::methods::test19::MetricTspLowerBound tsp;
    tsp.Build(metric, (1 << g) - 1);
    const auto representative_degree = BuildRepresentativeDegreeBounds(query, group_distance);
    const auto representative_path_begin = Clock::now();
    RepresentativePathLowerBound representative_path;
    representative_path.Build(query, group_distance);
    const double representative_path_ms =
        std::chrono::duration<double, std::milli>(
            Clock::now() - representative_path_begin).count();

    std::vector<int> pair_masks;
    std::vector<std::vector<int>> pair_index(g, std::vector<int>(g, -1));
    for (int a = 0; a < g; ++a)
    {
        for (int b = a + 1; b < g; ++b)
        {
            pair_index[a][b] = static_cast<int>(pair_masks.size());
            pair_masks.push_back((1 << a) | (1 << b));
        }
    }

    const auto pair_begin = Clock::now();
    std::vector<std::vector<double>> pair_rows;
    pair_rows.reserve(pair_masks.size());
    for (int mask : pair_masks)
        pair_rows.push_back(ExactPairRow(graph, mask, group_distance));
    const double pair_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - pair_begin).count();

    const int full_mask = (1 << g) - 1;
    long long tsp_states = 0;
    long long exact_pair_states = 0;
    long long representative_degree_states = 0;
    long long representative_path_states = 0;
    for (int state_index = 0; state_index < static_cast<int>(pair_masks.size()); ++state_index)
    {
        const int state_mask = pair_masks[state_index];
        const int remaining = full_mask ^ state_mask;
        for (int v = 1; v <= graph.n; ++v)
        {
            const double state_cost = pair_rows[state_index][v];
            const double tsp_bound = tsp.TourHalf(remaining, v, group_distance);
            if (state_cost + tsp_bound > known_optimum + gst::fp::kEps)
                continue;
            ++tsp_states;
            if (state_cost + std::max(tsp_bound, representative_degree[remaining]) <=
                known_optimum + gst::fp::kEps)
                ++representative_degree_states;
            if (state_cost +
                    std::max(tsp_bound,
                             representative_path.TourHalf(remaining, v, group_distance)) <=
                known_optimum + gst::fp::kEps)
                ++representative_path_states;

            double pair_bound = 0.0;
            for (int a = 0; a < g; ++a)
            {
                if (!((remaining >> a) & 1))
                    continue;
                for (int b = a + 1; b < g; ++b)
                {
                    if (!((remaining >> b) & 1))
                        continue;
                    pair_bound = std::max(pair_bound, pair_rows[pair_index[a][b]][v]);
                }
            }
            if (state_cost + std::max(tsp_bound, pair_bound) <=
                known_optimum + gst::fp::kEps)
                ++exact_pair_states;
        }
    }

    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    std::cout << std::fixed << std::setprecision(6)
              << "exact_pair_future query=" << query_id
              << " n=" << graph.n
              << " m=" << graph.m
              << " g=" << g
              << " known_optimum=" << known_optimum
              << " pair_rows=" << pair_rows.size()
              << " tsp_states=" << tsp_states
              << " exact_pair_states=" << exact_pair_states
              << " pruned=" << (tsp_states - exact_pair_states)
              << " pruned_pct=" << Percent(tsp_states - exact_pair_states, tsp_states)
              << " representative_degree_states=" << representative_degree_states
              << " representative_pruned=" << (tsp_states - representative_degree_states)
              << " representative_pruned_pct="
              << Percent(tsp_states - representative_degree_states, tsp_states)
              << " representative_path_states=" << representative_path_states
              << " representative_path_pruned=" << (tsp_states - representative_path_states)
              << " representative_path_pruned_pct="
              << Percent(tsp_states - representative_path_states, tsp_states)
              << " representative_path_precompute_ms=" << representative_path_ms
              << " representative_path_peak_bytes="
              << representative_path.PeakWorkingBytes()
              << " representative_path_resident_bytes="
              << representative_path.ResidentBytes()
              << " pair_precompute_ms=" << pair_ms
              << " total_ms=" << elapsed_ms << '\n';
}

void PrintUsage(const char* executable)
{
    std::cerr << "usage: " << executable
              << " <data_root> <graph_selector> <query_selector>"
              << " [query_begin_1based=1] [query_limit=1]\n"
              << "   or: " << executable
              << " --self-check [seed=1] [iterations=1000] [min_g=2] [max_g=8] [max_n=10]\n"
              << "   or: " << executable
              << " --second-order-self-check [seed=1] [iterations=1000]"
              << " [min_g=2] [max_g=8] [max_n=10]\n"
              << "   or: " << executable
              << " --seed-signal <data_root> <graph_selector> <query_selector>"
              << " <known_optimum> [query_begin_1based=1]\n"
              << "   or: " << executable
              << " --pair-layer-signal <data_root> <graph_selector> <query_selector>"
              << " <known_optimum> [query_begin_1based=1]\n"
              << "   or: " << executable
              << " --exact-pair-future <data_root> <graph_selector> <query_selector>"
              << " <known_optimum> [query_begin_1based=1]\n";
}
}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc >= 2 && std::string(argv[1]) == "--self-check")
        {
            const std::uint64_t seed = argc >= 3 ? std::stoull(argv[2]) : 1;
            const int iterations = argc >= 4 ? std::stoi(argv[3]) : 1000;
            const int min_group_count = argc >= 5 ? std::stoi(argv[4]) : 2;
            const int max_group_count = argc >= 6 ? std::stoi(argv[5]) : 8;
            const int max_vertex_count = argc >= 7 ? std::stoi(argv[6]) : 10;
            return RunSelfCheck(
                seed, iterations, min_group_count, max_group_count, max_vertex_count);
        }
        if (argc >= 2 && std::string(argv[1]) == "--second-order-self-check")
        {
            const std::uint64_t seed = argc >= 3 ? std::stoull(argv[2]) : 1;
            const int iterations = argc >= 4 ? std::stoi(argv[3]) : 1000;
            const int min_group_count = argc >= 5 ? std::stoi(argv[4]) : 2;
            const int max_group_count = argc >= 6 ? std::stoi(argv[5]) : 8;
            const int max_vertex_count = argc >= 7 ? std::stoi(argv[6]) : 10;
            return RunSecondOrderSelfCheck(
                seed, iterations, min_group_count, max_group_count, max_vertex_count);
        }
        if (argc >= 2 &&
            (std::string(argv[1]) == "--seed-signal" ||
             std::string(argv[1]) == "--pair-layer-signal" ||
             std::string(argv[1]) == "--exact-pair-future"))
        {
            if (argc < 6 || argc > 7)
            {
                PrintUsage(argv[0]);
                return 2;
            }
            const std::string data_root = argv[2];
            const std::string graph_selector = argv[3];
            const std::string query_selector = argv[4];
            const double known_optimum = std::stod(argv[5]);
            const int query_begin = argc >= 7 ? std::stoi(argv[6]) : 1;
            const std::string graph_folder = gst::ResolveGraphFolder(data_root, graph_selector);
            const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
            const std::vector<gst::Query> queries =
                gst::LoadQueriesFromFolder(graph_folder, query_selector);
            if (query_begin < 1 || query_begin > static_cast<int>(queries.size()))
                throw std::runtime_error("query_begin out of range");
            if (std::string(argv[1]) == "--seed-signal")
                ProbeSeedSignal(graph, queries[query_begin - 1], query_begin, known_optimum);
            else if (std::string(argv[1]) == "--pair-layer-signal")
                ProbePairLayerSignal(graph, queries[query_begin - 1], query_begin, known_optimum);
            else
                ProbeExactPairFutureSignal(
                    graph, queries[query_begin - 1], query_begin, known_optimum);
            return 0;
        }
        if (argc < 4 || argc > 6)
        {
            PrintUsage(argv[0]);
            return 2;
        }
        const std::string data_root = argv[1];
        const std::string graph_selector = argv[2];
        const std::string query_selector = argv[3];
        const int query_begin = argc >= 5 ? std::stoi(argv[4]) : 1;
        const int query_limit = argc >= 6 ? std::stoi(argv[5]) : 1;

        const std::string graph_folder = gst::ResolveGraphFolder(data_root, graph_selector);
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const std::vector<gst::Query> queries =
            gst::LoadQueriesFromFolder(graph_folder, query_selector);
        if (query_begin < 1 || query_begin > static_cast<int>(queries.size()))
            throw std::runtime_error("query_begin out of range");
        const int end = query_limit < 0
                            ? static_cast<int>(queries.size())
                            : std::min<int>(queries.size(), query_begin - 1 + query_limit);
        for (int index = query_begin - 1; index < end; ++index)
            ProbeQuery(graph, queries[index], index + 1);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
