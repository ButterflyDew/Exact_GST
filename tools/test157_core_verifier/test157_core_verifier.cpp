#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
constexpr double kInf = 1e80;
constexpr double kEps = 1e-8;
using Row = std::vector<double>;
using Rows = std::vector<Row>;

struct Edge
{
    int u = 0;
    int v = 0;
    int w = 0;
};

struct Instance
{
    int n = 0;
    std::vector<Edge> edges;
    std::vector<std::vector<int>> groups;
};

struct Counters
{
    long long iwata_m0_checks = 0;
    long long branch_cells = 0;
    long long suffix_rows = 0;
    long long cuts = 0;
    long long transpose_cells = 0;
    long long brute_oracles = 0;
    long long decomposition_checks = 0;
};

int Bits(int mask)
{
    int count = 0;
    while (mask)
    {
        mask &= mask - 1;
        ++count;
    }
    return count;
}

int FirstBit(int mask)
{
    int bit = 0;
    while (!(mask & (1 << bit)))
        ++bit;
    return bit;
}

bool Equal(double left, double right)
{
    if (left >= kInf || right >= kInf)
        return left >= kInf && right >= kInf;
    return std::abs(left - right) <= kEps;
}

void RequireEqual(
    double left,
    double right,
    const std::string& label,
    int mask = -1,
    int vertex = -1)
{
    if (Equal(left, right))
        return;
    std::cerr << "TEST157_MISMATCH kind=" << label
              << " mask=" << mask
              << " vertex=" << vertex
              << " left=" << left
              << " right=" << right << '\n';
    throw std::runtime_error("Test157 verifier mismatch");
}

Rows Metric(const Instance& instance)
{
    Rows metric(instance.n, Row(instance.n, kInf));
    for (int vertex = 0; vertex < instance.n; ++vertex)
        metric[vertex][vertex] = 0.0;
    for (const Edge& edge : instance.edges)
    {
        metric[edge.u][edge.v] = std::min(
            metric[edge.u][edge.v], static_cast<double>(edge.w));
        metric[edge.v][edge.u] = std::min(
            metric[edge.v][edge.u], static_cast<double>(edge.w));
    }
    for (int middle = 0; middle < instance.n; ++middle)
        for (int from = 0; from < instance.n; ++from)
            for (int to = 0; to < instance.n; ++to)
                metric[from][to] = std::min(
                    metric[from][to],
                    metric[from][middle] + metric[middle][to]);
    return metric;
}

Row Close(const Row& values, const Rows& metric)
{
    Row result(values.size(), kInf);
    for (int source = 0; source < static_cast<int>(values.size()); ++source)
    {
        if (values[source] >= kInf)
            continue;
        for (int target = 0; target < static_cast<int>(values.size()); ++target)
            result[target] = std::min(
                result[target], values[source] + metric[source][target]);
    }
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

Rows GroupDistances(const Instance& instance, const Rows& metric)
{
    Rows distances(instance.groups.size(), Row(instance.n, kInf));
    for (int group = 0; group < static_cast<int>(instance.groups.size()); ++group)
        for (int vertex = 0; vertex < instance.n; ++vertex)
            for (int terminal : instance.groups[group])
                distances[group][vertex] = std::min(
                    distances[group][vertex], metric[terminal][vertex]);
    return distances;
}

Rows StandardRows(const Rows& labels, const Rows& metric, int maximum_size)
{
    const int count = static_cast<int>(labels.size());
    const int states = 1 << count;
    const int n = static_cast<int>(metric.size());
    Rows rows(states, Row(n, kInf));
    rows[0].assign(n, 0.0);
    for (int size = 1; size <= maximum_size; ++size)
        for (int mask = 1; mask < states; ++mask)
        {
            if (Bits(mask) != size)
                continue;
            if (!(mask & (mask - 1)))
            {
                rows[mask] = labels[FirstBit(mask)];
                continue;
            }
            Row seed(n, kInf);
            const int pivot = mask & -mask;
            const int domain = mask ^ pivot;
            for (int right = domain; right; right = (right - 1) & domain)
            {
                const int left = mask ^ right;
                for (int vertex = 0; vertex < n; ++vertex)
                    seed[vertex] = std::min(
                        seed[vertex],
                        rows[left][vertex] + rows[right][vertex]);
            }
            rows[mask] = Close(seed, metric);
        }
    return rows;
}

double RootMinimum(const Row& row)
{
    return *std::min_element(row.begin(), row.end());
}

double IwataHalfValue(const Rows& group_distance, const Rows& metric)
{
    const int g = static_cast<int>(group_distance.size());
    const int full = (1 << g) - 1;
    const Rows rows = StandardRows(group_distance, metric, g / 2);
    double answer = kInf;
    for (int first = full;; first = (first - 1) & full)
    {
        const int remaining = full ^ first;
        for (int second = remaining;; second = (second - 1) & remaining)
        {
            const int third = remaining ^ second;
            if (first <= second && second <= third &&
                Bits(first) <= g / 2 &&
                Bits(second) <= g / 2 &&
                Bits(third) <= g / 2)
            {
                for (int vertex = 0; vertex < static_cast<int>(metric.size()); ++vertex)
                    answer = std::min(
                        answer,
                        rows[first][vertex] +
                            rows[second][vertex] +
                            rows[third][vertex]);
            }
            if (!second)
                break;
        }
        if (!first)
            break;
    }
    return answer;
}

struct BranchRows
{
    Rows rows;
    Rows seed;
    std::vector<std::vector<unsigned char>> branch;
};

BranchRows RestrictedBranchRows(
    const Rows& labels,
    const Rows& metric,
    int maximum_size)
{
    const int count = static_cast<int>(labels.size());
    const int states = 1 << count;
    const int n = static_cast<int>(metric.size());
    BranchRows result;
    result.rows.assign(states, Row(n, kInf));
    result.seed.assign(states, Row(n, kInf));
    result.branch.assign(states, std::vector<unsigned char>(n));
    result.rows[0].assign(n, 0.0);
    for (int size = 1; size <= maximum_size; ++size)
        for (int mask = 1; mask < states; ++mask)
        {
            if (Bits(mask) != size)
                continue;
            if (size == 1)
            {
                result.rows[mask] = labels[FirstBit(mask)];
                std::fill(
                    result.branch[mask].begin(),
                    result.branch[mask].end(),
                    static_cast<unsigned char>(1));
                continue;
            }
            const int pivot = mask & -mask;
            const int domain = mask ^ pivot;
            for (int right = domain; right; right = (right - 1) & domain)
            {
                const int left = mask ^ right;
                for (int vertex = 0; vertex < n; ++vertex)
                {
                    if (!result.branch[right][vertex])
                        continue;
                    result.seed[mask][vertex] = std::min(
                        result.seed[mask][vertex],
                        result.rows[left][vertex] + result.rows[right][vertex]);
                }
            }
            result.rows[mask] = Close(result.seed[mask], metric);
            for (int vertex = 0; vertex < n; ++vertex)
                result.branch[mask][vertex] =
                    result.rows[mask][vertex] + kEps < result.seed[mask][vertex];
        }
    return result;
}

Row BranchRow(const BranchRows& rows, int mask)
{
    Row result(rows.rows[mask].size(), kInf);
    if (!mask)
        return result;
    for (size_t vertex = 0; vertex < result.size(); ++vertex)
        if (rows.branch[mask][vertex])
            result[vertex] = rows.rows[mask][vertex];
    return result;
}

struct Circuit
{
    int g = 0;
    int k = 0;
    int h = 0;
    int a = 0;
    int full = 0;
    Rows ordinary;
    BranchRows restricted;
    Rows branch;
    Rows anchored;
    Rows terminal;
    Rows suffix;
    Rows backward;
    double forward_value = kInf;
};

Circuit BuildCircuit(const Rows& group_distance, const Rows& metric)
{
    Circuit circuit;
    circuit.g = static_cast<int>(group_distance.size());
    circuit.k = circuit.g - 1;
    circuit.h = circuit.g / 2;
    circuit.a = std::max(0, circuit.h - 1);
    circuit.full = (1 << circuit.k) - 1;
    const int states = 1 << circuit.k;
    const int n = static_cast<int>(metric.size());
    const Rows labels(group_distance.begin() + 1, group_distance.end());

    circuit.ordinary = StandardRows(labels, metric, circuit.h);
    circuit.restricted = RestrictedBranchRows(labels, metric, circuit.h);
    circuit.branch.assign(states, Row(n, kInf));
    for (int mask = 1; mask < states; ++mask)
        if (Bits(mask) <= circuit.h)
            circuit.branch[mask] = BranchRow(circuit.restricted, mask);

    circuit.anchored.assign(states, Row(n, kInf));
    circuit.anchored[0] = group_distance[0];
    for (int size = 1; size <= circuit.a; ++size)
        for (int mask = 1; mask < states; ++mask)
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
                            circuit.branch[block][vertex]);
            }
            circuit.anchored[mask] = Close(seed, metric);
        }

    circuit.terminal.assign(states, Row(n, kInf));
    for (int target = 0; target < states; ++target)
    {
        if (Bits(target) > circuit.a)
            continue;
        const int remaining = circuit.full ^ target;
        for (int left = remaining;; left = (left - 1) & remaining)
        {
            const int right = remaining ^ left;
            if (left <= right &&
                Bits(left) <= circuit.h &&
                Bits(right) <= circuit.h)
                for (int vertex = 0; vertex < n; ++vertex)
                    circuit.terminal[target][vertex] = std::min(
                        circuit.terminal[target][vertex],
                        circuit.ordinary[left][vertex] +
                            circuit.ordinary[right][vertex]);
            if (!left)
                break;
        }
        circuit.forward_value = std::min(
            circuit.forward_value,
            Inner(circuit.anchored[target], circuit.terminal[target]));
    }

    circuit.suffix.assign(states, Row(n, kInf));
    circuit.backward.assign(states, Row(n, kInf));
    for (int size = circuit.a; size >= 0; --size)
        for (int target = 0; target < states; ++target)
        {
            if (Bits(target) != size)
                continue;
            circuit.suffix[target] = circuit.terminal[target];
            const int outside = circuit.full ^ target;
            for (int block = outside; block; block = (block - 1) & outside)
            {
                const int successor = target | block;
                if (Bits(successor) > circuit.a)
                    continue;
                for (int vertex = 0; vertex < n; ++vertex)
                    circuit.suffix[target][vertex] = std::min(
                        circuit.suffix[target][vertex],
                        circuit.branch[block][vertex] +
                            circuit.backward[successor][vertex]);
            }
            circuit.backward[target] = Close(circuit.suffix[target], metric);
        }
    return circuit;
}

double ExplicitSuffix(
    const Circuit& circuit,
    const Rows& metric,
    int start,
    int root)
{
    const int states = 1 << circuit.k;
    const int n = static_cast<int>(metric.size());
    Rows active(states, Row(n, kInf));
    active[start][root] = 0.0;
    double answer = kInf;
    for (int size = Bits(start); size <= circuit.a; ++size)
        for (int mask = 0; mask < states; ++mask)
        {
            if (Bits(mask) != size || (start & ~mask))
                continue;
            answer = std::min(answer, Inner(active[mask], circuit.terminal[mask]));
            const int outside = circuit.full ^ mask;
            for (int block = outside; block; block = (block - 1) & outside)
            {
                const int successor = mask | block;
                if (Bits(successor) > circuit.a)
                    continue;
                Row seed(n, kInf);
                for (int vertex = 0; vertex < n; ++vertex)
                    seed[vertex] = active[mask][vertex] +
                        circuit.branch[block][vertex];
                MinInto(active[successor], Close(seed, metric));
            }
        }
    return answer;
}

double CutValue(const Circuit& circuit, int cut)
{
    const int states = 1 << circuit.k;
    const int n = static_cast<int>(circuit.anchored[0].size());
    double answer = kInf;
    for (int mask = 0; mask < states; ++mask)
        if (Bits(mask) <= cut)
            answer = std::min(
                answer,
                Inner(circuit.anchored[mask], circuit.terminal[mask]));

    for (int target = 1; target < states; ++target)
    {
        if (Bits(target) <= cut || Bits(target) > circuit.a)
            continue;
        for (int predecessor = target;;
             predecessor = (predecessor - 1) & target)
        {
            if (Bits(predecessor) <= cut && predecessor != target)
            {
                const int block = target ^ predecessor;
                for (int vertex = 0; vertex < n; ++vertex)
                    answer = std::min(
                        answer,
                        circuit.anchored[predecessor][vertex] +
                            circuit.branch[block][vertex] +
                            circuit.backward[target][vertex]);
            }
            if (!predecessor)
                break;
        }
    }
    return answer;
}

struct DisjointSet
{
    explicit DisjointSet(int n) : parent(n), rank(n)
    {
        std::iota(parent.begin(), parent.end(), 0);
    }

    int Find(int value)
    {
        if (parent[value] != value)
            parent[value] = Find(parent[value]);
        return parent[value];
    }

    bool Join(int left, int right)
    {
        left = Find(left);
        right = Find(right);
        if (left == right)
            return false;
        if (rank[left] < rank[right])
            std::swap(left, right);
        parent[right] = left;
        if (rank[left] == rank[right])
            ++rank[left];
        return true;
    }

    std::vector<int> parent;
    std::vector<int> rank;
};

struct OracleTree
{
    double cost = kInf;
    int vertices = 0;
    std::vector<Edge> edges;
};

bool HitsAll(const Instance& instance, int vertices)
{
    for (const auto& group : instance.groups)
    {
        bool hit = false;
        for (int vertex : group)
            hit = hit || (vertices & (1 << vertex));
        if (!hit)
            return false;
    }
    return true;
}

OracleTree Mst(const Instance& instance, int vertices)
{
    OracleTree result;
    result.vertices = vertices;
    const int count = Bits(vertices);
    if (count == 1)
    {
        result.cost = 0.0;
        return result;
    }
    std::vector<Edge> edges = instance.edges;
    std::sort(edges.begin(), edges.end(), [](const Edge& left, const Edge& right)
    {
        return std::tie(left.w, left.u, left.v) <
            std::tie(right.w, right.u, right.v);
    });
    DisjointSet dsu(instance.n);
    result.cost = 0.0;
    for (const Edge& edge : edges)
    {
        if (!(vertices & (1 << edge.u)) || !(vertices & (1 << edge.v)))
            continue;
        if (!dsu.Join(edge.u, edge.v))
            continue;
        result.cost += edge.w;
        result.edges.push_back(edge);
    }
    if (static_cast<int>(result.edges.size()) != count - 1)
    {
        result.cost = kInf;
        result.edges.clear();
    }
    return result;
}

OracleTree BruteGst(const Instance& instance)
{
    OracleTree best;
    for (int vertices = 1; vertices < (1 << instance.n); ++vertices)
    {
        if (!HitsAll(instance, vertices))
            continue;
        OracleTree candidate = Mst(instance, vertices);
        if (candidate.cost + kEps < best.cost ||
            (Equal(candidate.cost, best.cost) &&
             Bits(candidate.vertices) < Bits(best.vertices)))
            best = std::move(candidate);
    }
    return best;
}

bool HasBalancedDecomposition(const Instance& instance, const OracleTree& tree)
{
    const int g = static_cast<int>(instance.groups.size());
    const int h = g / 2;
    std::vector<int> representative(g, -1);
    for (int group = 0; group < g; ++group)
        for (int vertex : instance.groups[group])
            if (tree.vertices & (1 << vertex))
            {
                representative[group] = vertex;
                break;
            }

    std::vector<std::vector<int>> adjacency(instance.n);
    for (const Edge& edge : tree.edges)
    {
        adjacency[edge.u].push_back(edge.v);
        adjacency[edge.v].push_back(edge.u);
    }

    for (int root = 0; root < instance.n; ++root)
    {
        if (!(tree.vertices & (1 << root)))
            continue;
        std::vector<int> component(instance.n, -1);
        int component_count = 0;
        for (int neighbor : adjacency[root])
        {
            std::vector<int> stack{neighbor};
            component[neighbor] = component_count;
            while (!stack.empty())
            {
                const int vertex = stack.back();
                stack.pop_back();
                for (int next : adjacency[vertex])
                    if (next != root && component[next] < 0)
                    {
                        component[next] = component_count;
                        stack.push_back(next);
                    }
            }
            ++component_count;
        }

        std::vector<int> labels(component_count);
        int root_labels = 0;
        int anchor_component = -1;
        for (int group = 0; group < g; ++group)
        {
            if (representative[group] == root)
                ++root_labels;
            else
            {
                const int id = component[representative[group]];
                ++labels[id];
                if (group == 0)
                    anchor_component = id;
            }
        }

        int assignments = 1;
        for (int index = 0; index < component_count; ++index)
            assignments *= 3;
        for (int code = 0; code < assignments; ++code)
        {
            int value = code;
            std::array<int, 3> load{};
            bool valid = true;
            for (int id = 0; id < component_count; ++id)
            {
                const int bin = value % 3;
                value /= 3;
                if (id == anchor_component && bin != 0)
                    valid = false;
                load[bin] += labels[id];
                if (load[bin] > h)
                    valid = false;
            }
            if (!valid)
                continue;
            int flexible = root_labels;
            if (representative[0] == root)
            {
                ++load[0];
                --flexible;
            }
            if (load[0] > h)
                continue;
            const int capacity =
                (h - load[0]) + (h - load[1]) + (h - load[2]);
            if (capacity >= flexible)
                return true;
        }
    }
    return false;
}

double MaskPotential(
    const std::vector<Row>& potential,
    int mask,
    int vertex)
{
    double value = 0.0;
    for (int bits = mask; bits; bits &= bits - 1)
        value += potential[FirstBit(bits & -bits) + 1][vertex];
    return value;
}

void CheckTransposed(
    const Circuit& circuit,
    std::mt19937_64& random,
    Counters& counters)
{
    const int states = 1 << circuit.k;
    const int n = static_cast<int>(circuit.ordinary[0].size());
    std::vector<Row> potential(circuit.g, Row(n));
    std::uniform_int_distribution<int> potential_value(0, 4);
    for (Row& row : potential)
        for (double& value : row)
            value = potential_value(random);

    const std::array<double, 5> budgets{
        0.0,
        3.0,
        9.0,
        20.0,
        1e6};
    for (int vertex = 0; vertex < n; ++vertex)
    {
        struct Entry
        {
            int mask = 0;
            double value = kInf;
            double reduced = kInf;
        };
        std::vector<Entry> entries;
        for (int mask = 1; mask < states; ++mask)
            if (Bits(mask) <= circuit.h &&
                circuit.ordinary[mask][vertex] < kInf)
                entries.push_back({
                    mask,
                    circuit.ordinary[mask][vertex],
                    circuit.ordinary[mask][vertex] -
                        MaskPotential(potential, mask, vertex)});

        Row direct(states, kInf);
        for (const Entry& entry : entries)
            if (Bits(circuit.full ^ entry.mask) <= circuit.a)
                direct[circuit.full ^ entry.mask] = std::min(
                    direct[circuit.full ^ entry.mask], entry.value);
        for (size_t left = 0; left < entries.size(); ++left)
            for (size_t right = left + 1; right < entries.size(); ++right)
            {
                if (entries[left].mask & entries[right].mask)
                    continue;
                const int target = circuit.full ^
                    (entries[left].mask | entries[right].mask);
                if (Bits(target) <= circuit.a)
                    direct[target] = std::min(
                        direct[target],
                        entries[left].value + entries[right].value);
            }
        for (int target = 0; target < states; ++target)
            if (Bits(target) <= circuit.a)
            {
                RequireEqual(
                    direct[target],
                    circuit.terminal[target][vertex],
                    "transpose_direct",
                    target,
                    vertex);
                ++counters.transpose_cells;
            }

        std::sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right)
        {
            return std::tie(left.reduced, left.mask) <
                std::tie(right.reduced, right.mask);
        });
        for (double best : budgets)
        {
            const double full_potential = potential[0][vertex] +
                MaskPotential(potential, circuit.full, vertex);
            const double reduced_budget = best - full_potential;
            Row sorted(states, kInf);
            Row submask(states, kInf);
            for (const Entry& entry : entries)
                if (entry.reduced <= reduced_budget)
                {
                    const int target = circuit.full ^ entry.mask;
                    if (Bits(target) <= circuit.a)
                    {
                        sorted[target] = std::min(sorted[target], entry.value);
                        submask[target] = std::min(submask[target], entry.value);
                    }
                }
            for (size_t left = 0; left < entries.size(); ++left)
                for (size_t right = left + 1; right < entries.size(); ++right)
                {
                    if (entries[left].reduced + entries[right].reduced >
                            reduced_budget ||
                        (entries[left].mask & entries[right].mask))
                        continue;
                    const int target = circuit.full ^
                        (entries[left].mask | entries[right].mask);
                    if (Bits(target) <= circuit.a)
                        sorted[target] = std::min(
                            sorted[target],
                            entries[left].value + entries[right].value);
                }

            std::vector<double> value(states, kInf);
            std::vector<double> reduced(states, kInf);
            for (const Entry& entry : entries)
            {
                value[entry.mask] = entry.value;
                reduced[entry.mask] = entry.reduced;
            }
            for (const Entry& left : entries)
            {
                const int complement = circuit.full ^ left.mask;
                for (int right = complement; right; right = (right - 1) & complement)
                {
                    if (right <= left.mask || value[right] >= kInf ||
                        left.reduced + reduced[right] > reduced_budget)
                        continue;
                    const int target = circuit.full ^ (left.mask | right);
                    if (Bits(target) <= circuit.a)
                        submask[target] = std::min(
                            submask[target], left.value + value[right]);
                }
            }

            for (int target = 0; target < states; ++target)
            {
                if (Bits(target) > circuit.a)
                    continue;
                const double prefix = potential[0][vertex] +
                    MaskPotential(potential, target, vertex);
                const double expected =
                    circuit.terminal[target][vertex] + prefix <= best + kEps
                        ? circuit.terminal[target][vertex]
                        : kInf;
                RequireEqual(expected, sorted[target], "transpose_sorted", target, vertex);
                RequireEqual(expected, submask[target], "transpose_submask", target, vertex);
                counters.transpose_cells += 2;
            }
        }
    }
}

void PrintInstance(const Instance& instance)
{
    std::cerr << "INSTANCE n=" << instance.n
              << " g=" << instance.groups.size()
              << " m=" << instance.edges.size() << '\n';
    for (const Edge& edge : instance.edges)
        std::cerr << "EDGE " << edge.u << ' ' << edge.v << ' ' << edge.w << '\n';
    for (int group = 0; group < static_cast<int>(instance.groups.size()); ++group)
    {
        std::cerr << "GROUP " << group;
        for (int vertex : instance.groups[group])
            std::cerr << ' ' << vertex;
        std::cerr << '\n';
    }
}

void CheckInstance(
    const Instance& instance,
    std::mt19937_64& random,
    bool exhaustive_suffix,
    Counters& counters)
{
    try
    {
        const Rows metric = Metric(instance);
        const Rows group_distance = GroupDistances(instance, metric);
        const Circuit circuit = BuildCircuit(group_distance, metric);
        const int states = 1 << circuit.k;

        const Rows full_rows = StandardRows(
            group_distance, metric, static_cast<int>(group_distance.size()));
        const double full_dp = RootMinimum(full_rows.back());
        const double iwata_m0 = IwataHalfValue(group_distance, metric);
        RequireEqual(iwata_m0, full_dp, "iwata_m0_full_dp");
        ++counters.iwata_m0_checks;

        for (int mask = 0; mask < states; ++mask)
            if (Bits(mask) <= circuit.h)
                for (int vertex = 0; vertex < instance.n; ++vertex)
                {
                    RequireEqual(
                        circuit.ordinary[mask][vertex],
                        circuit.restricted.rows[mask][vertex],
                        "branch_basis",
                        mask,
                        vertex);
                    ++counters.branch_cells;
                }

        const OracleTree oracle = BruteGst(instance);
        RequireEqual(circuit.forward_value, oracle.cost, "brute_gst");
        ++counters.brute_oracles;
        if (!HasBalancedDecomposition(instance, oracle))
            throw std::runtime_error("No balanced decomposition on oracle tree");
        ++counters.decomposition_checks;

        std::vector<int> masks;
        for (int mask = 0; mask < states; ++mask)
            if (Bits(mask) <= circuit.a)
                masks.push_back(mask);
        std::shuffle(masks.begin(), masks.end(), random);
        if (!exhaustive_suffix && masks.size() > 4)
            masks.resize(4);
        for (int mask : masks)
        {
            Row explicit_row(instance.n, kInf);
            for (int root = 0; root < instance.n; ++root)
                explicit_row[root] = ExplicitSuffix(circuit, metric, mask, root);
            for (int root = 0; root < instance.n; ++root)
                RequireEqual(
                    explicit_row[root],
                    circuit.suffix[mask][root],
                    "raw_suffix_F",
                    mask,
                    root);
            const Row explicit_backward = Close(explicit_row, metric);
            for (int root = 0; root < instance.n; ++root)
                RequireEqual(
                    explicit_backward[root],
                    circuit.backward[mask][root],
                    "closed_backward_H",
                    mask,
                    root);
            ++counters.suffix_rows;
        }

        for (int cut = 0; cut <= circuit.a; ++cut)
        {
            RequireEqual(CutValue(circuit, cut), circuit.forward_value, "cut_value", cut);
            ++counters.cuts;
        }
        CheckTransposed(circuit, random, counters);
    }
    catch (...)
    {
        PrintInstance(instance);
        throw;
    }
}

Instance SharedRepresentativeCase()
{
    Instance instance;
    instance.n = 1;
    instance.groups = {{0}, {0}, {0}, {0}, {0}};
    return instance;
}

Instance ZeroWeightOverlapCase()
{
    Instance instance;
    instance.n = 5;
    instance.edges = {
        {0, 1, 0},
        {1, 2, 0},
        {2, 3, 2},
        {2, 4, 1},
        {0, 4, 4}};
    instance.groups = {{0, 1}, {1}, {2, 3}, {2}, {4}};
    return instance;
}

Instance BalancedStarCase()
{
    Instance instance;
    instance.n = 7;
    instance.edges = {
        {0, 1, 0},
        {0, 2, 1},
        {0, 3, 1},
        {0, 4, 2},
        {0, 5, 2},
        {0, 6, 3},
        {1, 2, 2},
        {4, 5, 0}};
    instance.groups = {
        {0, 1}, {1}, {2}, {3}, {4, 5}, {5}, {6}};
    return instance;
}

Instance RandomInstance(
    std::mt19937_64& random,
    int max_n,
    int max_g,
    int max_edges,
    int min_g)
{
    std::uniform_int_distribution<int> n_distribution(2, max_n);
    Instance instance;
    instance.n = n_distribution(random);
    const int possible_edges = instance.n * (instance.n - 1) / 2;
    const int edge_limit = std::max(
        instance.n - 1,
        std::min(max_edges, possible_edges));
    std::uniform_int_distribution<int> target_edges(instance.n - 1, edge_limit);
    const int target = target_edges(random);
    std::vector<std::vector<unsigned char>> used(
        instance.n, std::vector<unsigned char>(instance.n));
    std::uniform_int_distribution<int> weight_distribution(0, 9);
    auto AddEdge = [&](int u, int v)
    {
        if (u > v)
            std::swap(u, v);
        if (u == v || used[u][v])
            return false;
        used[u][v] = 1;
        int weight = weight_distribution(random);
        if ((random() & 3ULL) != 0 && weight == 0)
            weight = 1 + static_cast<int>(random() % 9);
        instance.edges.push_back({u, v, weight});
        return true;
    };
    for (int vertex = 1; vertex < instance.n; ++vertex)
    {
        std::uniform_int_distribution<int> parent_distribution(0, vertex - 1);
        AddEdge(vertex, parent_distribution(random));
    }
    std::uniform_int_distribution<int> vertex_distribution(0, instance.n - 1);
    while (static_cast<int>(instance.edges.size()) < target)
        AddEdge(vertex_distribution(random), vertex_distribution(random));

    const int actual_min_g = std::min(min_g, max_g);
    std::uniform_int_distribution<int> g_distribution(actual_min_g, max_g);
    const int g = g_distribution(random);
    instance.groups.resize(g);
    const int shared = vertex_distribution(random);
    for (int group = 0; group < g; ++group)
    {
        const int limit = std::min(3, instance.n);
        std::uniform_int_distribution<int> size_distribution(1, limit);
        const int size = size_distribution(random);
        if (group > 0 && (random() % 3 == 0))
            instance.groups[group].push_back(shared);
        while (static_cast<int>(instance.groups[group].size()) < size)
        {
            const int vertex = vertex_distribution(random);
            if (std::find(
                    instance.groups[group].begin(),
                    instance.groups[group].end(),
                    vertex) == instance.groups[group].end())
                instance.groups[group].push_back(vertex);
        }
        std::shuffle(
            instance.groups[group].begin(),
            instance.groups[group].end(),
            random);
    }
    return instance;
}
}

int main(int argc, char** argv)
{
    try
    {
        const std::uint64_t seed = argc > 1
            ? std::strtoull(argv[1], nullptr, 10)
            : 157001ULL;
        const int iterations = argc > 2 ? std::atoi(argv[2]) : 300;
        const int max_n = argc > 3 ? std::atoi(argv[3]) : 9;
        const int max_g = argc > 4 ? std::atoi(argv[4]) : 8;
        const int max_edges = argc > 5 ? std::atoi(argv[5]) : 14;
        const int min_g = argc > 6 ? std::atoi(argv[6]) : 2;
        if (iterations < 0 || max_n < 2 || max_n > 12 ||
            min_g < 2 || max_g < min_g || max_g > 10)
            throw std::runtime_error("Invalid Test157 verifier arguments");

        std::mt19937_64 random(seed);
        Counters counters;
        std::vector<Instance> handcrafted{
            SharedRepresentativeCase(),
            ZeroWeightOverlapCase(),
            BalancedStarCase()};
        for (const Instance& instance : handcrafted)
            CheckInstance(instance, random, true, counters);
        for (int iteration = 0; iteration < iterations; ++iteration)
            CheckInstance(
                RandomInstance(random, max_n, max_g, max_edges, min_g),
                random,
                iteration < 10,
                counters);

        std::cout << "TEST157_OK"
                  << " seed=" << seed
                  << " handcrafted=" << handcrafted.size()
                  << " random=" << iterations
                  << " iwata_m0_checks=" << counters.iwata_m0_checks
                  << " branch_cells=" << counters.branch_cells
                  << " suffix_rows=" << counters.suffix_rows
                  << " cuts=" << counters.cuts
                  << " transpose_cells=" << counters.transpose_cells
                  << " brute_oracles=" << counters.brute_oracles
                  << " decompositions=" << counters.decomposition_checks
                  << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "TEST157_FAIL " << error.what() << '\n';
        return 1;
    }
}
