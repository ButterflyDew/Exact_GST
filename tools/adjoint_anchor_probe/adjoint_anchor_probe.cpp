#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
constexpr double kInf = 1e100;
constexpr double kEps = 1e-8;
using Row = std::vector<double>;

struct Edge
{
    int u = 0;
    int v = 0;
    double w = 0.0;
};

struct Instance
{
    int n = 0;
    std::vector<Edge> edges;
    std::vector<std::vector<int>> groups;
};

int Bits(int mask)
{
    int result = 0;
    while (mask)
    {
        mask &= mask - 1;
        ++result;
    }
    return result;
}

int FirstBit(int mask)
{
    int bit = 0;
    while (!(mask & (1 << bit)))
        ++bit;
    return bit;
}

std::vector<Row> Metric(const Instance& instance)
{
    std::vector<Row> metric(instance.n, Row(instance.n, kInf));
    for (int vertex = 0; vertex < instance.n; ++vertex)
        metric[vertex][vertex] = 0.0;
    for (const Edge& edge : instance.edges)
    {
        metric[edge.u][edge.v] = std::min(metric[edge.u][edge.v], edge.w);
        metric[edge.v][edge.u] = std::min(metric[edge.v][edge.u], edge.w);
    }
    for (int middle = 0; middle < instance.n; ++middle)
        for (int from = 0; from < instance.n; ++from)
            for (int to = 0; to < instance.n; ++to)
                metric[from][to] = std::min(
                    metric[from][to],
                    metric[from][middle] + metric[middle][to]);
    return metric;
}

Row Close(const Row& values, const std::vector<Row>& metric)
{
    Row result(values.size(), kInf);
    for (int root = 0; root < static_cast<int>(values.size()); ++root)
        for (int vertex = 0; vertex < static_cast<int>(values.size()); ++vertex)
            result[vertex] = std::min(
                result[vertex], values[root] + metric[root][vertex]);
    return result;
}

double Inner(const Row& first, const Row& second)
{
    double result = kInf;
    for (size_t vertex = 0; vertex < first.size(); ++vertex)
        result = std::min(result, first[vertex] + second[vertex]);
    return result;
}

void MinInto(Row& target, const Row& source)
{
    for (size_t vertex = 0; vertex < target.size(); ++vertex)
        target[vertex] = std::min(target[vertex], source[vertex]);
}

std::vector<Row> GroupDistance(
    const Instance& instance,
    const std::vector<Row>& metric)
{
    std::vector<Row> result(
        instance.groups.size(), Row(instance.n, kInf));
    for (size_t group = 0; group < instance.groups.size(); ++group)
        for (int vertex = 0; vertex < instance.n; ++vertex)
            for (int terminal : instance.groups[group])
                result[group][vertex] = std::min(
                    result[group][vertex], metric[terminal][vertex]);
    return result;
}

std::vector<Row> RootedRows(
    const std::vector<Row>& labels,
    const std::vector<Row>& metric,
    int maximum_size)
{
    const int label_count = static_cast<int>(labels.size());
    const int state_count = 1 << label_count;
    const int n = static_cast<int>(metric.size());
    std::vector<Row> rows(state_count, Row(n, kInf));
    rows[0].assign(n, 0.0);
    for (int mask = 1; mask < state_count; ++mask)
    {
        if (Bits(mask) > maximum_size)
            continue;
        if (!(mask & (mask - 1)))
        {
            rows[mask] = labels[FirstBit(mask)];
            continue;
        }
        Row seed(n, kInf);
        const int pivot = mask & -mask;
        for (int left = (mask - 1) & mask; left;
             left = (left - 1) & mask)
        {
            if (!(left & pivot))
                continue;
            const int right = mask ^ left;
            if (!right)
                continue;
            for (int vertex = 0; vertex < n; ++vertex)
                seed[vertex] = std::min(
                    seed[vertex], rows[left][vertex] + rows[right][vertex]);
        }
        rows[mask] = Close(seed, metric);
    }
    return rows;
}

struct Circuit
{
    int k = 0;
    int half = 0;
    int anchored_limit = 0;
    int full = 0;
    std::vector<Row> ordinary;
    std::vector<Row> anchored;
    std::vector<Row> terminal;
    double forward_value = kInf;
};

Circuit BuildCircuit(
    const std::vector<Row>& group_distance,
    const std::vector<Row>& metric)
{
    const int g = static_cast<int>(group_distance.size());
    const int n = static_cast<int>(metric.size());
    Circuit circuit;
    circuit.k = g - 1;
    circuit.half = g / 2;
    circuit.anchored_limit = std::max(0, circuit.half - 1);
    circuit.full = (1 << circuit.k) - 1;
    const int state_count = 1 << circuit.k;

    std::vector<Row> nonanchor(
        group_distance.begin() + 1, group_distance.end());
    circuit.ordinary = RootedRows(nonanchor, metric, circuit.half);
    circuit.anchored.assign(state_count, Row(n, kInf));
    circuit.anchored[0] = group_distance[0];
    for (int size = 1; size <= circuit.anchored_limit; ++size)
        for (int mask = 1; mask < state_count; ++mask)
        {
            if (Bits(mask) != size)
                continue;
            Row seed(n, kInf);
            for (int block = mask; block; block = (block - 1) & mask)
            {
                const int predecessor = mask ^ block;
                for (int vertex = 0; vertex < n; ++vertex)
                    seed[vertex] = std::min(
                        seed[vertex],
                        circuit.anchored[predecessor][vertex] +
                            circuit.ordinary[block][vertex]);
            }
            circuit.anchored[mask] = Close(seed, metric);
        }

    circuit.terminal.assign(state_count, Row(n, kInf));
    for (int anchored = 0; anchored < state_count; ++anchored)
    {
        if (Bits(anchored) > circuit.anchored_limit)
            continue;
        const int remaining = circuit.full ^ anchored;
        for (int left = remaining;; left = (left - 1) & remaining)
        {
            const int right = remaining ^ left;
            if (Bits(left) <= circuit.half && Bits(right) <= circuit.half)
                for (int vertex = 0; vertex < n; ++vertex)
                    circuit.terminal[anchored][vertex] = std::min(
                        circuit.terminal[anchored][vertex],
                        circuit.ordinary[left][vertex] +
                            circuit.ordinary[right][vertex]);
            if (!left)
                break;
        }
        circuit.forward_value = std::min(
            circuit.forward_value,
            Inner(circuit.anchored[anchored], circuit.terminal[anchored]));
    }
    return circuit;
}

bool CheckTransposedTerminal(
    const Circuit& circuit,
    const std::vector<Row>& group_distance,
    bool report)
{
    struct Value
    {
        int mask = 0;
        double distance = kInf;
        double reduced = kInf;
    };

    const int n = static_cast<int>(group_distance.front().size());
    const int state_count = 1 << circuit.k;
    std::vector<double> potential(state_count);
    std::vector<double> actual(state_count, kInf);
    std::vector<double> submask_actual(state_count, kInf);
    std::vector<double> distance_by_mask(state_count, kInf);
    std::vector<double> reduced_by_mask(state_count, kInf);
    std::vector<Value> values;
    for (int vertex = 0; vertex < n; ++vertex)
    {
        potential[0] = 0.0;
        for (int mask = 1; mask < state_count; ++mask)
        {
            const int bit = mask & -mask;
            potential[mask] = potential[mask ^ bit] +
                group_distance[FirstBit(bit) + 1][vertex] /
                    static_cast<double>(circuit.k + 1);
        }
        const double full_potential =
            group_distance[0][vertex] /
                static_cast<double>(circuit.k + 1) +
            potential[circuit.full];

        values.clear();
        std::fill(distance_by_mask.begin(), distance_by_mask.end(), kInf);
        std::fill(reduced_by_mask.begin(), reduced_by_mask.end(), kInf);
        for (int mask = 1; mask < state_count; ++mask)
            if (Bits(mask) <= circuit.half &&
                circuit.ordinary[mask][vertex] < kInf)
            {
                const double distance = circuit.ordinary[mask][vertex];
                const double reduced = distance - potential[mask];
                values.push_back({mask, distance, reduced});
                distance_by_mask[mask] = distance;
                reduced_by_mask[mask] = reduced;
            }
        std::sort(values.begin(), values.end(), [](const Value& left,
                                                    const Value& right)
        {
            if (left.reduced != right.reduced)
                return left.reduced < right.reduced;
            return left.mask < right.mask;
        });

        double minimum = kInf;
        double maximum = 0.0;
        for (int target = 0; target < state_count; ++target)
        {
            if (Bits(target) > circuit.anchored_limit ||
                circuit.terminal[target][vertex] >= kInf)
                continue;
            const double total = circuit.terminal[target][vertex] +
                full_potential - potential[circuit.full ^ target];
            minimum = std::min(minimum, total);
            maximum = std::max(maximum, total);
        }
        if (minimum >= kInf)
            continue;
        const std::vector<double> budgets = {
            minimum - 0.5,
            minimum,
            (minimum + maximum) * 0.5,
            maximum,
            circuit.forward_value};
        for (double best : budgets)
        {
            std::fill(actual.begin(), actual.end(), kInf);
            std::fill(submask_actual.begin(), submask_actual.end(), kInf);
            const double reduced_budget = best - full_potential;
            if (reduced_budget >= -kEps)
            {
                for (const Value& entry : values)
                {
                    if (entry.reduced > reduced_budget + kEps)
                        break;
                    const int target = circuit.full ^ entry.mask;
                    actual[target] = std::min(actual[target], entry.distance);
                }
                for (size_t left = 0; left < values.size(); ++left)
                {
                    if (left + 1 == values.size() ||
                        values[left].reduced + values[left + 1].reduced >
                            reduced_budget + kEps)
                        break;
                    for (size_t right = left + 1; right < values.size(); ++right)
                    {
                        if (values[left].reduced + values[right].reduced >
                            reduced_budget + kEps)
                            break;
                        if (values[left].mask & values[right].mask)
                            continue;
                        const int target = circuit.full ^
                            (values[left].mask | values[right].mask);
                        actual[target] = std::min(
                            actual[target],
                            values[left].distance + values[right].distance);
                    }
                }

                for (const Value& left : values)
                {
                    const int complement = circuit.full ^ left.mask;
                    for (int right = complement; right;
                         right = (right - 1) & complement)
                    {
                        if (right <= left.mask || distance_by_mask[right] >= kInf ||
                            left.reduced + reduced_by_mask[right] >
                                reduced_budget + kEps)
                            continue;
                        const int target = circuit.full ^ (left.mask | right);
                        submask_actual[target] = std::min(
                            submask_actual[target],
                            left.distance + distance_by_mask[right]);
                    }
                }
                for (const Value& entry : values)
                    if (entry.reduced <= reduced_budget + kEps)
                    {
                        const int target = circuit.full ^ entry.mask;
                        submask_actual[target] = std::min(
                            submask_actual[target], entry.distance);
                    }
            }

            for (int target = 0; target < state_count; ++target)
            {
                if (Bits(target) > circuit.anchored_limit)
                    continue;
                const double prefix = full_potential -
                    potential[circuit.full ^ target];
                const double expected =
                    circuit.terminal[target][vertex] + prefix <= best + kEps
                        ? circuit.terminal[target][vertex]
                        : kInf;
                const double transposed =
                    actual[target] + prefix <= best + kEps
                        ? actual[target]
                        : kInf;
                const double submask_transposed =
                    submask_actual[target] + prefix <= best + kEps
                        ? submask_actual[target]
                        : kInf;
                const bool same_infinity =
                    (expected >= kInf) == (transposed >= kInf) &&
                    (expected >= kInf) == (submask_transposed >= kInf);
                if (!same_infinity || (expected < kInf &&
                    (std::abs(expected - transposed) > kEps ||
                     std::abs(expected - submask_transposed) > kEps)))
                {
                    if (report)
                        std::cout << "transpose vertex=" << vertex
                                  << " target=" << target
                                  << " best=" << best
                                  << " expected=" << expected
                                  << " sorted=" << transposed
                                  << " submask=" << submask_transposed << '\n';
                    return false;
                }
            }
        }
    }
    return true;
}

double AdjointCut(
    const Circuit& circuit,
    const std::vector<Row>& metric,
    int cut)
{
    const int n = static_cast<int>(metric.size());
    const int state_count = 1 << circuit.k;
    std::vector<Row> backward(state_count, Row(n, kInf));
    std::vector<Row> boundary(state_count, Row(n, kInf));
    for (int mask = 0; mask < state_count; ++mask)
    {
        if (Bits(mask) > circuit.anchored_limit)
            continue;
        if (Bits(mask) > cut)
            backward[mask] = circuit.terminal[mask];
        else
            boundary[mask] = circuit.terminal[mask];
    }

    for (int size = circuit.anchored_limit; size > cut; --size)
        for (int target = 1; target < state_count; ++target)
        {
            if (Bits(target) != size)
                continue;
            const Row closed = Close(backward[target], metric);
            for (int predecessor = (target - 1) & target;;
                 predecessor = (predecessor - 1) & target)
            {
                const int block = target ^ predecessor;
                Row contribution(n, kInf);
                for (int vertex = 0; vertex < n; ++vertex)
                    contribution[vertex] =
                        circuit.ordinary[block][vertex] + closed[vertex];
                if (Bits(predecessor) > cut)
                    MinInto(backward[predecessor], contribution);
                else
                    MinInto(boundary[predecessor], contribution);
                if (!predecessor)
                    break;
            }
        }

    double result = kInf;
    for (int mask = 0; mask < state_count; ++mask)
        if (Bits(mask) <= cut)
            result = std::min(
                result, Inner(circuit.anchored[mask], boundary[mask]));
    return result;
}

double Exact(
    const std::vector<Row>& group_distance,
    const std::vector<Row>& metric)
{
    const std::vector<Row> rows = RootedRows(
        group_distance, metric, static_cast<int>(group_distance.size()));
    return *std::min_element(rows.back().begin(), rows.back().end());
}

bool Check(const Instance& instance, bool report)
{
    const auto metric = Metric(instance);
    const auto group_distance = GroupDistance(instance, metric);
    const Circuit circuit = BuildCircuit(group_distance, metric);
    const double exact = Exact(group_distance, metric);
    bool ok = std::abs(exact - circuit.forward_value) <= kEps &&
        CheckTransposedTerminal(circuit, group_distance, report);
    for (int cut = 0; cut <= circuit.anchored_limit; ++cut)
    {
        const double adjoint = AdjointCut(circuit, metric, cut);
        if (std::abs(adjoint - circuit.forward_value) > kEps)
        {
            ok = false;
            if (report)
                std::cout << "cut=" << cut << " adjoint=" << adjoint
                          << " forward=" << circuit.forward_value << '\n';
            break;
        }
    }
    if (!ok && report)
    {
        std::cout << "exact=" << exact
                  << " forward=" << circuit.forward_value
                  << " n=" << instance.n
                  << " g=" << instance.groups.size() << " edges";
        for (const Edge& edge : instance.edges)
            std::cout << " (" << edge.u << ',' << edge.v << ',' << edge.w << ')';
        std::cout << " groups";
        for (const auto& group : instance.groups)
        {
            std::cout << " [";
            for (size_t index = 0; index < group.size(); ++index)
            {
                if (index)
                    std::cout << ',';
                std::cout << group[index];
            }
            std::cout << ']';
        }
        std::cout << '\n';
    }
    return ok;
}

Instance Test96()
{
    Instance instance;
    instance.n = 13;
    instance.edges = {
        {1,0,20}, {2,0,4}, {3,0,17}, {4,1,18},
        {5,1,17}, {6,2,10}, {7,4,20}, {8,7,3},
        {9,4,4}, {10,5,13}, {11,2,13}, {12,3,19},
        {12,4,12}, {9,7,2}, {12,0,18}, {7,10,15}};
    instance.groups = {
        {5,3}, {9,8}, {8,5}, {3,9}, {7,12}, {1,7}, {0,4},
        {10,11}, {2,6}, {12,10}, {11,1}, {6,0}, {4,2}};
    return instance;
}

Instance RandomInstance(
    std::mt19937_64& rng,
    int max_n,
    int max_g,
    int max_edges,
    int min_g)
{
    std::uniform_int_distribution<int> n_dist(4, max_n);
    Instance instance;
    instance.n = n_dist(rng);
    const int maximum_edges = instance.n * (instance.n - 1) / 2;
    std::uniform_int_distribution<int> edge_count_dist(
        instance.n - 1, std::min(maximum_edges, max_edges));
    const int edge_count = edge_count_dist(rng);
    std::vector<std::vector<char>> used(
        instance.n, std::vector<char>(instance.n));
    std::uniform_int_distribution<int> weight_dist(0, 20);
    for (int vertex = 1; vertex < instance.n; ++vertex)
    {
        std::uniform_int_distribution<int> parent_dist(0, vertex - 1);
        const int parent = parent_dist(rng);
        used[vertex][parent] = used[parent][vertex] = true;
        instance.edges.push_back(
            {vertex, parent, static_cast<double>(weight_dist(rng))});
    }
    std::uniform_int_distribution<int> vertex_dist(0, instance.n - 1);
    while (static_cast<int>(instance.edges.size()) < edge_count)
    {
        const int u = vertex_dist(rng);
        const int v = vertex_dist(rng);
        if (u == v || used[u][v])
            continue;
        used[u][v] = used[v][u] = true;
        instance.edges.push_back(
            {u, v, static_cast<double>(weight_dist(rng))});
    }

    std::uniform_int_distribution<int> g_dist(min_g, max_g);
    const int g = g_dist(rng);
    instance.groups.resize(g);
    std::uniform_int_distribution<int> candidate_count_dist(1, 2);
    for (auto& group : instance.groups)
    {
        const int count = std::min(instance.n, candidate_count_dist(rng));
        while (static_cast<int>(group.size()) < count)
        {
            const int vertex = vertex_dist(rng);
            if (std::find(group.begin(), group.end(), vertex) == group.end())
                group.push_back(vertex);
        }
    }
    return instance;
}
}  // namespace

int main(int argc, char** argv)
{
    const std::uint64_t seed =
        argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1;
    const int iterations = argc > 2 ? std::atoi(argv[2]) : 500;
    const int max_n = argc > 3 ? std::atoi(argv[3]) : 9;
    const int max_g = argc > 4 ? std::atoi(argv[4]) : 9;
    const int max_edges = argc > 5 ? std::atoi(argv[5]) : 15;
    const int min_g = argc > 6 ? std::atoi(argv[6]) : 3;

    if (!Check(Test96(), true))
        return 1;
    std::cout << "test96=ok exact=71\n";

    std::mt19937_64 rng(seed);
    for (int iteration = 1; iteration <= iterations; ++iteration)
    {
        const Instance instance = RandomInstance(
            rng, max_n, max_g, max_edges, min_g);
        if (!Check(instance, true))
        {
            std::cout << "FAILED seed=" << seed
                      << " iteration=" << iteration << '\n';
            return 1;
        }
    }
    std::cout << "ALL_OK seed=" << seed
              << " iterations=" << iterations << '\n';
    return 0;
}
