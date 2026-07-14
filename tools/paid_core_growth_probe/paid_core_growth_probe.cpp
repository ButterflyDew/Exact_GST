#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "float_compare.h"
#include "graph_io.h"
#include "query_io.h"

namespace
{
using Clock = std::chrono::steady_clock;
using Item = std::pair<double, int>;
using Heap = std::priority_queue<Item, std::vector<Item>, std::greater<Item>>;

int FirstBit(int mask)
{
    int bit = 0;
    while ((mask & (1 << bit)) == 0)
        ++bit;
    return bit;
}

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

class SimplexSolver
{
public:
    SimplexSolver(const std::vector<std::vector<double>>& constraints,
                  const std::vector<double>& bounds,
                  const std::vector<double>& objective)
        : m_(static_cast<int>(bounds.size())),
          n_(static_cast<int>(objective.size())),
          basic_(m_), nonbasic_(n_ + 1),
          tableau_(m_ + 2, std::vector<double>(n_ + 2))
    {
        for (int row = 0; row < m_; ++row)
            for (int column = 0; column < n_; ++column)
                tableau_[row][column] = constraints[row][column];
        for (int row = 0; row < m_; ++row)
        {
            basic_[row] = n_ + row;
            tableau_[row][n_] = -1.0;
            tableau_[row][n_ + 1] = bounds[row];
        }
        for (int column = 0; column < n_; ++column)
        {
            nonbasic_[column] = column;
            tableau_[m_][column] = -objective[column];
        }
        nonbasic_[n_] = -1;
        tableau_[m_ + 1][n_] = 1.0;
    }

    double Solve()
    {
        int row = 0;
        for (int candidate = 1; candidate < m_; ++candidate)
            if (tableau_[candidate][n_ + 1] < tableau_[row][n_ + 1])
                row = candidate;
        if (tableau_[row][n_ + 1] < -kTolerance)
        {
            Pivot(row, n_);
            if (!RunSimplex(1) || tableau_[m_ + 1][n_ + 1] < -kTolerance)
                throw std::runtime_error("macro packing LP is infeasible");
            if (std::abs(tableau_[m_ + 1][n_ + 1]) > kTolerance)
                throw std::runtime_error("macro packing LP phase one failed");
            for (int basic_row = 0; basic_row < m_; ++basic_row)
                if (basic_[basic_row] == -1)
                {
                    int column = 0;
                    for (int candidate = 1; candidate <= n_; ++candidate)
                        if (tableau_[basic_row][candidate] <
                                tableau_[basic_row][column] - kTolerance ||
                            (std::abs(tableau_[basic_row][candidate] -
                                      tableau_[basic_row][column]) <= kTolerance &&
                             nonbasic_[candidate] < nonbasic_[column]))
                            column = candidate;
                    Pivot(basic_row, column);
                }
        }
        if (!RunSimplex(2))
            throw std::runtime_error("macro packing LP is unbounded");
        return tableau_[m_][n_ + 1];
    }

private:
    static constexpr double kTolerance = 1e-10;

    void Pivot(int row, int column)
    {
        const double inverse = 1.0 / tableau_[row][column];
        for (int other_row = 0; other_row < m_ + 2; ++other_row)
            if (other_row != row)
                for (int other_column = 0; other_column < n_ + 2;
                     ++other_column)
                    if (other_column != column)
                        tableau_[other_row][other_column] -=
                            tableau_[row][other_column] *
                            tableau_[other_row][column] * inverse;
        for (int other_column = 0; other_column < n_ + 2; ++other_column)
            if (other_column != column)
                tableau_[row][other_column] *= inverse;
        for (int other_row = 0; other_row < m_ + 2; ++other_row)
            if (other_row != row)
                tableau_[other_row][column] *= -inverse;
        tableau_[row][column] = inverse;
        std::swap(basic_[row], nonbasic_[column]);
    }

    bool RunSimplex(int phase)
    {
        const int objective_row = phase == 1 ? m_ + 1 : m_;
        while (true)
        {
            int column = -1;
            for (int candidate = 0; candidate <= n_; ++candidate)
            {
                if (phase == 2 && nonbasic_[candidate] == -1)
                    continue;
                if (column < 0 ||
                    tableau_[objective_row][candidate] <
                        tableau_[objective_row][column] - kTolerance ||
                    (std::abs(tableau_[objective_row][candidate] -
                              tableau_[objective_row][column]) <= kTolerance &&
                     nonbasic_[candidate] < nonbasic_[column]))
                    column = candidate;
            }
            if (tableau_[objective_row][column] >= -kTolerance)
                return true;
            int row = -1;
            for (int candidate = 0; candidate < m_; ++candidate)
            {
                if (tableau_[candidate][column] <= kTolerance)
                    continue;
                if (row < 0)
                {
                    row = candidate;
                    continue;
                }
                const double ratio = tableau_[candidate][n_ + 1] /
                    tableau_[candidate][column];
                const double best_ratio = tableau_[row][n_ + 1] /
                    tableau_[row][column];
                if (ratio < best_ratio - kTolerance ||
                    (std::abs(ratio - best_ratio) <= kTolerance &&
                     basic_[candidate] < basic_[row]))
                    row = candidate;
            }
            if (row < 0)
                return false;
            Pivot(row, column);
        }
    }

    int m_ = 0;
    int n_ = 0;
    std::vector<int> basic_;
    std::vector<int> nonbasic_;
    std::vector<std::vector<double>> tableau_;
};

struct GroupData
{
    std::vector<std::vector<double>> distance;
    std::vector<std::vector<int>> parent;
    std::vector<std::vector<int>> parent_edge;
};

GroupData BuildGroups(const gst::Graph& graph, const gst::Query& query)
{
    const int g = static_cast<int>(query.groups.size());
    GroupData result;
    result.distance.assign(g, std::vector<double>(graph.n + 1, gst::fp::kInf));
    result.parent.assign(g, std::vector<int>(graph.n + 1, -1));
    result.parent_edge.assign(g, std::vector<int>(graph.n + 1, -1));
    for (int group = 0; group < g; ++group)
    {
        Heap heap;
        for (int vertex : query.groups[group])
        {
            result.distance[group][vertex] = 0.0;
            result.parent[group][vertex] = 0;
            heap.push({0.0, vertex});
        }
        while (!heap.empty())
        {
            const auto [distance, vertex] = heap.top();
            heap.pop();
            if (distance != result.distance[group][vertex])
                continue;
            for (const auto& edge : graph.adj[vertex])
            {
                const double next = distance + edge.w;
                if (next < result.distance[group][edge.to])
                {
                    result.distance[group][edge.to] = next;
                    result.parent[group][edge.to] = vertex;
                    result.parent_edge[group][edge.to] = edge.edge_id;
                    heap.push({next, edge.to});
                }
            }
        }
    }
    return result;
}

struct LowRow
{
    std::vector<double> distance;
    std::vector<int> parent;
    std::vector<int> parent_edge;
    std::vector<int> split;
};

struct LowRows
{
    int q = 0;
    std::vector<LowRow> row;
    long long seed_probes = 0;
    long long queue_pops = 0;
    double milliseconds = 0.0;
};

double Value(const LowRows& rows,
             const GroupData& groups,
             int mask,
             int vertex)
{
    if (!(mask & (mask - 1)))
        return groups.distance[FirstBit(mask)][vertex];
    return rows.row[mask].distance[vertex];
}

LowRows BuildLowRows(const gst::Graph& graph,
                     const GroupData& groups,
                     int g,
                     int q)
{
    const auto begin = Clock::now();
    const int subset_count = 1 << g;
    LowRows result;
    result.q = q;
    result.row.resize(subset_count);
    for (int size = 2; size <= q; ++size)
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (Bits(mask) != size)
                continue;
            LowRow& row = result.row[mask];
            row.distance.assign(graph.n + 1, gst::fp::kInf);
            row.parent.assign(graph.n + 1, -1);
            row.parent_edge.assign(graph.n + 1, -1);
            row.split.assign(graph.n + 1, 0);
            const int pivot = FirstBit(mask);
            const int branch_domain = mask ^ (1 << pivot);
            for (int right = branch_domain; right; right = (right - 1) & branch_domain)
            {
                const int left = mask ^ right;
                for (int vertex = 1; vertex <= graph.n; ++vertex)
                {
                    const double candidate =
                        Value(result, groups, left, vertex) +
                        Value(result, groups, right, vertex);
                    ++result.seed_probes;
                    if (candidate < row.distance[vertex])
                    {
                        row.distance[vertex] = candidate;
                        row.parent[vertex] = 0;
                        row.split[vertex] = left;
                    }
                }
            }
            Heap heap;
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                heap.push({row.distance[vertex], vertex});
            while (!heap.empty())
            {
                const auto [distance, vertex] = heap.top();
                heap.pop();
                ++result.queue_pops;
                if (distance != row.distance[vertex])
                    continue;
                for (const auto& edge : graph.adj[vertex])
                {
                    const double next = distance + edge.w;
                    if (next < row.distance[edge.to])
                    {
                        row.distance[edge.to] = next;
                        row.parent[edge.to] = vertex;
                        row.parent_edge[edge.to] = edge.edge_id;
                        row.split[edge.to] = 0;
                        heap.push({next, edge.to});
                    }
                }
            }
        }
    result.milliseconds =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return result;
}

std::vector<double> BuildTopRow(const gst::Graph& graph,
                                const GroupData& groups,
                                const LowRows& rows,
                                int mask)
{
    if (Bits(mask) == 1)
        return groups.distance[FirstBit(mask)];
    if (Bits(mask) <= rows.q)
        return rows.row[mask].distance;
    std::vector<double> distance(graph.n + 1, gst::fp::kInf);
    const int pivot = FirstBit(mask);
    const int branch_domain = mask ^ (1 << pivot);
    for (int right = branch_domain; right; right = (right - 1) & branch_domain)
    {
        const int left = mask ^ right;
        if (Bits(left) > rows.q || Bits(right) > rows.q)
            continue;
        for (int vertex = 1; vertex <= graph.n; ++vertex)
            distance[vertex] = std::min(
                distance[vertex],
                Value(rows, groups, left, vertex) +
                    Value(rows, groups, right, vertex));
    }
    Heap heap;
    for (int vertex = 1; vertex <= graph.n; ++vertex)
        heap.push({distance[vertex], vertex});
    while (!heap.empty())
    {
        const auto [value, vertex] = heap.top();
        heap.pop();
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
    return distance;
}

void CloseDenseRow(const gst::Graph& graph,
                   std::vector<double>& distance,
                   long long& queue_pops)
{
    Heap heap;
    for (int vertex = 1; vertex <= graph.n; ++vertex)
        heap.push({distance[vertex], vertex});
    while (!heap.empty())
    {
        const auto [value, vertex] = heap.top();
        heap.pop();
        ++queue_pops;
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

struct Witness
{
    std::vector<int> edges;
    std::vector<int> vertices;
};

Witness BuildWitness(const gst::Graph& graph,
                     const GroupData& groups,
                     const LowRows& rows,
                     int mask,
                     int root,
                     std::vector<int>& edge_stamp,
                     std::vector<int>& vertex_stamp,
                     int& stamp,
                     std::vector<int>* selected_terminal = nullptr)
{
    ++stamp;
    if (selected_terminal)
        selected_terminal->assign(groups.distance.size(), -1);
    Witness result;
    auto AddVertex = [&](int vertex)
    {
        if (vertex_stamp[vertex] != stamp)
        {
            vertex_stamp[vertex] = stamp;
            result.vertices.push_back(vertex);
        }
    };
    auto AddEdge = [&](int edge_id)
    {
        if (edge_id >= 0 && edge_stamp[edge_id] != stamp)
        {
            edge_stamp[edge_id] = stamp;
            result.edges.push_back(edge_id);
            AddVertex(graph.edges[edge_id].u);
            AddVertex(graph.edges[edge_id].v);
        }
    };
    auto Reconstruct = [&](auto&& self, int current_mask, int vertex) -> void
    {
        AddVertex(vertex);
        if (!(current_mask & (current_mask - 1)))
        {
            const int group = FirstBit(current_mask);
            while (groups.parent[group][vertex] > 0)
            {
                AddEdge(groups.parent_edge[group][vertex]);
                vertex = groups.parent[group][vertex];
            }
            AddVertex(vertex);
            if (selected_terminal)
                (*selected_terminal)[group] = vertex;
            return;
        }
        const LowRow& row = rows.row[current_mask];
        if (row.parent[vertex] > 0)
        {
            AddEdge(row.parent_edge[vertex]);
            self(self, current_mask, row.parent[vertex]);
            return;
        }
        const int left = row.split[vertex];
        self(self, left, vertex);
        self(self, current_mask ^ left, vertex);
    };
    Reconstruct(Reconstruct, mask, root);
    std::sort(result.edges.begin(), result.edges.end());
    std::sort(result.vertices.begin(), result.vertices.end());
    return result;
}

struct ContractedBlockResult
{
    double value = gst::fp::kInf;
    std::vector<double> root;
    long long join_probes = 0;
    long long queue_pops = 0;
};

ContractedBlockResult PriceContractedBlock(
    const gst::Graph& graph,
    const GroupData& groups,
    int block,
    const Witness& paid_core,
    std::vector<int>& paid_edge_stamp,
    int& paid_generation)
{
    if (++paid_generation == 0)
        throw std::runtime_error("contracted edge stamp overflow");
    for (int edge : paid_core.edges)
        paid_edge_stamp[edge] = paid_generation;

    std::vector<int> labels;
    for (int bits = block; bits; bits &= bits - 1)
        labels.push_back(FirstBit(bits));
    const int subset_count = 1 << static_cast<int>(labels.size());
    std::vector<std::vector<double>> distance(
        subset_count,
        std::vector<double>(graph.n + 1, gst::fp::kInf));
    ContractedBlockResult result;

    for (int mask = 1; mask < subset_count; ++mask)
    {
        std::vector<double>& row = distance[mask];
        if (!(mask & (mask - 1)))
        {
            const int group = labels[FirstBit(mask)];
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                if (groups.distance[group][vertex] == 0.0)
                    row[vertex] = 0.0;
        }
        else
        {
            const int pivot = FirstBit(mask);
            const int branch_domain = mask ^ (1 << pivot);
            for (int right = branch_domain; right;
                 right = (right - 1) & branch_domain)
            {
                const int left = mask ^ right;
                for (int vertex = 1; vertex <= graph.n; ++vertex)
                {
                    row[vertex] = std::min(
                        row[vertex],
                        distance[left][vertex] + distance[right][vertex]);
                    ++result.join_probes;
                }
            }
        }

        Heap heap;
        for (int vertex = 1; vertex <= graph.n; ++vertex)
            if (row[vertex] < gst::fp::kInf)
                heap.push({row[vertex], vertex});
        while (!heap.empty())
        {
            const auto [value, vertex] = heap.top();
            heap.pop();
            if (value != row[vertex])
                continue;
            ++result.queue_pops;
            for (const auto& edge : graph.adj[vertex])
            {
                const double weight =
                    paid_edge_stamp[edge.edge_id] == paid_generation
                    ? 0.0
                    : edge.w;
                const double next = value + weight;
                if (next < row[edge.to])
                {
                    row[edge.to] = next;
                    heap.push({next, edge.to});
                }
            }
        }
    }

    const std::vector<double>& full = distance.back();
    for (int vertex : paid_core.vertices)
        result.value = std::min(result.value, full[vertex]);
    result.root = std::move(distance.back());
    return result;
}

ContractedBlockResult PriceContractedPair(
    const gst::Graph& graph,
    const ContractedBlockResult& left,
    const ContractedBlockResult& right,
    const Witness& paid_core,
    const std::vector<int>& paid_edge_stamp,
    int paid_generation)
{
    ContractedBlockResult result;
    result.root.assign(graph.n + 1, gst::fp::kInf);
    Heap heap;
    for (int vertex = 1; vertex <= graph.n; ++vertex)
    {
        result.root[vertex] = left.root[vertex] + right.root[vertex];
        if (result.root[vertex] < gst::fp::kInf)
            heap.push({result.root[vertex], vertex});
        ++result.join_probes;
    }
    while (!heap.empty())
    {
        const auto [value, vertex] = heap.top();
        heap.pop();
        if (value != result.root[vertex])
            continue;
        ++result.queue_pops;
        for (const auto& edge : graph.adj[vertex])
        {
            const double weight =
                paid_edge_stamp[edge.edge_id] == paid_generation
                ? 0.0
                : edge.w;
            const double next = value + weight;
            if (next < result.root[edge.to])
            {
                result.root[edge.to] = next;
                heap.push({next, edge.to});
            }
        }
    }
    for (int vertex : paid_core.vertices)
        result.value = std::min(result.value, result.root[vertex]);
    return result;
}

struct PairProfileColumn
{
    std::vector<double> value;
    std::vector<int> argmin;
};

PairProfileColumn BuildPairProfileColumn(const LowRows& rows,
                                         const GroupData& groups,
                                         int pair,
                                         int block,
                                         int n)
{
    const int first = FirstBit(pair);
    const int second = FirstBit(pair ^ (1 << first));
    auto BuildGroupProfile = [&](int group)
    {
        PairProfileColumn result;
        result.value.assign(n + 1, gst::fp::kInf);
        result.argmin.assign(n + 1, 0);
        std::vector<char> ready(n + 1);
        std::vector<int> path;
        for (int start = 1; start <= n; ++start)
        {
            int vertex = start;
            path.clear();
            while (vertex > 0 && !ready[vertex])
            {
                path.push_back(vertex);
                vertex = groups.parent[group][vertex];
            }
            double inherited = vertex > 0
                ? result.value[vertex]
                : gst::fp::kInf;
            int inherited_arg = vertex > 0 ? result.argmin[vertex] : 0;
            for (auto it = path.rbegin(); it != path.rend(); ++it)
            {
                const int current = *it;
                const double local = Value(rows, groups, block, current);
                if (groups.parent[group][current] <= 0 || local < inherited)
                {
                    inherited = local;
                    inherited_arg = current;
                }
                result.value[current] = inherited;
                result.argmin[current] = inherited_arg;
                ready[current] = 1;
            }
        }
        return result;
    };

    const PairProfileColumn first_profile = BuildGroupProfile(first);
    const PairProfileColumn second_profile = BuildGroupProfile(second);
    PairProfileColumn result;
    result.value.assign(n + 1, gst::fp::kInf);
    result.argmin.assign(n + 1, 0);
    std::vector<char> ready(n + 1);
    std::vector<int> path;
    const LowRow& pair_row = rows.row[pair];
    for (int start = 1; start <= n; ++start)
    {
        int vertex = start;
        path.clear();
        while (vertex > 0 && !ready[vertex])
        {
            path.push_back(vertex);
            vertex = pair_row.parent[vertex];
        }
        double inherited = vertex > 0
            ? result.value[vertex]
            : gst::fp::kInf;
        int inherited_arg = vertex > 0 ? result.argmin[vertex] : 0;
        for (auto it = path.rbegin(); it != path.rend(); ++it)
        {
            const int current = *it;
            if (pair_row.parent[current] <= 0)
            {
                if (first_profile.value[current] <=
                    second_profile.value[current])
                {
                    inherited = first_profile.value[current];
                    inherited_arg = first_profile.argmin[current];
                }
                else
                {
                    inherited = second_profile.value[current];
                    inherited_arg = second_profile.argmin[current];
                }
            }
            else
            {
                const double local = Value(rows, groups, block, current);
                if (local < inherited)
                {
                    inherited = local;
                    inherited_arg = current;
                }
            }
            result.value[current] = inherited;
            result.argmin[current] = inherited_arg;
            ready[current] = 1;
        }
    }
    return result;
}

class PairProfileTargetOracle
{
public:
    explicit PairProfileTargetOracle(int n)
        : first_value_(n + 1), second_value_(n + 1), pair_value_(n + 1),
          first_arg_(n + 1), second_arg_(n + 1), pair_arg_(n + 1),
          first_head_(n + 1), second_head_(n + 1),
          pair_first_head_(n + 1), pair_second_head_(n + 1),
          first_stamp_(n + 1), second_stamp_(n + 1), pair_stamp_(n + 1),
          enumeration_stamp_(n + 1)
    {
    }

    void Evaluate(const LowRows& rows,
                  const GroupData& groups,
                  int pair,
                  int block,
                  const std::vector<int>& targets,
                  std::vector<double>& values,
                  std::vector<int>& argmins,
                  std::vector<int>* argmin_offsets = nullptr,
                  std::vector<int>* all_argmins = nullptr)
    {
        if ((argmin_offsets == nullptr) != (all_argmins == nullptr))
            throw std::runtime_error(
                "argmin offsets and values must be requested together");
        if (++stamp_ == 0)
            throw std::runtime_error("pair-profile target stamp overflow");
        list_vertex_.assign(1, 0);
        list_next_.assign(1, 0);
        auto Prepend = [&](int vertex, int next)
        {
            list_vertex_.push_back(vertex);
            list_next_.push_back(next);
            return static_cast<int>(list_vertex_.size()) - 1;
        };
        const int first = FirstBit(pair);
        const int second = FirstBit(pair ^ (1 << first));
        auto GroupValue = [&](int group,
                              int start,
                              std::vector<double>& memo_value,
                              std::vector<int>& memo_arg,
                              std::vector<int>& memo_head,
                              std::vector<int>& memo_stamp)
        {
            int vertex = start;
            group_path_.clear();
            while (vertex > 0 && memo_stamp[vertex] != stamp_)
            {
                group_path_.push_back(vertex);
                vertex = groups.parent[group][vertex];
            }
            double inherited = vertex > 0
                ? memo_value[vertex]
                : gst::fp::kInf;
            int inherited_arg = vertex > 0 ? memo_arg[vertex] : 0;
            int inherited_head = vertex > 0 ? memo_head[vertex] : 0;
            for (auto it = group_path_.rbegin();
                 it != group_path_.rend(); ++it)
            {
                const int current = *it;
                const double local = Value(rows, groups, block, current);
                if (groups.parent[group][current] <= 0)
                {
                    inherited = local;
                    inherited_arg = current;
                    inherited_head = Prepend(current, 0);
                }
                else if (gst::fp::Eq(local, inherited))
                {
                    if (local < inherited)
                    {
                        inherited = local;
                        inherited_arg = current;
                    }
                    inherited_head = Prepend(current, inherited_head);
                }
                else if (local < inherited)
                {
                    inherited = local;
                    inherited_arg = current;
                    inherited_head = Prepend(current, 0);
                }
                memo_value[current] = inherited;
                memo_arg[current] = inherited_arg;
                memo_head[current] = inherited_head;
                memo_stamp[current] = stamp_;
            }
            return std::tuple<double, int, int>{
                memo_value[start], memo_arg[start], memo_head[start]};
        };
        auto FirstValue = [&](int vertex)
        {
            return GroupValue(first, vertex, first_value_, first_arg_,
                              first_head_, first_stamp_);
        };
        auto SecondValue = [&](int vertex)
        {
            return GroupValue(
                second, vertex, second_value_, second_arg_, second_head_,
                second_stamp_);
        };
        const LowRow& pair_row = rows.row[pair];
        auto PairValueAt = [&](int start)
        {
            int vertex = start;
            path_.clear();
            while (vertex > 0 && pair_stamp_[vertex] != stamp_)
            {
                path_.push_back(vertex);
                vertex = pair_row.parent[vertex];
            }
            double inherited = vertex > 0
                ? pair_value_[vertex]
                : gst::fp::kInf;
            int inherited_arg = vertex > 0 ? pair_arg_[vertex] : 0;
            int inherited_first_head = vertex > 0
                ? pair_first_head_[vertex]
                : 0;
            int inherited_second_head = vertex > 0
                ? pair_second_head_[vertex]
                : 0;
            for (auto it = path_.rbegin(); it != path_.rend(); ++it)
            {
                const int current = *it;
                if (pair_row.parent[current] <= 0)
                {
                    const auto left = FirstValue(current);
                    const auto right = SecondValue(current);
                    if (std::get<0>(left) <= std::get<0>(right))
                    {
                        inherited = std::get<0>(left);
                        inherited_arg = std::get<1>(left);
                        inherited_first_head = std::get<2>(left);
                        inherited_second_head = 0;
                    }
                    else
                    {
                        inherited = std::get<0>(right);
                        inherited_arg = std::get<1>(right);
                        inherited_first_head = std::get<2>(right);
                        inherited_second_head = 0;
                    }
                    if (gst::fp::Eq(std::get<0>(left), std::get<0>(right)))
                    {
                        inherited_first_head = std::get<2>(left);
                        inherited_second_head = std::get<2>(right);
                    }
                }
                else
                {
                    const double local = Value(rows, groups, block, current);
                    if (gst::fp::Eq(local, inherited))
                    {
                        if (local < inherited)
                        {
                            inherited = local;
                            inherited_arg = current;
                        }
                        inherited_first_head = Prepend(
                            current, inherited_first_head);
                    }
                    else if (local < inherited)
                    {
                        inherited = local;
                        inherited_arg = current;
                        inherited_first_head = Prepend(current, 0);
                        inherited_second_head = 0;
                    }
                }
                pair_value_[current] = inherited;
                pair_arg_[current] = inherited_arg;
                pair_first_head_[current] = inherited_first_head;
                pair_second_head_[current] = inherited_second_head;
                pair_stamp_[current] = stamp_;
            }
            return std::tuple<double, int, int, int>{
                pair_value_[start], pair_arg_[start],
                pair_first_head_[start], pair_second_head_[start]};
        };

        values.resize(targets.size());
        argmins.resize(targets.size());
        std::vector<int> target_first_heads;
        std::vector<int> target_second_heads;
        if (all_argmins)
        {
            target_first_heads.resize(targets.size());
            target_second_heads.resize(targets.size());
        }
        for (size_t index = 0; index < targets.size(); ++index)
        {
            const auto result = PairValueAt(targets[index]);
            values[index] = std::get<0>(result);
            argmins[index] = std::get<1>(result);
            if (all_argmins)
            {
                target_first_heads[index] = std::get<2>(result);
                target_second_heads[index] = std::get<3>(result);
            }
        }
        if (all_argmins)
        {
            argmin_offsets->resize(targets.size() + 1);
            all_argmins->clear();
            for (size_t index = 0; index < targets.size(); ++index)
            {
                (*argmin_offsets)[index] = all_argmins->size();
                if (++enumeration_generation_ == 0)
                    throw std::runtime_error("argmin enumeration stamp overflow");
                for (int head : {target_first_heads[index],
                                 target_second_heads[index]})
                    for (int node = head; node; node = list_next_[node])
                    {
                        const int vertex = list_vertex_[node];
                        if (enumeration_stamp_[vertex] ==
                            enumeration_generation_)
                            continue;
                        enumeration_stamp_[vertex] = enumeration_generation_;
                        all_argmins->push_back(vertex);
                    }
            }
            (*argmin_offsets)[targets.size()] = all_argmins->size();
        }
    }

private:
    int stamp_ = 0;
    std::vector<double> first_value_;
    std::vector<double> second_value_;
    std::vector<double> pair_value_;
    std::vector<int> first_arg_;
    std::vector<int> second_arg_;
    std::vector<int> pair_arg_;
    std::vector<int> first_head_;
    std::vector<int> second_head_;
    std::vector<int> pair_first_head_;
    std::vector<int> pair_second_head_;
    std::vector<int> first_stamp_;
    std::vector<int> second_stamp_;
    std::vector<int> pair_stamp_;
    std::vector<int> path_;
    std::vector<int> group_path_;
    std::vector<int> list_vertex_;
    std::vector<int> list_next_;
    std::vector<int> enumeration_stamp_;
    int enumeration_generation_ = 0;
};

struct ProfileFamily
{
    double first = gst::fp::kInf;
    double second = gst::fp::kInf;
    double sum = gst::fp::kInf;
};

void Accumulate(ProfileFamily& target, const ProfileFamily& candidate)
{
    target.first = std::min(target.first, candidate.first);
    target.second = std::min(target.second, candidate.second);
    target.sum = std::min(target.sum, candidate.sum);
}

ProfileFamily CombineFamilies(const ProfileFamily& left,
                              const ProfileFamily& right)
{
    return {
        std::min(left.first, right.first),
        std::min(left.second, right.second),
        std::min({left.sum, right.sum,
                  left.first + right.second,
                  right.first + left.second})};
}

ProfileFamily ExtendFamily(const ProfileFamily& family,
                           double first,
                           double second)
{
    return {
        std::min(family.first, first),
        std::min(family.second, second),
        std::min({family.sum, family.first + second,
                  first + family.second, first + second})};
}

struct OptimalWitnessProfiles
{
    std::vector<ProfileFamily> root;
    long long join_transitions = 0;
    long long edge_transitions = 0;
};

OptimalWitnessProfiles BuildOptimalWitnessProfiles(
    const gst::Graph& graph,
    const GroupData& groups,
    const LowRows& rows,
    int core,
    int first_block,
    int second_block)
{
    std::vector<std::vector<ProfileFamily>> family;
    std::vector<int> original_masks;
    for (int original = core;; original = (original - 1) & core)
    {
        if (original)
            original_masks.push_back(original);
        if (!original)
            break;
    }
    std::sort(original_masks.begin(), original_masks.end(), [](int a, int b)
    {
        if (Bits(a) != Bits(b))
            return Bits(a) < Bits(b);
        return a < b;
    });
    int compact_full = 0;
    std::vector<int> core_bits;
    for (int bits = core; bits; bits &= bits - 1)
        core_bits.push_back(FirstBit(bits & -bits));
    compact_full = (1 << static_cast<int>(core_bits.size())) - 1;
    family.assign(compact_full + 1, {});
    auto Compact = [&](int original)
    {
        int compact = 0;
        for (int index = 0; index < static_cast<int>(core_bits.size()); ++index)
            if (original & (1 << core_bits[index]))
                compact |= 1 << index;
        return compact;
    };

    OptimalWitnessProfiles result;
    std::vector<int> order(graph.n);
    std::iota(order.begin(), order.end(), 1);
    for (int mask : original_masks)
    {
        const int compact_mask = Compact(mask);
        auto& current = family[compact_mask];
        current.assign(graph.n + 1, {});
        std::sort(order.begin(), order.end(), [&](int a, int b)
        {
            const double left = Value(rows, groups, mask, a);
            const double right = Value(rows, groups, mask, b);
            if (left != right)
                return left < right;
            return a < b;
        });
        for (int vertex : order)
        {
            ProfileFamily best;
            if (!(mask & (mask - 1)) &&
                Value(rows, groups, mask, vertex) <= gst::fp::kEps)
            {
                const double first = Value(
                    rows, groups, first_block, vertex);
                const double second = Value(
                    rows, groups, second_block, vertex);
                best = {first, second, first + second};
            }
            if (mask & (mask - 1))
            {
                const int pivot = mask & -mask;
                for (int left = (mask - 1) & mask; left;
                     left = (left - 1) & mask)
                {
                    if (!(left & pivot))
                        continue;
                    const int right = mask ^ left;
                    if (!right ||
                        !gst::fp::Eq(
                            Value(rows, groups, left, vertex) +
                                Value(rows, groups, right, vertex),
                            Value(rows, groups, mask, vertex)))
                        continue;
                    Accumulate(best, CombineFamilies(
                        family[Compact(left)][vertex],
                        family[Compact(right)][vertex]));
                    ++result.join_transitions;
                }
            }
            for (const gst::AdjEdge& edge : graph.adj[vertex])
            {
                if (!(Value(rows, groups, mask, edge.to) <
                      Value(rows, groups, mask, vertex)))
                    continue;
                if (!gst::fp::Eq(
                        Value(rows, groups, mask, edge.to) + edge.w,
                        Value(rows, groups, mask, vertex)))
                    continue;
                Accumulate(best, ExtendFamily(
                    current[edge.to],
                    Value(rows, groups, first_block, vertex),
                    Value(rows, groups, second_block, vertex)));
                ++result.edge_transitions;
            }
            current[vertex] = best;
        }
    }
    result.root = std::move(family[compact_full]);
    return result;
}

struct MacroPlanResult
{
    double value = gst::fp::kInf;
    long long join_probes = 0;
    long long queue_pops = 0;
};

MacroPlanResult PriceMacroPlan(const gst::Graph& graph,
                               const GroupData& groups,
                               const LowRows& rows,
                               int core,
                               int first_block,
                               int second_block)
{
    std::vector<int> labels;
    for (int bits = core; bits; bits &= bits - 1)
        labels.push_back(bits & -bits);
    labels.push_back(first_block);
    labels.push_back(second_block);
    if (labels.size() != 6)
        throw std::runtime_error("macro plan requires six labels");
    constexpr int kLabelCount = 6;
    constexpr int kMaskCount = 1 << kLabelCount;
    std::vector<std::vector<double>> dp(
        kMaskCount, std::vector<double>(graph.n + 1, gst::fp::kInf));
    for (int label = 0; label < kLabelCount; ++label)
        for (int vertex = 1; vertex <= graph.n; ++vertex)
            dp[1 << label][vertex] = Value(
                rows, groups, labels[label], vertex);

    MacroPlanResult result;
    for (int mask = 1; mask < kMaskCount; ++mask)
    {
        if (mask & (mask - 1))
        {
            const int pivot = mask & -mask;
            for (int left = (mask - 1) & mask; left;
                 left = (left - 1) & mask)
            {
                if (!(left & pivot))
                    continue;
                const int right = mask ^ left;
                if (!right)
                    continue;
                for (int vertex = 1; vertex <= graph.n; ++vertex)
                {
                    dp[mask][vertex] = std::min(
                        dp[mask][vertex],
                        dp[left][vertex] + dp[right][vertex]);
                    ++result.join_probes;
                }
            }
        }
        Heap heap;
        for (int vertex = 1; vertex <= graph.n; ++vertex)
            if (dp[mask][vertex] < gst::fp::kInf)
                heap.push({dp[mask][vertex], vertex});
        while (!heap.empty())
        {
            const auto [distance, vertex] = heap.top();
            heap.pop();
            if (distance != dp[mask][vertex])
                continue;
            ++result.queue_pops;
            for (const gst::AdjEdge& edge : graph.adj[vertex])
            {
                const double next = distance + edge.w;
                if (next < dp[mask][edge.to])
                {
                    dp[mask][edge.to] = next;
                    heap.push({next, edge.to});
                }
            }
        }
    }
    result.value = *std::min_element(
        dp[kMaskCount - 1].begin() + 1,
        dp[kMaskCount - 1].end());
    return result;
}

double MacroTourHalf(const GroupData& groups,
                     const LowRows& rows,
                     int vertex_count,
                     int core,
                     int first_block,
                     int second_block)
{
    std::vector<int> labels;
    for (int bits = core; bits; bits &= bits - 1)
        labels.push_back(bits & -bits);
    labels.push_back(first_block);
    labels.push_back(second_block);
    if (labels.size() != 6)
        throw std::runtime_error("macro tour requires six labels");
    constexpr int kLabelCount = 6;
    double metric[kLabelCount][kLabelCount] = {};
    for (int left = 0; left < kLabelCount; ++left)
        for (int right = left + 1; right < kLabelCount; ++right)
        {
            metric[left][right] = gst::fp::kInf;
            for (int vertex = 1; vertex <= vertex_count; ++vertex)
                metric[left][right] = std::min(
                    metric[left][right],
                    Value(rows, groups, labels[left], vertex) +
                        Value(rows, groups, labels[right], vertex));
            metric[right][left] = metric[left][right];
        }

    constexpr int kMaskCount = 1 << kLabelCount;
    double path[kMaskCount][kLabelCount];
    for (auto& row : path)
        std::fill(std::begin(row), std::end(row), gst::fp::kInf);
    path[1][0] = 0.0;
    for (int mask = 1; mask < kMaskCount; ++mask)
    {
        if (!(mask & 1))
            continue;
        for (int last = 0; last < kLabelCount; ++last)
        {
            if (!(mask & (1 << last)) || path[mask][last] >= gst::fp::kInf)
                continue;
            for (int next = 1; next < kLabelCount; ++next)
            {
                if (mask & (1 << next))
                    continue;
                path[mask | (1 << next)][next] = std::min(
                    path[mask | (1 << next)][next],
                    path[mask][last] + metric[last][next]);
            }
        }
    }
    double cycle = gst::fp::kInf;
    for (int last = 1; last < kLabelCount; ++last)
        cycle = std::min(
            cycle, path[kMaskCount - 1][last] + metric[last][0]);
    return 0.5 * cycle;
}

struct ThreeBlockResult
{
    double upper = gst::fp::kInf;
    int first = 0;
    int second = 0;
    int third = 0;
    int root = 0;
    long long partitions = 0;
    long long probes = 0;
    double milliseconds = 0.0;
};

ThreeBlockResult CompleteThreeBlocks(const gst::Graph& graph,
                                     const GroupData& groups,
                                     const LowRows& rows,
                                     int full_mask)
{
    const auto begin = Clock::now();
    ThreeBlockResult result;
    for (int first = 1; first < full_mask; ++first)
    {
        if (Bits(first) > rows.q)
            continue;
        const int remaining = full_mask ^ first;
        for (int second = remaining; second; second = (second - 1) & remaining)
        {
            const int third = remaining ^ second;
            if (!third || first > second || second > third ||
                Bits(second) > rows.q || Bits(third) > rows.q)
                continue;
            ++result.partitions;
            for (int vertex = 1; vertex <= graph.n; ++vertex)
            {
                const double candidate =
                    Value(rows, groups, first, vertex) +
                    Value(rows, groups, second, vertex) +
                    Value(rows, groups, third, vertex);
                if (candidate < result.upper)
                {
                    result.upper = candidate;
                    result.first = first;
                    result.second = second;
                    result.third = third;
                    result.root = vertex;
                }
                ++result.probes;
            }
        }
    }
    result.milliseconds =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return result;
}

struct State
{
    int declared = 0;
    int origin_pair = 0;
    int origin_root = 0;
    double cost = 0.0;
    std::vector<int> edges;
    std::vector<int> vertices;
    std::vector<int> growth_blocks;
    std::vector<double> profile;
};

double EdgeCost(const gst::Graph& graph, const std::vector<int>& edges)
{
    double result = 0.0;
    for (int edge : edges)
        result += graph.edges[edge].w;
    return result;
}

State Unite(const gst::Graph& graph,
            const State& state,
            const Witness& witness,
            int declared)
{
    State result;
    result.declared = declared;
    result.origin_pair = state.origin_pair;
    result.origin_root = state.origin_root;
    result.growth_blocks = state.growth_blocks;
    std::set_union(state.edges.begin(), state.edges.end(),
                   witness.edges.begin(), witness.edges.end(),
                   std::back_inserter(result.edges));
    std::set_union(state.vertices.begin(), state.vertices.end(),
                   witness.vertices.begin(), witness.vertices.end(),
                   std::back_inserter(result.vertices));
    result.cost = EdgeCost(graph, result.edges);
    return result;
}

void BuildProfile(State& state,
                  const LowRows& rows,
                  const GroupData& groups,
                  const std::vector<int>& low_masks)
{
    state.profile.assign(low_masks.size(), gst::fp::kInf);
    state.profile[0] = 0.0;
    for (size_t index = 1; index < low_masks.size(); ++index)
    {
        const int mask = low_masks[index];
        if (mask & state.declared)
            continue;
        for (int vertex : state.vertices)
            state.profile[index] =
                std::min(state.profile[index], Value(rows, groups, mask, vertex));
    }
}

bool Dominates(const State& first,
               const State& second,
               int remaining,
               const std::vector<int>& low_masks)
{
    if (first.cost > second.cost + gst::fp::kEps)
        return false;
    for (size_t index = 1; index < low_masks.size(); ++index)
        if ((low_masks[index] & remaining) == low_masks[index] &&
            first.profile[index] > second.profile[index] + gst::fp::kEps)
            return false;
    return true;
}

void Prune(std::vector<State>& states,
           const LowRows& rows,
           const GroupData& groups,
           int g,
           const std::vector<int>& low_masks)
{
    for (State& state : states)
        if (state.profile.empty())
            BuildProfile(state, rows, groups, low_masks);
    std::sort(states.begin(), states.end(), [](const State& a, const State& b)
    {
        if (a.edges != b.edges)
            return a.edges < b.edges;
        return a.cost < b.cost;
    });
    states.erase(std::unique(states.begin(), states.end(), [](const State& a, const State& b)
    {
        return a.edges == b.edges;
    }), states.end());
    std::vector<State> front;
    for (State& state : states)
    {
        const int remaining = ((1 << g) - 1) ^ state.declared;
        bool dominated = false;
        for (const State& other : front)
            if (Dominates(other, state, remaining, low_masks))
            {
                dominated = true;
                break;
            }
        if (dominated)
            continue;
        front.erase(std::remove_if(front.begin(), front.end(), [&](const State& other)
        {
            return Dominates(state, other, remaining, low_masks);
        }), front.end());
        front.push_back(std::move(state));
    }
    states.swap(front);
}

void Probe(const gst::Graph& graph,
           const gst::Query& query,
           int query_id,
           double known_optimum,
           bool profile_seeds,
           bool price_best_plan,
           bool price_strong_plans,
           bool price_factorized_plans,
           bool price_rooted_core_plans,
           bool price_optimal_core_plans,
           bool price_macro_core_plans,
           bool price_contracted_core_plans,
           bool price_pair_union_core_plans,
           bool price_all_attachment_core_plans,
           bool price_oracle_tree_core_plans,
           bool measure_near_full_lower,
           bool measure_macro_tour_lower,
           bool measure_block_anchor_keys,
           bool price_first_order_anchor_plans,
           bool measure_three_pendant_plans,
           bool measure_pendant_cherry_plans,
           bool measure_macro_packing_lower,
           bool measure_factor_columns,
           bool measure_factor_targets,
           int q_override,
           int core_size_override)
{
    const auto begin = Clock::now();
    const int g = static_cast<int>(query.groups.size());
    if (g < 5 || g > 16)
        throw std::runtime_error("paid core growth requires 5 <= g <= 16");
    const int half = g / 2;
    const int q = q_override > 0 ? q_override : (half + 1) / 2;
    if (q < 1 || q > half)
        throw std::runtime_error("q must satisfy 1 <= q <= floor(g/2)");
    const int core_size = core_size_override > 0
        ? core_size_override
        : g - 2 * q;
    if (core_size < 2 || core_size > g || g - core_size > 2 * q)
        throw std::runtime_error(
            "core size must leave at most two q-sized completion blocks");
    const int full_mask = (1 << g) - 1;
    const GroupData groups = BuildGroups(graph, query);

    int root = 1;
    double root_star = gst::fp::kInf;
    for (int vertex = 1; vertex <= graph.n; ++vertex)
    {
        double value = 0.0;
        for (int group = 0; group < g; ++group)
            value += groups.distance[group][vertex];
        if (value < root_star)
        {
            root_star = value;
            root = vertex;
        }
    }
    int anchor_group = 0;
    for (int group = 1; group < g; ++group)
        if (groups.distance[group][root] > groups.distance[anchor_group][root])
            anchor_group = group;

    const LowRows rows = BuildLowRows(graph, groups, g, q);
    const ThreeBlockResult three_block =
        CompleteThreeBlocks(graph, groups, rows, full_mask);
    std::vector<double> block_optimum(1 << g, gst::fp::kInf);
    block_optimum[0] = 0.0;
    for (int mask = 1; mask <= full_mask; ++mask)
    {
        if (Bits(mask) > q)
            continue;
        for (int vertex = 1; vertex <= graph.n; ++vertex)
            block_optimum[mask] = std::min(
                block_optimum[mask], Value(rows, groups, mask, vertex));
    }
    struct CorePlanLower
    {
        double lower = gst::fp::kInf;
        int core = 0;
        int left = 0;
        int right = 0;
    };
    std::vector<CorePlanLower> component_plans;
    std::vector<double> root_free_optimum(1 << g, gst::fp::kInf);
    for (int mask = 0; mask <= full_mask; ++mask)
        if (Bits(mask) <= q)
            root_free_optimum[mask] = block_optimum[mask];
    long long root_free_probes = 0;
    const auto root_free_begin = Clock::now();
    auto ComponentLower = [&](int mask)
    {
        if (block_optimum[mask] < gst::fp::kInf)
            return block_optimum[mask];
        double lower = 0.0;
        for (int subset = mask; subset; subset = (subset - 1) & mask)
            if (Bits(subset) <= q)
                lower = std::max(lower, block_optimum[subset]);
        return lower;
    };
    auto RootFreeOptimum = [&](int mask)
    {
        double& optimum = root_free_optimum[mask];
        if (optimum < gst::fp::kInf)
            return optimum;
        const int pivot = FirstBit(mask);
        const int branch_domain = mask ^ (1 << pivot);
        for (int right = branch_domain; right;
             right = (right - 1) & branch_domain)
        {
            const int left = mask ^ right;
            if (Bits(left) > q || Bits(right) > q)
                continue;
            for (int vertex = 1; vertex <= graph.n; ++vertex)
            {
                optimum = std::min(
                    optimum,
                    Value(rows, groups, left, vertex) +
                        Value(rows, groups, right, vertex));
                ++root_free_probes;
            }
        }
        return optimum;
    };
    for (int core = 1; core <= full_mask; ++core)
    {
        if (Bits(core) != core_size)
            continue;
        const int remaining = full_mask ^ core;
        for (int left = remaining; left; left = (left - 1) & remaining)
        {
            const int right = remaining ^ left;
            if (!right || left > right || Bits(left) > q || Bits(right) > q)
                continue;
            component_plans.push_back(
                {ComponentLower(core) + block_optimum[left] +
                     block_optimum[right],
                 core, left, right});
        }
    }
    std::vector<CorePlanLower> strong_plans = component_plans;
    for (CorePlanLower& plan : strong_plans)
    {
        const double core_left = RootFreeOptimum(plan.core | plan.left);
        const double core_right = RootFreeOptimum(plan.core | plan.right);
        if (core_left >= gst::fp::kInf || core_right >= gst::fp::kInf)
            continue;
        plan.lower = std::max(
            plan.lower,
            std::max(
                std::max(core_left + block_optimum[plan.right],
                         core_right + block_optimum[plan.left]),
                0.5 * (core_left + core_right +
                       block_optimum[plan.left] +
                       block_optimum[plan.right])));
    }
    double near_full_lower = 0.0;
    long long near_full_masks = 0;
    const long long near_full_probe_begin = root_free_probes;
    const auto near_full_begin = Clock::now();
    if (measure_near_full_lower)
    {
        const int near_full_size = std::min(g, 2 * q);
        for (int mask = 1; mask <= full_mask; ++mask)
        {
            if (Bits(mask) != near_full_size)
                continue;
            near_full_lower = std::max(
                near_full_lower, RootFreeOptimum(mask));
            ++near_full_masks;
        }
    }
    const long long near_full_probes =
        root_free_probes - near_full_probe_begin;
    const double near_full_ms =
        std::chrono::duration<double, std::milli>(Clock::now() -
                                                   near_full_begin)
            .count();
    const double root_free_ms =
        std::chrono::duration<double, std::milli>(Clock::now() -
                                                   root_free_begin)
            .count();
    std::sort(component_plans.begin(), component_plans.end(),
              [](const CorePlanLower& a, const CorePlanLower& b)
    {
        if (a.lower != b.lower)
            return a.lower < b.lower;
        if (a.core != b.core)
            return a.core < b.core;
        if (a.left != b.left)
            return a.left < b.left;
        return a.right < b.right;
    });
    std::sort(strong_plans.begin(), strong_plans.end(),
              [](const CorePlanLower& a, const CorePlanLower& b)
    {
        if (a.lower != b.lower)
            return a.lower < b.lower;
        if (a.core != b.core)
            return a.core < b.core;
        if (a.left != b.left)
            return a.left < b.left;
        return a.right < b.right;
    });
    long long component_below_optimum = 0;
    for (const CorePlanLower& plan : component_plans)
        if (plan.lower + gst::fp::kEps < known_optimum)
            ++component_below_optimum;
    long long strong_below_optimum = 0;
    for (const CorePlanLower& plan : strong_plans)
        if (plan.lower + gst::fp::kEps < known_optimum)
            ++strong_below_optimum;
    double macro_tour_best_lower = gst::fp::kInf;
    long long macro_tour_below_optimum = 0;
    long long macro_tour_closed_plans = 0;
    double macro_tour_ms = 0.0;
    std::vector<double> macro_tour_plan_lowers;
    if (measure_macro_tour_lower)
    {
        const auto macro_tour_begin = Clock::now();
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            const double lower = std::max(
                plan.lower,
                MacroTourHalf(
                    groups, rows, graph.n,
                    plan.core, plan.left, plan.right));
            macro_tour_plan_lowers.push_back(lower);
            macro_tour_best_lower = std::min(
                macro_tour_best_lower, lower);
            if (lower + gst::fp::kEps < known_optimum)
                ++macro_tour_below_optimum;
            else
                ++macro_tour_closed_plans;
        }
        macro_tour_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       macro_tour_begin)
                .count();
    }
    double macro_packing_best_lower = gst::fp::kInf;
    long long macro_packing_below_optimum = 0;
    long long macro_packing_closed_plans = 0;
    long long macro_packing_root_free_probes = 0;
    double macro_packing_ms = 0.0;
    if (measure_macro_packing_lower)
    {
        const auto packing_begin = Clock::now();
        const long long packing_probe_begin = root_free_probes;
        constexpr int kMacroCount = 6;
        constexpr int kMacroFull = (1 << kMacroCount) - 1;
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            std::vector<int> labels;
            for (int bits = plan.core; bits; bits &= bits - 1)
                labels.push_back(bits & -bits);
            labels.push_back(plan.left);
            labels.push_back(plan.right);

            std::vector<double> objective(kMacroFull - 1);
            for (int subset = 1; subset < kMacroFull; ++subset)
            {
                int original = 0;
                for (int bits = subset; bits; bits &= bits - 1)
                    original |= labels[FirstBit(bits)];
                objective[subset - 1] = Bits(original) <= 2 * q
                    ? RootFreeOptimum(original)
                    : ComponentLower(original);
            }

            std::vector<std::vector<double>> constraints;
            for (int cut = 1; cut < kMacroFull; ++cut)
            {
                if (!(cut & 1))
                    continue;
                constraints.emplace_back(kMacroFull - 1, 0.0);
                for (int subset = 1; subset < kMacroFull; ++subset)
                    if ((subset & cut) && (subset & (kMacroFull ^ cut)))
                        constraints.back()[subset - 1] = 1.0;
            }
            for (int block_label : {4, 5})
            {
                constraints.emplace_back(kMacroFull - 1, 0.0);
                for (int subset = 1; subset < kMacroFull; ++subset)
                    if (subset & (1 << block_label))
                        constraints.back()[subset - 1] = 1.0;
            }
            std::vector<double> bounds(constraints.size(), 1.0);
            const double packed = SimplexSolver(
                constraints, bounds, objective).Solve();
            const double lower = std::max(plan.lower, packed);
            macro_packing_best_lower = std::min(
                macro_packing_best_lower, lower);
            if (lower + gst::fp::kEps < known_optimum)
                ++macro_packing_below_optimum;
            else
                ++macro_packing_closed_plans;
        }
        macro_packing_root_free_probes =
            root_free_probes - packing_probe_begin;
        macro_packing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       packing_begin)
                .count();
    }
    long long block_anchor_raw_keys = 0;
    long long block_anchor_unique_keys = 0;
    int block_anchor_distinct_blocks = 0;
    int block_anchor_max_keys_per_block = 0;
    std::vector<long long> block_anchor_keys_by_size(5);
    long long block_anchor_closed_keys = 0;
    std::vector<long long> block_anchor_closed_by_size(3);
    double block_anchor_key_ms = 0.0;
    if (measure_block_anchor_keys)
    {
        if (core_size != 4)
            throw std::runtime_error(
                "block-anchor key measurement requires a four-group core");
        const auto key_begin = Clock::now();
        std::vector<std::pair<int, int>> keys;
        std::vector<std::pair<int, int>> closed_keys;
        auto AddDependencies = [&](int block, int mask)
        {
            for (int subset = mask;; subset = (subset - 1) & mask)
            {
                closed_keys.push_back({block, subset});
                if (!subset)
                    break;
            }
        };
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            const int closed_block = std::min(plan.left, plan.right);
            const int raw_block = std::max(plan.left, plan.right);
            for (int subset = plan.core;;
                 subset = (subset - 1) & plan.core)
            {
                keys.push_back({plan.left, subset});
                keys.push_back({plan.right, plan.core ^ subset});
                block_anchor_raw_keys += 2;
                if (!subset)
                    break;
            }
            for (int subset = plan.core;;
                 subset = (subset - 1) & plan.core)
            {
                if (Bits(subset) <= 2)
                    AddDependencies(closed_block, subset);
                if (Bits(subset) <= 1)
                    AddDependencies(raw_block, subset);
                if (!subset)
                    break;
            }
        }
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
        block_anchor_unique_keys = keys.size();
        for (size_t begin = 0; begin < keys.size();)
        {
            size_t end = begin + 1;
            while (end < keys.size() && keys[end].first == keys[begin].first)
                ++end;
            ++block_anchor_distinct_blocks;
            block_anchor_max_keys_per_block = std::max(
                block_anchor_max_keys_per_block,
                static_cast<int>(end - begin));
            for (size_t index = begin; index < end; ++index)
                ++block_anchor_keys_by_size[Bits(keys[index].second)];
            begin = end;
        }
        std::sort(closed_keys.begin(), closed_keys.end());
        closed_keys.erase(
            std::unique(closed_keys.begin(), closed_keys.end()),
            closed_keys.end());
        block_anchor_closed_keys = closed_keys.size();
        for (const auto [block, mask] : closed_keys)
        {
            (void)block;
            ++block_anchor_closed_by_size[Bits(mask)];
        }
        block_anchor_key_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       key_begin)
                .count();
    }
    double first_order_anchor_family_best = gst::fp::kInf;
    int first_order_anchor_first_exact_rank = 0;
    long long first_order_anchor_rows_d1 = 0;
    long long first_order_anchor_rows_d2 = 0;
    long long first_order_anchor_candidate_plans = 0;
    long long first_order_anchor_row_values = 0;
    long long first_order_anchor_queue_pops = 0;
    long long first_order_anchor_plan_probes = 0;
    double first_order_anchor_build_ms = 0.0;
    double first_order_anchor_pricing_ms = 0.0;
    if (price_first_order_anchor_plans)
    {
        if (core_size != 4)
            throw std::runtime_error(
                "first-order anchor pricing requires a four-group core");
        struct AnchorRow
        {
            int block = 0;
            int mask = 0;
            std::vector<double> distance;
        };
        const double pricing_limit = three_block.upper;
        std::vector<std::pair<int, int>> required_keys;
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= pricing_limit)
                break;
            ++first_order_anchor_candidate_plans;
            const int closed_block = std::min(plan.left, plan.right);
            const int raw_block = std::max(plan.left, plan.right);
            for (int subset = plan.core;;
                 subset = (subset - 1) & plan.core)
            {
                if (Bits(subset) <= 2)
                    required_keys.push_back({closed_block, subset});
                if (Bits(subset) <= 1)
                    required_keys.push_back({raw_block, subset});
                if (!subset)
                    break;
            }
        }
        std::sort(required_keys.begin(), required_keys.end());
        required_keys.erase(
            std::unique(required_keys.begin(), required_keys.end()),
            required_keys.end());
        std::vector<AnchorRow> anchor_rows;
        for (const auto [block, mask] : required_keys)
            if (mask)
                anchor_rows.push_back({block, mask, {}});
        auto FindAnchorRow = [&](int block, int mask) -> AnchorRow&
        {
            const auto position = std::lower_bound(
                anchor_rows.begin(), anchor_rows.end(),
                std::pair<int, int>{block, mask},
                [](const AnchorRow& row, const std::pair<int, int>& key)
                {
                    return std::pair<int, int>{row.block, row.mask} < key;
                });
            if (position == anchor_rows.end() ||
                position->block != block || position->mask != mask)
                throw std::runtime_error("missing first-order anchor row");
            return *position;
        };
        auto OrdinaryDistance = [&](int mask) -> const std::vector<double>&
        {
            return mask & (mask - 1)
                ? rows.row[mask].distance
                : groups.distance[FirstBit(mask)];
        };
        auto AnchorDistance = [&](int block, int mask)
            -> const std::vector<double>&
        {
            return mask
                ? FindAnchorRow(block, mask).distance
                : rows.row[block].distance;
        };

        const auto build_begin = Clock::now();
        for (int size = 1; size <= 2; ++size)
            for (AnchorRow& row : anchor_rows)
            {
                if (Bits(row.mask) != size)
                    continue;
                row.distance.assign(graph.n + 1, gst::fp::kInf);
                for (int branch = row.mask; branch;
                     branch = (branch - 1) & row.mask)
                {
                    const int base = row.mask ^ branch;
                    const std::vector<double>& base_distance =
                        AnchorDistance(row.block, base);
                    const std::vector<double>& branch_distance =
                        OrdinaryDistance(branch);
                    for (int vertex = 1; vertex <= graph.n; ++vertex)
                        row.distance[vertex] = std::min(
                            row.distance[vertex],
                            base_distance[vertex] +
                                branch_distance[vertex]);
                }
                CloseDenseRow(
                    graph, row.distance, first_order_anchor_queue_pops);
                first_order_anchor_row_values += graph.n;
                if (size == 1)
                    ++first_order_anchor_rows_d1;
                else
                    ++first_order_anchor_rows_d2;
            }
        first_order_anchor_build_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       build_begin)
                .count();

        const auto pricing_begin = Clock::now();
        int rank = 0;
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= pricing_limit)
                break;
            ++rank;
            const int closed_block = std::min(plan.left, plan.right);
            const int raw_block = std::max(plan.left, plan.right);
            double priced = gst::fp::kInf;
            for (int raw_mask = plan.core;;
                 raw_mask = (raw_mask - 1) & plan.core)
            {
                if (Bits(raw_mask) <= 1)
                {
                    const int remaining = plan.core ^ raw_mask;
                    for (int closed_mask = remaining;;
                         closed_mask = (closed_mask - 1) & remaining)
                    {
                        const int ordinary_mask = remaining ^ closed_mask;
                        if (ordinary_mask && Bits(closed_mask) <= 2)
                        {
                            const std::vector<double>& raw_distance =
                                AnchorDistance(raw_block, raw_mask);
                            const std::vector<double>& closed_distance =
                                AnchorDistance(closed_block, closed_mask);
                            const std::vector<double>& ordinary_distance =
                                OrdinaryDistance(ordinary_mask);
                            for (int vertex = 1; vertex <= graph.n; ++vertex)
                            {
                                priced = std::min(
                                    priced,
                                    raw_distance[vertex] +
                                        closed_distance[vertex] +
                                        ordinary_distance[vertex]);
                                ++first_order_anchor_plan_probes;
                            }
                        }
                        if (!closed_mask)
                            break;
                    }
                }
                if (!raw_mask)
                    break;
            }
            first_order_anchor_family_best = std::min(
                first_order_anchor_family_best, priced);
            if (!first_order_anchor_first_exact_rank &&
                priced <= known_optimum + gst::fp::kEps)
                first_order_anchor_first_exact_rank = rank;
        }
        first_order_anchor_pricing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       pricing_begin)
                .count();
    }
    int three_pendant_min_block = 0;
    int three_pendant_max_block = 0;
    long long three_pendant_plan_count = 0;
    long long three_pendant_below_optimum = 0;
    long long three_pendant_below_upper = 0;
    double three_pendant_best_lower = gst::fp::kInf;
    long long three_pendant_unique_anchor_keys = 0;
    std::vector<long long> three_pendant_anchor_keys_by_size(5);
    double three_pendant_ms = 0.0;
    if (measure_three_pendant_plans)
    {
        const auto pendant_begin = Clock::now();
        three_pendant_min_block = (g - 4 + 2) / 3;
        three_pendant_max_block = 2 * three_pendant_min_block - 2;
        if (three_pendant_min_block < 2 ||
            g - 2 * three_pendant_max_block < three_pendant_min_block ||
            three_pendant_max_block > q)
            throw std::runtime_error(
                "three-pendant decomposition is not supported by this q");

        std::vector<std::pair<int, int>> anchor_keys;
        for (int first = 1; first <= full_mask; ++first)
        {
            const int first_size = Bits(first);
            if (first_size < three_pendant_min_block ||
                first_size > three_pendant_max_block)
                continue;
            const int after_first = full_mask ^ first;
            for (int second = after_first; second;
                 second = (second - 1) & after_first)
            {
                const int second_size = Bits(second);
                if (first >= second ||
                    second_size < three_pendant_min_block ||
                    second_size > three_pendant_max_block)
                    continue;
                const int after_second = after_first ^ second;
                for (int third = after_second; third;
                     third = (third - 1) & after_second)
                {
                    const int third_size = Bits(third);
                    if (second >= third ||
                        third_size < three_pendant_min_block ||
                        third_size > three_pendant_max_block)
                        continue;
                    const int core = after_second ^ third;
                    if (Bits(core) > 4)
                        continue;

                    const int pieces[4] = {first, second, third, core};
                    double lower = block_optimum[first] +
                        block_optimum[second] + block_optimum[third] +
                        block_optimum[core];
                    for (int left = 0; left < 4; ++left)
                        if (pieces[left])
                            for (int right = left + 1; right < 4; ++right)
                                if (pieces[right])
                                {
                                    double candidate = RootFreeOptimum(
                                        pieces[left] | pieces[right]);
                                    for (int other = 0; other < 4; ++other)
                                        if (other != left && other != right)
                                            candidate +=
                                                block_optimum[pieces[other]];
                                    lower = std::max(lower, candidate);
                                }

                    ++three_pendant_plan_count;
                    three_pendant_best_lower = std::min(
                        three_pendant_best_lower, lower);
                    if (lower + gst::fp::kEps < known_optimum)
                        ++three_pendant_below_optimum;
                    if (lower + gst::fp::kEps < three_block.upper)
                    {
                        ++three_pendant_below_upper;
                        for (int subset = core;;
                             subset = (subset - 1) & core)
                        {
                            anchor_keys.push_back({first, subset});
                            anchor_keys.push_back({second, subset});
                            anchor_keys.push_back({third, subset});
                            if (!subset)
                                break;
                        }
                    }
                }
            }
        }
        std::sort(anchor_keys.begin(), anchor_keys.end());
        anchor_keys.erase(
            std::unique(anchor_keys.begin(), anchor_keys.end()),
            anchor_keys.end());
        three_pendant_unique_anchor_keys = anchor_keys.size();
        for (const auto [block, mask] : anchor_keys)
        {
            (void)block;
            ++three_pendant_anchor_keys_by_size[Bits(mask)];
        }
        three_pendant_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       pendant_begin)
                .count();
    }
    long long pendant_cherry_plan_count = 0;
    long long pendant_cherry_below_optimum = 0;
    long long pendant_cherry_below_upper = 0;
    double pendant_cherry_best_lower = gst::fp::kInf;
    std::vector<long long> pendant_cherry_plans_by_labels(5);
    long long pendant_cherry_unique_atoms = 0;
    std::vector<long long> pendant_cherry_atoms_by_size(5);
    long long pendant_cherry_raw_targets = 0;
    long long pendant_cherry_target_records = 0;
    long long pendant_cherry_all_target_records = 0;
    long long pendant_cherry_half_rows = 0;
    long long pendant_cherry_ordinary_rows = 0;
    long long pendant_cherry_anchored_rows = 0;
    int pendant_cherry_max_targets_per_row = 0;
    std::vector<long long> pendant_cherry_rows_by_size(17);
    double pendant_cherry_family_best = gst::fp::kInf;
    long long pendant_cherry_price_probes = 0;
    double pendant_cherry_pricing_ms = 0.0;
    double pendant_anchor_witness_price = gst::fp::kInf;
    double pendant_anchor_standard_price = gst::fp::kInf;
    double pendant_anchor_witness_edge_cost = 0.0;
    int pendant_anchor_witness_core = 0;
    std::vector<int> pendant_anchor_witness_blocks(3);
    long long pendant_anchor_witness_queue_pops = 0;
    double pendant_anchor_witness_ms = 0.0;
    double pendant_cherry_ms = 0.0;
    if (measure_pendant_cherry_plans)
    {
        if (q < 4)
            throw std::runtime_error(
                "pendant-cherry measurement requires D4 low rows");
        const auto cherry_begin = Clock::now();
        std::vector<int> atom_keys;
        std::vector<std::tuple<int, int, int>> targets;
        std::vector<std::tuple<int, int, int>> all_targets;
        std::vector<int> blocks;
        auto AddTargets = [&](const std::vector<int>& atoms,
                              std::vector<std::tuple<int, int, int>>& output)
        {
            if (atoms.size() != 4)
                return;
            for (int partner = 1; partner < 4; ++partner)
            {
                std::vector<int> rest;
                for (int index = 1; index < 4; ++index)
                    if (index != partner)
                        rest.push_back(atoms[index]);
                const int first_mask = atoms[0] | atoms[partner];
                const int second_mask = rest[0] | rest[1];
                int half_mask = first_mask;
                int consumer_first = rest[0];
                int consumer_second = rest[1];
                if (Bits(first_mask) > half ||
                    (Bits(second_mask) <= half &&
                     second_mask < first_mask))
                {
                    half_mask = second_mask;
                    consumer_first = atoms[0];
                    consumer_second = atoms[partner];
                }
                if (Bits(half_mask) > half)
                    throw std::runtime_error(
                        "four-atom plan has no half-sized central side");
                if (consumer_first > consumer_second)
                    std::swap(consumer_first, consumer_second);
                output.push_back(
                    {half_mask, consumer_first, consumer_second});
            }
        };
        auto RecordPlan = [&](int core)
        {
            std::vector<int> atoms = blocks;
            if (core)
                atoms.push_back(core);
            if (atoms.size() < 3 || atoms.size() > 4)
                return;

            double component_sum = 0.0;
            for (int atom : atoms)
                component_sum += block_optimum[atom];
            double lower = component_sum;
            for (size_t first = 0; first < atoms.size(); ++first)
                for (size_t second = first + 1; second < atoms.size();
                     ++second)
                {
                    const double pair = RootFreeOptimum(
                        atoms[first] | atoms[second]);
                    lower = std::max(
                        lower,
                        pair + component_sum - block_optimum[atoms[first]] -
                            block_optimum[atoms[second]]);
                }

            ++pendant_cherry_plan_count;
            ++pendant_cherry_plans_by_labels[atoms.size()];
            pendant_cherry_best_lower = std::min(
                pendant_cherry_best_lower, lower);
            if (lower + gst::fp::kEps < known_optimum)
                ++pendant_cherry_below_optimum;
            AddTargets(atoms, all_targets);
            if (atoms.size() == 3)
                for (int vertex = 1; vertex <= graph.n; ++vertex)
                    pendant_cherry_family_best = std::min(
                        pendant_cherry_family_best,
                        Value(rows, groups, atoms[0], vertex) +
                            Value(rows, groups, atoms[1], vertex) +
                            Value(rows, groups, atoms[2], vertex));
            if (lower + gst::fp::kEps >= three_block.upper)
                return;

            ++pendant_cherry_below_upper;
            atom_keys.insert(atom_keys.end(), atoms.begin(), atoms.end());
            const size_t before = targets.size();
            AddTargets(atoms, targets);
            pendant_cherry_raw_targets += targets.size() - before;
        };
        auto Enumerate = [&](auto&& self, int remaining, int minimum_block,
                             int blocks_left) -> void
        {
            if (!blocks_left)
            {
                if (Bits(remaining) <= 4)
                    RecordPlan(remaining);
                return;
            }
            for (int block = remaining; block;
                 block = (block - 1) & remaining)
            {
                const int size = Bits(block);
                if (block <= minimum_block || size < 3 || size > 4)
                    continue;
                const int after = remaining ^ block;
                const int after_size = Bits(after);
                if (after_size < 3 * (blocks_left - 1) ||
                    after_size > 4 * (blocks_left - 1) + 4)
                    continue;
                blocks.push_back(block);
                self(self, after, block, blocks_left - 1);
                blocks.pop_back();
            }
        };
        Enumerate(Enumerate, full_mask, 0, 3);

        std::sort(atom_keys.begin(), atom_keys.end());
        atom_keys.erase(
            std::unique(atom_keys.begin(), atom_keys.end()), atom_keys.end());
        pendant_cherry_unique_atoms = atom_keys.size();
        for (int atom : atom_keys)
            ++pendant_cherry_atoms_by_size[Bits(atom)];

        std::sort(targets.begin(), targets.end());
        targets.erase(std::unique(targets.begin(), targets.end()),
                      targets.end());
        pendant_cherry_target_records = targets.size();
        for (size_t begin = 0; begin < targets.size();)
        {
            size_t end = begin + 1;
            while (end < targets.size() &&
                   std::get<0>(targets[end]) ==
                       std::get<0>(targets[begin]))
                ++end;
            const int mask = std::get<0>(targets[begin]);
            ++pendant_cherry_half_rows;
            ++pendant_cherry_rows_by_size[Bits(mask)];
            if (mask & (1 << anchor_group))
                ++pendant_cherry_anchored_rows;
            else
                ++pendant_cherry_ordinary_rows;
            pendant_cherry_max_targets_per_row = std::max(
                pendant_cherry_max_targets_per_row,
                static_cast<int>(end - begin));
            begin = end;
        }

        std::sort(all_targets.begin(), all_targets.end());
        all_targets.erase(
            std::unique(all_targets.begin(), all_targets.end()),
            all_targets.end());
        pendant_cherry_all_target_records = all_targets.size();
        if (q >= half)
        {
            const auto pricing_begin = Clock::now();
            for (const auto [mask, first, second] : all_targets)
                for (int vertex = 1; vertex <= graph.n; ++vertex)
                {
                    pendant_cherry_family_best = std::min(
                        pendant_cherry_family_best,
                        Value(rows, groups, mask, vertex) +
                            Value(rows, groups, first, vertex) +
                            Value(rows, groups, second, vertex));
                    ++pendant_cherry_price_probes;
                }
            pendant_cherry_pricing_ms =
                std::chrono::duration<double, std::milli>(Clock::now() -
                                                           pricing_begin)
                    .count();

            const auto witness_begin = Clock::now();
            std::vector<int> edge_stamp(graph.edges.size());
            std::vector<int> vertex_stamp(graph.n + 1);
            int stamp = 0;
            std::vector<int> selected_terminal(g, -1);
            std::vector<int> tree_edges;
            for (int top_block : {three_block.first, three_block.second,
                                  three_block.third})
            {
                std::vector<int> selected;
                const Witness witness = BuildWitness(
                    graph, groups, rows, top_block, three_block.root,
                    edge_stamp, vertex_stamp, stamp, &selected);
                tree_edges.insert(tree_edges.end(), witness.edges.begin(),
                                  witness.edges.end());
                for (int group = 0; group < g; ++group)
                    if (selected[group] >= 0)
                        selected_terminal[group] = selected[group];
            }
            std::sort(tree_edges.begin(), tree_edges.end());
            tree_edges.erase(
                std::unique(tree_edges.begin(), tree_edges.end()),
                tree_edges.end());
            for (int edge_id : tree_edges)
                pendant_anchor_witness_edge_cost += graph.edges[edge_id].w;

            std::vector<std::vector<int>> tree(graph.n + 1);
            for (int edge_id : tree_edges)
            {
                const auto& edge = graph.edges[edge_id];
                tree[edge.u].push_back(edge.v);
                tree[edge.v].push_back(edge.u);
            }
            std::vector<int> parent(graph.n + 1, -1);
            std::vector<int> order{three_block.root};
            parent[three_block.root] = 0;
            for (size_t index = 0; index < order.size(); ++index)
            {
                const int vertex = order[index];
                for (int next : tree[vertex])
                    if (next != parent[vertex])
                    {
                        if (parent[next] >= 0)
                            throw std::runtime_error(
                                "optimal witness union is not a tree");
                        parent[next] = vertex;
                        order.push_back(next);
                    }
            }
            std::vector<int> tokens(graph.n + 1);
            for (int group = 0; group < g; ++group)
            {
                if (selected_terminal[group] < 0)
                    throw std::runtime_error(
                        "optimal witness omitted a selected group");
                tokens[selected_terminal[group]] |= 1 << group;
            }

            struct BinaryNode
            {
                int left = -1;
                int right = -1;
                int token = 0;
            };
            std::vector<BinaryNode> binary;
            std::vector<int> binary_root(graph.n + 1, -1);
            auto AddBinaryNode = [&](int left, int right, int token)
            {
                binary.push_back({left, right, token});
                return static_cast<int>(binary.size()) - 1;
            };
            for (auto position = order.rbegin(); position != order.rend();
                 ++position)
            {
                const int vertex = *position;
                std::vector<int> units;
                for (int child : tree[vertex])
                    if (parent[child] == vertex && binary_root[child] >= 0)
                        units.push_back(binary_root[child]);
                for (int bits = tokens[vertex]; bits; bits &= bits - 1)
                    units.push_back(AddBinaryNode(-1, -1, bits & -bits));
                while (units.size() > 1)
                {
                    const int right = units.back();
                    units.pop_back();
                    const int left = units.back();
                    units.pop_back();
                    units.push_back(AddBinaryNode(left, right, 0));
                }
                if (!units.empty())
                    binary_root[vertex] = units[0];
            }
            const int binary_tree_root = binary_root[three_block.root];
            if (binary_tree_root < 0)
                throw std::runtime_error("optimal witness has no token tree");
            std::vector<int> binary_parent(binary.size(), -1);
            std::vector<int> binary_depth(binary.size());
            std::vector<int> binary_order{binary_tree_root};
            for (size_t index = 0; index < binary_order.size(); ++index)
            {
                const int node = binary_order[index];
                for (int child : {binary[node].left, binary[node].right})
                    if (child >= 0)
                    {
                        binary_parent[child] = node;
                        binary_depth[child] = binary_depth[node] + 1;
                        binary_order.push_back(child);
                    }
            }
            int active = full_mask;
            for (int round = 0; round < 3; ++round)
            {
                std::vector<int> subtree(binary.size());
                for (auto position = binary_order.rbegin();
                     position != binary_order.rend();
                     ++position)
                {
                    const int node = *position;
                    subtree[node] |= binary[node].token & active;
                    if (binary_parent[node] >= 0)
                        subtree[binary_parent[node]] |= subtree[node];
                }
                int junction = -1;
                for (int node : binary_order)
                    if (Bits(subtree[node]) >= 3 &&
                        (junction < 0 ||
                         binary_depth[node] > binary_depth[junction]))
                        junction = node;
                if (junction < 0)
                    throw std::runtime_error(
                        "failed to locate a pendant 3/4-token junction");
                const int block = subtree[junction] & active;
                if (Bits(block) < 3 || Bits(block) > 4)
                    throw std::runtime_error(
                        "binary pendant subtree is not a 3/4-token block");
                pendant_anchor_witness_blocks[round] = block;
                active ^= block;
            }
            pendant_anchor_witness_core = active;
            if (Bits(active) > 4)
                throw std::runtime_error(
                    "three pendant extractions left more than four groups");

            std::vector<int> core_labels;
            for (int bits = active; bits; bits &= bits - 1)
                core_labels.push_back(bits & -bits);
            const int core_count = static_cast<int>(core_labels.size());
            const int core_subsets = 1 << core_count;
            std::vector<int> original_core(core_subsets);
            for (int mask = 1; mask < core_subsets; ++mask)
            {
                const int bit = FirstBit(mask);
                original_core[mask] =
                    original_core[mask ^ (1 << bit)] | core_labels[bit];
            }
            std::vector<std::vector<std::vector<double>>> anchored(3);
            for (int block_index = 0; block_index < 3; ++block_index)
            {
                anchored[block_index].assign(
                    core_subsets,
                    std::vector<double>(graph.n + 1, gst::fp::kInf));
                anchored[block_index][0] = rows.row[
                    pendant_anchor_witness_blocks[block_index]].distance;
                for (int mask = 1; mask < core_subsets; ++mask)
                {
                    auto& distance = anchored[block_index][mask];
                    for (int branch = mask; branch;
                         branch = (branch - 1) & mask)
                    {
                        const auto& base = anchored[block_index][mask ^ branch];
                        const int ordinary_mask = original_core[branch];
                        for (int vertex = 1; vertex <= graph.n; ++vertex)
                            distance[vertex] = std::min(
                                distance[vertex],
                                base[vertex] +
                                    Value(rows, groups, ordinary_mask,
                                          vertex));
                    }
                    CloseDenseRow(
                        graph, distance, pendant_anchor_witness_queue_pops);
                }
            }
            const int core_full = core_subsets - 1;
            for (int first = 0; first <= core_full; ++first)
            {
                const int remaining = core_full ^ first;
                for (int second = remaining;;
                     second = (second - 1) & remaining)
                {
                    const int third = remaining ^ second;
                    const int first_mask =
                        pendant_anchor_witness_blocks[0] |
                        original_core[first];
                    const int second_mask =
                        pendant_anchor_witness_blocks[1] |
                        original_core[second];
                    const int third_mask =
                        pendant_anchor_witness_blocks[2] |
                        original_core[third];
                    for (int vertex = 1; vertex <= graph.n; ++vertex)
                    {
                        pendant_anchor_witness_price = std::min(
                            pendant_anchor_witness_price,
                            anchored[0][first][vertex] +
                                anchored[1][second][vertex] +
                                anchored[2][third][vertex]);
                        if (Bits(first_mask) <= q &&
                            Bits(second_mask) <= q &&
                            Bits(third_mask) <= q)
                            pendant_anchor_standard_price = std::min(
                                pendant_anchor_standard_price,
                                Value(rows, groups, first_mask, vertex) +
                                    Value(rows, groups, second_mask, vertex) +
                                    Value(rows, groups, third_mask, vertex));
                    }
                    if (!second)
                        break;
                }
            }
            pendant_anchor_witness_ms =
                std::chrono::duration<double, std::milli>(Clock::now() -
                                                           witness_begin)
                    .count();
        }
        pendant_cherry_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       cherry_begin)
                .count();
    }
    std::vector<int> strong_plans_per_core(1 << g);
    std::vector<int> strong_cores_per_pair(1 << g);
    struct PairCoreBudget
    {
        int pair = 0;
        int core = 0;
        double root_limit = -gst::fp::kInf;
    };
    std::vector<PairCoreBudget> pair_core_budgets;
    long long strong_plan_pair_root_probes = 0;
    for (const CorePlanLower& plan : strong_plans)
    {
        if (plan.lower + gst::fp::kEps >= known_optimum)
            break;
        ++strong_plans_per_core[plan.core];
        const double root_limit = known_optimum -
            block_optimum[plan.left] - block_optimum[plan.right];
        for (int pair = plan.core; pair; pair = (pair - 1) & plan.core)
        {
            if (Bits(pair) != 2 || (pair & (1 << anchor_group)))
                continue;
            pair_core_budgets.push_back({pair, plan.core, root_limit});
            const auto& pair_row = rows.row[pair].distance;
            for (int root = 1; root <= graph.n; ++root)
                if (pair_row[root] + gst::fp::kEps < root_limit)
                    ++strong_plan_pair_root_probes;
        }
    }
    std::sort(pair_core_budgets.begin(), pair_core_budgets.end(),
              [](const PairCoreBudget& a, const PairCoreBudget& b)
    {
        if (a.pair != b.pair)
            return a.pair < b.pair;
        if (a.core != b.core)
            return a.core < b.core;
        return a.root_limit > b.root_limit;
    });
    std::vector<PairCoreBudget> unique_pair_cores;
    for (const PairCoreBudget& budget : pair_core_budgets)
    {
        if (!unique_pair_cores.empty() &&
            unique_pair_cores.back().pair == budget.pair &&
            unique_pair_cores.back().core == budget.core)
        {
            unique_pair_cores.back().root_limit = std::max(
                unique_pair_cores.back().root_limit, budget.root_limit);
            continue;
        }
        unique_pair_cores.push_back(budget);
    }
    int strong_distinct_cores = 0;
    int strong_max_plans_per_core = 0;
    for (int core = 0; core <= full_mask; ++core)
        if (strong_plans_per_core[core])
        {
            ++strong_distinct_cores;
            strong_max_plans_per_core = std::max(
                strong_max_plans_per_core, strong_plans_per_core[core]);
        }
    long long strong_bound_core_root_scans = 0;
    for (const PairCoreBudget& budget : unique_pair_cores)
    {
        ++strong_cores_per_pair[budget.pair];
        const auto& pair_row = rows.row[budget.pair].distance;
        for (int root = 1; root <= graph.n; ++root)
            if (pair_row[root] + gst::fp::kEps < budget.root_limit)
                ++strong_bound_core_root_scans;
    }
    int strong_distinct_pairs = 0;
    int strong_max_cores_per_pair = 0;
    long long strong_bound_pair_root_scans = 0;
    std::vector<std::pair<int, int>> factor_profile_keys;
    for (int pair = 0; pair <= full_mask; ++pair)
    {
        if (!strong_cores_per_pair[pair])
            continue;
        ++strong_distinct_pairs;
        strong_max_cores_per_pair = std::max(
            strong_max_cores_per_pair, strong_cores_per_pair[pair]);
        double root_limit = -gst::fp::kInf;
        for (const PairCoreBudget& budget : unique_pair_cores)
            if (budget.pair == pair)
                root_limit = std::max(root_limit, budget.root_limit);
        const auto& pair_row = rows.row[pair].distance;
        for (int root = 1; root <= graph.n; ++root)
            if (pair_row[root] + gst::fp::kEps < root_limit)
                ++strong_bound_pair_root_scans;
    }
    for (const CorePlanLower& plan : strong_plans)
    {
        if (plan.lower + gst::fp::kEps >= known_optimum)
            break;
        for (int pair = plan.core; pair; pair = (pair - 1) & plan.core)
        {
            if (Bits(pair) != 2 || (pair & (1 << anchor_group)))
                continue;
            const int added = plan.core ^ pair;
            factor_profile_keys.push_back({pair, added});
            factor_profile_keys.push_back({pair, plan.left});
            factor_profile_keys.push_back({pair, plan.right});
            if (Bits(added) == 2)
            {
                factor_profile_keys.push_back({added, plan.left});
                factor_profile_keys.push_back({added, plan.right});
            }
        }
    }
    std::sort(factor_profile_keys.begin(), factor_profile_keys.end());
    factor_profile_keys.erase(
        std::unique(factor_profile_keys.begin(), factor_profile_keys.end()),
        factor_profile_keys.end());
    std::vector<std::pair<int, int>> factor_group_profile_keys;
    for (const auto [pair, block] : factor_profile_keys)
    {
        const int first = FirstBit(pair);
        const int second = FirstBit(pair ^ (1 << first));
        factor_group_profile_keys.push_back({first, block});
        factor_group_profile_keys.push_back({second, block});
    }
    std::sort(factor_group_profile_keys.begin(),
              factor_group_profile_keys.end());
    factor_group_profile_keys.erase(
        std::unique(factor_group_profile_keys.begin(),
                    factor_group_profile_keys.end()),
        factor_group_profile_keys.end());
    int factor_distinct_pairs = 0;
    int factor_max_columns_per_pair = 0;
    for (size_t begin = 0; begin < factor_profile_keys.size();)
    {
        size_t end = begin + 1;
        while (end < factor_profile_keys.size() &&
               factor_profile_keys[end].first ==
                   factor_profile_keys[begin].first)
            ++end;
        ++factor_distinct_pairs;
        factor_max_columns_per_pair = std::max(
            factor_max_columns_per_pair,
            static_cast<int>(end - begin));
        begin = end;
    }
    struct CorrectionPlan
    {
        double lower = gst::fp::kInf;
        int core = 0;
        int left = 0;
        int right = 0;
    };
    std::vector<CorrectionPlan> correction_plans;
    if (core_size == 3)
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            for (int left_bits = plan.left; left_bits;
                 left_bits &= left_bits - 1)
            {
                const int from_left = left_bits & -left_bits;
                for (int right_bits = plan.right; right_bits;
                     right_bits &= right_bits - 1)
                {
                    const int from_right = right_bits & -right_bits;
                    int left = plan.left ^ from_left;
                    int right = plan.right ^ from_right;
                    if (left > right)
                        std::swap(left, right);
                    correction_plans.push_back(
                        {gst::fp::kInf,
                         plan.core | from_left | from_right, left, right});
                }
            }
        }
    std::sort(correction_plans.begin(), correction_plans.end(),
              [](const CorrectionPlan& a, const CorrectionPlan& b)
    {
        if (a.core != b.core)
            return a.core < b.core;
        if (a.left != b.left)
            return a.left < b.left;
        return a.right < b.right;
    });
    correction_plans.erase(
        std::unique(correction_plans.begin(), correction_plans.end(),
                    [](const CorrectionPlan& a, const CorrectionPlan& b)
        {
            return a.core == b.core && a.left == b.left &&
                   a.right == b.right;
        }),
        correction_plans.end());
    const long long correction_root_free_before = root_free_probes;
    const auto correction_lower_begin = Clock::now();
    for (CorrectionPlan& plan : correction_plans)
    {
        const double component = block_optimum[plan.core] +
            block_optimum[plan.left] + block_optimum[plan.right];
        const double core_left = RootFreeOptimum(plan.core | plan.left);
        const double core_right = RootFreeOptimum(plan.core | plan.right);
        plan.lower = std::max(
            component,
            std::max(
                std::max(core_left + block_optimum[plan.right],
                         core_right + block_optimum[plan.left]),
                0.5 * (core_left + core_right +
                       block_optimum[plan.left] +
                       block_optimum[plan.right])));
    }
    const long long correction_root_free_probes =
        root_free_probes - correction_root_free_before;
    const double correction_lower_ms =
        std::chrono::duration<double, std::milli>(Clock::now() -
                                                   correction_lower_begin)
            .count();
    long long correction_strong_below_optimum = 0;
    for (const CorrectionPlan& plan : correction_plans)
        if (plan.lower + gst::fp::kEps < known_optimum)
            ++correction_strong_below_optimum;
    int correction_distinct_cores = 0;
    int correction_max_plans_per_core = 0;
    for (size_t begin = 0; begin < correction_plans.size();)
    {
        size_t end = begin + 1;
        while (end < correction_plans.size() &&
               correction_plans[end].core == correction_plans[begin].core)
            ++end;
        ++correction_distinct_cores;
        correction_max_plans_per_core = std::max(
            correction_max_plans_per_core,
            static_cast<int>(end - begin));
        begin = end;
    }
    std::vector<PairCoreBudget> correction_budgets;
    for (const CorrectionPlan& plan : correction_plans)
    {
        if (plan.lower + gst::fp::kEps >= known_optimum)
            continue;
        for (int pair = plan.core; pair; pair = (pair - 1) & plan.core)
        {
            if (Bits(pair) != 2 || (pair & (1 << anchor_group)))
                continue;
            const int added = plan.core ^ pair;
            const double root_limit = known_optimum -
                block_optimum[added] - block_optimum[plan.left] -
                block_optimum[plan.right];
            correction_budgets.push_back({pair, plan.core, root_limit});
        }
    }
    std::sort(correction_budgets.begin(), correction_budgets.end(),
              [](const PairCoreBudget& a, const PairCoreBudget& b)
    {
        if (a.pair != b.pair)
            return a.pair < b.pair;
        if (a.core != b.core)
            return a.core < b.core;
        return a.root_limit > b.root_limit;
    });
    std::vector<PairCoreBudget> unique_correction_budgets;
    for (const PairCoreBudget& budget : correction_budgets)
    {
        if (!unique_correction_budgets.empty() &&
            unique_correction_budgets.back().pair == budget.pair &&
            unique_correction_budgets.back().core == budget.core)
        {
            unique_correction_budgets.back().root_limit = std::max(
                unique_correction_budgets.back().root_limit,
                budget.root_limit);
            continue;
        }
        unique_correction_budgets.push_back(budget);
    }
    long long correction_bound_core_root_scans = 0;
    for (const PairCoreBudget& budget : unique_correction_budgets)
    {
        const auto& pair_row = rows.row[budget.pair].distance;
        for (int root = 1; root <= graph.n; ++root)
            if (pair_row[root] + gst::fp::kEps < budget.root_limit)
                ++correction_bound_core_root_scans;
    }
    std::vector<int> low_masks{0};
    std::vector<int> low_index(1 << g, -1);
    low_index[0] = 0;
    for (int mask = 1; mask <= full_mask; ++mask)
        if (Bits(mask) <= q)
        {
            low_index[mask] = static_cast<int>(low_masks.size());
            low_masks.push_back(mask);
        }
    std::vector<std::vector<State>> states(1 << g);
    std::vector<int> edge_stamp(graph.m);
    std::vector<int> vertex_stamp(graph.n + 1);
    int witness_stamp = 0;
    long long factor_profile_errors = 0;
    long long factor_profile_checks = 0;
    long long factor_target_errors = 0;
    double factor_profile_checksum = 0.0;
    double factor_profile_ms = 0.0;
    double factor_target_ms = 0.0;
    long long factor_attachment_targets = 0;
    long long factor_attachment_survivors = 0;
    double factor_attachment_ms = 0.0;
    if (measure_factor_columns || measure_factor_targets)
    {
        PairProfileTargetOracle target_oracle(graph.n);
        std::vector<int> target(1);
        std::vector<double> target_value;
        std::vector<int> target_argmin;
        if (measure_factor_columns)
          for (size_t index = 0; index < factor_profile_keys.size(); ++index)
        {
            const auto [pair, block] = factor_profile_keys[index];
            const auto full_begin = Clock::now();
            const PairProfileColumn column = BuildPairProfileColumn(
                rows, groups, pair, block, graph.n);
            factor_profile_ms +=
                std::chrono::duration<double, std::milli>(Clock::now() -
                                                           full_begin)
                    .count();
            const int root = static_cast<int>(index % graph.n) + 1;
            factor_profile_checksum += column.value[root];
            target[0] = root;
            const auto target_begin = Clock::now();
            target_oracle.Evaluate(rows, groups, pair, block, target,
                                   target_value, target_argmin);
            factor_target_ms +=
                std::chrono::duration<double, std::milli>(Clock::now() -
                                                           target_begin)
                    .count();
            if (!gst::fp::Eq(target_value[0], column.value[root]) ||
                target_argmin[0] != column.argmin[root])
                ++factor_target_errors;
            if (factor_profile_checks < 2000)
            {
                const Witness witness = BuildWitness(
                    graph, groups, rows, pair, root,
                    edge_stamp, vertex_stamp, witness_stamp);
                double direct = gst::fp::kInf;
                bool argmin_found = false;
                for (int vertex : witness.vertices)
                {
                    direct = std::min(
                        direct, Value(rows, groups, block, vertex));
                    argmin_found |= vertex == column.argmin[root];
                }
                if (!gst::fp::Eq(direct, column.value[root]) ||
                    !argmin_found)
                    ++factor_profile_errors;
                ++factor_profile_checks;
            }
        }
        if (measure_factor_targets || measure_factor_columns)
        {
            const auto attachment_begin = Clock::now();
            std::vector<int> attachment_targets;
            std::vector<double> attachment_values;
            std::vector<int> attachment_argmins;
            for (const PairCoreBudget& budget : unique_pair_cores)
            {
                attachment_targets.clear();
                const auto& pair_row = rows.row[budget.pair].distance;
                for (int root = 1; root <= graph.n; ++root)
                    if (pair_row[root] + gst::fp::kEps < budget.root_limit)
                        attachment_targets.push_back(root);
                factor_attachment_targets += attachment_targets.size();
                target_oracle.Evaluate(
                    rows, groups, budget.pair, budget.core ^ budget.pair,
                    attachment_targets, attachment_values, attachment_argmins);
                for (size_t index = 0; index < attachment_targets.size(); ++index)
                    if (pair_row[attachment_targets[index]] +
                            attachment_values[index] + gst::fp::kEps <
                        budget.root_limit)
                        ++factor_attachment_survivors;
            }
            factor_attachment_ms =
                std::chrono::duration<double, std::milli>(Clock::now() -
                                                           attachment_begin)
                    .count();
        }
    }
    double factorized_family_best = gst::fp::kInf;
    int factorized_first_exact_rank = 0;
    long long factorized_unpriced_plans = 0;
    long long factorized_records = 0;
    long long factorized_tied_targets = 0;
    long long factorized_tied_records = 0;
    long long factorized_argmin_checks = 0;
    long long factorized_argmin_errors = 0;
    long long factorized_completion_probes = 0;
    double factorized_attachment_ms = 0.0;
    double factorized_completion_ms = 0.0;
    std::vector<double> factorized_plan_prices;
    if (price_factorized_plans)
    {
        if (core_size != 4)
            throw std::runtime_error(
                "factorized pricing currently requires a four-group core");
        struct FactorRecord
        {
            int pair = 0;
            int root = 0;
            int added = 0;
            int attachment_root = 0;
            double base_cost = gst::fp::kInf;
        };
        std::vector<CorePlanLower> candidates;
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            candidates.push_back(plan);
        }
        std::vector<std::vector<int>> plans_by_core(1 << g);
        for (int index = 0; index < static_cast<int>(candidates.size()); ++index)
            plans_by_core[candidates[index].core].push_back(index);
        std::vector<std::vector<FactorRecord>> records_by_core(1 << g);
        std::vector<double> priced(candidates.size(), gst::fp::kInf);
        PairProfileTargetOracle target_oracle(graph.n);
        std::vector<int> targets;
        std::vector<double> target_values;
        std::vector<int> target_argmins;
        std::vector<int> target_argmin_offsets;
        std::vector<int> target_all_argmins;
        std::vector<int> argmin_stamp(graph.n + 1);
        int argmin_generation = 0;

        const auto attachment_begin = Clock::now();
        for (const PairCoreBudget& budget : unique_pair_cores)
        {
            const int added = budget.core ^ budget.pair;
            if (Bits(added) != 2)
                continue;
            targets.clear();
            const auto& pair_row = rows.row[budget.pair].distance;
            for (int root = 1; root <= graph.n; ++root)
                if (pair_row[root] + gst::fp::kEps < budget.root_limit)
                    targets.push_back(root);
            target_oracle.Evaluate(rows, groups, budget.pair, added,
                                   targets, target_values, target_argmins,
                                   &target_argmin_offsets,
                                   &target_all_argmins);
            auto& records = records_by_core[budget.core];
            for (size_t index = 0; index < targets.size(); ++index)
            {
                const double base = pair_row[targets[index]] +
                    target_values[index];
                if (base + gst::fp::kEps >= budget.root_limit)
                    continue;
                const int argmin_begin = target_argmin_offsets[index];
                const int argmin_end = target_argmin_offsets[index + 1];
                if (price_strong_plans)
                {
                    if (++argmin_generation == 0)
                        throw std::runtime_error(
                            "factorized argmin check stamp overflow");
                    for (int argmin_index = argmin_begin;
                         argmin_index < argmin_end; ++argmin_index)
                        argmin_stamp[target_all_argmins[argmin_index]] =
                            argmin_generation;
                    const Witness pair_witness = BuildWitness(
                        graph, groups, rows, budget.pair, targets[index],
                        edge_stamp, vertex_stamp, witness_stamp);
                    int expected = 0;
                    bool mismatch = false;
                    for (int vertex : pair_witness.vertices)
                    {
                        if (!gst::fp::Eq(
                                Value(rows, groups, added, vertex),
                                target_values[index]))
                            continue;
                        ++expected;
                        mismatch |= argmin_stamp[vertex] !=
                            argmin_generation;
                    }
                    mismatch |= expected != argmin_end - argmin_begin;
                    ++factorized_argmin_checks;
                    factorized_argmin_errors += mismatch;
                }
                if (argmin_end - argmin_begin > 1)
                {
                    ++factorized_tied_targets;
                    factorized_tied_records += argmin_end - argmin_begin;
                }
                for (int argmin_index = argmin_begin;
                     argmin_index < argmin_end; ++argmin_index)
                {
                    records.push_back(
                        {budget.pair, targets[index], added,
                         target_all_argmins[argmin_index], base});
                    ++factorized_records;
                }
            }
        }
        factorized_attachment_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       attachment_begin)
                .count();

        const auto completion_begin = Clock::now();
        std::vector<int> root_targets;
        std::vector<int> added_targets;
        std::vector<double> root_values;
        std::vector<int> root_argmins;
        std::vector<double> added_values;
        std::vector<int> added_argmins;
        for (int core = 0; core <= full_mask; ++core)
        {
            auto& records = records_by_core[core];
            if (records.empty() || plans_by_core[core].empty())
                continue;
            std::sort(records.begin(), records.end(),
                      [](const FactorRecord& a, const FactorRecord& b)
            {
                if (a.pair != b.pair)
                    return a.pair < b.pair;
                return a.root < b.root;
            });
            std::vector<int> blocks;
            for (int plan_index : plans_by_core[core])
            {
                blocks.push_back(candidates[plan_index].left);
                blocks.push_back(candidates[plan_index].right);
            }
            std::sort(blocks.begin(), blocks.end());
            blocks.erase(std::unique(blocks.begin(), blocks.end()), blocks.end());
            for (size_t begin = 0; begin < records.size();)
            {
                size_t end = begin + 1;
                while (end < records.size() &&
                       records[end].pair == records[begin].pair)
                    ++end;
                const size_t count = end - begin;
                root_targets.resize(count);
                added_targets.resize(count);
                for (size_t index = 0; index < count; ++index)
                {
                    root_targets[index] = records[begin + index].root;
                    added_targets[index] =
                        records[begin + index].attachment_root;
                }
                std::vector<std::vector<double>> profiles(
                    blocks.size(), std::vector<double>(count, gst::fp::kInf));
                for (size_t block_index = 0;
                     block_index < blocks.size(); ++block_index)
                {
                    const int block = blocks[block_index];
                    target_oracle.Evaluate(
                        rows, groups, records[begin].pair, block,
                        root_targets, root_values, root_argmins);
                    target_oracle.Evaluate(
                        rows, groups, records[begin].added, block,
                        added_targets, added_values, added_argmins);
                    for (size_t index = 0; index < count; ++index)
                        profiles[block_index][index] = std::min(
                            root_values[index], added_values[index]);
                }
                for (size_t index = 0; index < count; ++index)
                    for (int plan_index : plans_by_core[core])
                    {
                        const CorePlanLower& plan = candidates[plan_index];
                        if (records[begin + index].base_cost +
                                block_optimum[plan.left] +
                                block_optimum[plan.right] +
                                gst::fp::kEps >=
                            known_optimum)
                            continue;
                        const size_t left_index = static_cast<size_t>(
                            std::lower_bound(blocks.begin(), blocks.end(),
                                             plan.left) - blocks.begin());
                        const size_t right_index = static_cast<size_t>(
                            std::lower_bound(blocks.begin(), blocks.end(),
                                             plan.right) - blocks.begin());
                        priced[plan_index] = std::min(
                            priced[plan_index],
                            records[begin + index].base_cost +
                                profiles[left_index][index] +
                                profiles[right_index][index]);
                        ++factorized_completion_probes;
                    }
                begin = end;
            }
        }
        factorized_completion_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       completion_begin)
                .count();
        for (int index = 0; index < static_cast<int>(priced.size()); ++index)
        {
            factorized_family_best = std::min(
                factorized_family_best, priced[index]);
            if (priced[index] >= gst::fp::kInf)
                ++factorized_unpriced_plans;
            if (!factorized_first_exact_rank &&
                priced[index] <= known_optimum + gst::fp::kEps)
                factorized_first_exact_rank = index + 1;
        }
        factorized_plan_prices = std::move(priced);
    }
    double rooted_core_family_best = gst::fp::kInf;
    int rooted_core_first_exact_rank = 0;
    long long rooted_core_unpriced_plans = 0;
    long long rooted_core_roots = 0;
    long long rooted_core_completion_probes = 0;
    double rooted_core_pricing_ms = 0.0;
    std::vector<double> rooted_core_plan_prices;
    if (price_rooted_core_plans)
    {
        if (core_size != 4)
            throw std::runtime_error(
                "rooted-core pricing currently requires a four-group core");
        const auto pricing_begin = Clock::now();
        std::vector<CorePlanLower> candidates;
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            candidates.push_back(plan);
        }
        std::vector<std::vector<int>> plans_by_core(1 << g);
        for (int index = 0; index < static_cast<int>(candidates.size()); ++index)
            plans_by_core[candidates[index].core].push_back(index);
        std::vector<double> priced(candidates.size(), gst::fp::kInf);
        for (int core = 0; core <= full_mask; ++core)
        {
            if (plans_by_core[core].empty())
                continue;
            std::vector<int> blocks;
            for (int plan_index : plans_by_core[core])
            {
                blocks.push_back(candidates[plan_index].left);
                blocks.push_back(candidates[plan_index].right);
            }
            std::sort(blocks.begin(), blocks.end());
            blocks.erase(std::unique(blocks.begin(), blocks.end()), blocks.end());
            std::vector<double> profiles(blocks.size(), gst::fp::kInf);
            for (int root = 1; root <= graph.n; ++root)
            {
                const double base = Value(rows, groups, core, root);
                bool viable = false;
                for (int plan_index : plans_by_core[core])
                {
                    const CorePlanLower& plan = candidates[plan_index];
                    viable |= base + block_optimum[plan.left] +
                            block_optimum[plan.right] + gst::fp::kEps <
                        known_optimum;
                }
                if (!viable)
                    continue;
                const Witness witness = BuildWitness(
                    graph, groups, rows, core, root,
                    edge_stamp, vertex_stamp, witness_stamp);
                std::fill(profiles.begin(), profiles.end(), gst::fp::kInf);
                for (int vertex : witness.vertices)
                    for (size_t block_index = 0;
                         block_index < blocks.size(); ++block_index)
                        profiles[block_index] = std::min(
                            profiles[block_index],
                            Value(rows, groups, blocks[block_index], vertex));
                ++rooted_core_roots;
                for (int plan_index : plans_by_core[core])
                {
                    const CorePlanLower& plan = candidates[plan_index];
                    if (base + block_optimum[plan.left] +
                            block_optimum[plan.right] + gst::fp::kEps >=
                        known_optimum)
                        continue;
                    const size_t left_index = static_cast<size_t>(
                        std::lower_bound(blocks.begin(), blocks.end(),
                                         plan.left) - blocks.begin());
                    const size_t right_index = static_cast<size_t>(
                        std::lower_bound(blocks.begin(), blocks.end(),
                                         plan.right) - blocks.begin());
                    priced[plan_index] = std::min(
                        priced[plan_index],
                        base + profiles[left_index] + profiles[right_index]);
                    ++rooted_core_completion_probes;
                }
            }
        }
        for (int index = 0; index < static_cast<int>(priced.size()); ++index)
        {
            rooted_core_family_best = std::min(
                rooted_core_family_best, priced[index]);
            if (priced[index] >= gst::fp::kInf)
                ++rooted_core_unpriced_plans;
            if (!rooted_core_first_exact_rank &&
                priced[index] <= known_optimum + gst::fp::kEps)
                rooted_core_first_exact_rank = index + 1;
        }
        rooted_core_plan_prices = std::move(priced);
        rooted_core_pricing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       pricing_begin)
                .count();
    }
    long long rooted_tour_exact_plans = 0;
    long long rooted_tour_unresolved_plans = 0;
    if (price_rooted_core_plans && measure_macro_tour_lower)
    {
        if (rooted_core_plan_prices.size() != macro_tour_plan_lowers.size())
            throw std::runtime_error(
                "rooted and macro-tour plan lists disagree");
        for (size_t index = 0; index < rooted_core_plan_prices.size(); ++index)
        {
            if (gst::fp::Eq(rooted_core_plan_prices[index],
                            macro_tour_plan_lowers[index]))
                ++rooted_tour_exact_plans;
            else
                ++rooted_tour_unresolved_plans;
        }
    }
    double optimal_core_family_best = gst::fp::kInf;
    int optimal_core_first_exact_rank = 0;
    long long optimal_core_calls = 0;
    long long optimal_core_join_transitions = 0;
    long long optimal_core_edge_transitions = 0;
    double optimal_core_pricing_ms = 0.0;
    if (price_optimal_core_plans)
    {
        if (core_size != 4)
            throw std::runtime_error(
                "optimal-core pricing currently requires a four-group core");
        const auto pricing_begin = Clock::now();
        int rank = 0;
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            ++rank;
            const OptimalWitnessProfiles profiles =
                BuildOptimalWitnessProfiles(
                    graph, groups, rows, plan.core,
                    plan.left, plan.right);
            double priced = gst::fp::kInf;
            for (int root = 1; root <= graph.n; ++root)
                priced = std::min(
                    priced,
                    Value(rows, groups, plan.core, root) +
                        profiles.root[root].sum);
            optimal_core_family_best = std::min(
                optimal_core_family_best, priced);
            if (!optimal_core_first_exact_rank &&
                priced <= known_optimum + gst::fp::kEps)
                optimal_core_first_exact_rank = rank;
            ++optimal_core_calls;
            optimal_core_join_transitions += profiles.join_transitions;
            optimal_core_edge_transitions += profiles.edge_transitions;
        }
        optimal_core_pricing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       pricing_begin)
                .count();
    }
    double macro_core_family_best = gst::fp::kInf;
    int macro_core_first_exact_rank = 0;
    long long macro_core_calls = 0;
    long long macro_core_join_probes = 0;
    long long macro_core_queue_pops = 0;
    double macro_core_pricing_ms = 0.0;
    if (price_macro_core_plans)
    {
        if (core_size != 4)
            throw std::runtime_error(
                "macro-core pricing currently requires a four-group core");
        const auto pricing_begin = Clock::now();
        int rank = 0;
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            ++rank;
            const MacroPlanResult priced = PriceMacroPlan(
                graph, groups, rows,
                plan.core, plan.left, plan.right);
            macro_core_family_best = std::min(
                macro_core_family_best, priced.value);
            if (!macro_core_first_exact_rank &&
                priced.value <= known_optimum + gst::fp::kEps)
                macro_core_first_exact_rank = rank;
            ++macro_core_calls;
            macro_core_join_probes += priced.join_probes;
            macro_core_queue_pops += priced.queue_pops;
        }
        macro_core_pricing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       pricing_begin)
                .count();
    }
    double contracted_core_separate_best = gst::fp::kInf;
    double contracted_core_family_best = gst::fp::kInf;
    int contracted_core_first_exact_rank = 0;
    long long contracted_core_calls = 0;
    long long contracted_core_roots = 0;
    long long contracted_core_join_probes = 0;
    long long contracted_core_queue_pops = 0;
    double contracted_core_pricing_ms = 0.0;
    if (price_contracted_core_plans)
    {
        if (core_size != 4)
            throw std::runtime_error(
                "contracted-core pricing currently requires a four-group core");
        const auto pricing_begin = Clock::now();
        int rank = 0;
        std::vector<int> paid_edge_stamp(graph.m, 0);
        int paid_generation = 0;
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            ++rank;
            double separate_priced = gst::fp::kInf;
            double priced = gst::fp::kInf;
            for (int root = 1; root <= graph.n; ++root)
            {
                const double base = Value(rows, groups, plan.core, root);
                if (base + block_optimum[plan.left] +
                        block_optimum[plan.right] + gst::fp::kEps >=
                    known_optimum)
                    continue;
                const Witness witness = BuildWitness(
                    graph, groups, rows, plan.core, root,
                    edge_stamp, vertex_stamp, witness_stamp);
                const ContractedBlockResult left = PriceContractedBlock(
                    graph, groups, plan.left, witness,
                    paid_edge_stamp, paid_generation);
                const ContractedBlockResult right = PriceContractedBlock(
                    graph, groups, plan.right, witness,
                    paid_edge_stamp, paid_generation);
                const ContractedBlockResult joint = PriceContractedPair(
                    graph, left, right, witness,
                    paid_edge_stamp, paid_generation);
                separate_priced = std::min(
                    separate_priced, base + left.value + right.value);
                priced = std::min(
                    priced, base + joint.value);
                ++contracted_core_roots;
                contracted_core_join_probes +=
                    left.join_probes + right.join_probes + joint.join_probes;
                contracted_core_queue_pops +=
                    left.queue_pops + right.queue_pops + joint.queue_pops;
            }
            contracted_core_separate_best = std::min(
                contracted_core_separate_best, separate_priced);
            contracted_core_family_best = std::min(
                contracted_core_family_best, priced);
            if (!contracted_core_first_exact_rank &&
                priced <= known_optimum + gst::fp::kEps)
                contracted_core_first_exact_rank = rank;
            ++contracted_core_calls;
        }
        contracted_core_pricing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       pricing_begin)
                .count();
    }
    double pair_union_core_family_best = gst::fp::kInf;
    int pair_union_core_first_exact_rank = 0;
    long long pair_union_core_calls = 0;
    long long pair_union_core_candidates = 0;
    long long pair_union_core_profile_probes = 0;
    double pair_union_core_pricing_ms = 0.0;
    if (price_pair_union_core_plans)
    {
        if (core_size != 4)
            throw std::runtime_error(
                "pair-union pricing currently requires a four-group core");
        const auto pricing_begin = Clock::now();
        int rank = 0;
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            ++rank;
            double priced = gst::fp::kInf;
            const int pivot = FirstBit(plan.core);
            const int branch_domain = plan.core ^ (1 << pivot);
            for (int tail = branch_domain; tail;
                 tail = (tail - 1) & branch_domain)
            {
                const int first_pair = tail | (1 << pivot);
                if (Bits(first_pair) != 2)
                    continue;
                const int second_pair = plan.core ^ first_pair;
                for (int root = 1; root <= graph.n; ++root)
                {
                    const Witness first = BuildWitness(
                        graph, groups, rows, first_pair, root,
                        edge_stamp, vertex_stamp, witness_stamp);
                    const Witness second = BuildWitness(
                        graph, groups, rows, second_pair, root,
                        edge_stamp, vertex_stamp, witness_stamp);
                    Witness core;
                    std::set_union(
                        first.edges.begin(), first.edges.end(),
                        second.edges.begin(), second.edges.end(),
                        std::back_inserter(core.edges));
                    std::set_union(
                        first.vertices.begin(), first.vertices.end(),
                        second.vertices.begin(), second.vertices.end(),
                        std::back_inserter(core.vertices));
                    const double cost = EdgeCost(graph, core.edges);
                    if (cost + gst::fp::kEps >= known_optimum)
                        continue;
                    double left = gst::fp::kInf;
                    double right = gst::fp::kInf;
                    for (int vertex : core.vertices)
                    {
                        left = std::min(
                            left, Value(rows, groups, plan.left, vertex));
                        right = std::min(
                            right, Value(rows, groups, plan.right, vertex));
                        pair_union_core_profile_probes += 2;
                    }
                    priced = std::min(priced, cost + left + right);
                    ++pair_union_core_candidates;
                }
            }
            pair_union_core_family_best = std::min(
                pair_union_core_family_best, priced);
            if (!pair_union_core_first_exact_rank &&
                priced <= known_optimum + gst::fp::kEps)
                pair_union_core_first_exact_rank = rank;
            ++pair_union_core_calls;
        }
        pair_union_core_pricing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       pricing_begin)
                .count();
    }
    double all_attachment_core_family_best = gst::fp::kInf;
    int all_attachment_core_first_exact_rank = 0;
    long long all_attachment_core_calls = 0;
    long long all_attachment_core_candidates = 0;
    long long all_attachment_core_profile_probes = 0;
    double all_attachment_core_pricing_ms = 0.0;
    if (price_all_attachment_core_plans)
    {
        if (core_size != 4)
            throw std::runtime_error(
                "all-attachment pricing currently requires a four-group core");
        const auto pricing_begin = Clock::now();
        int rank = 0;
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            ++rank;
            double priced = gst::fp::kInf;
            for (int first_pair = plan.core; first_pair;
                 first_pair = (first_pair - 1) & plan.core)
            {
                if (Bits(first_pair) != 2)
                    continue;
                const int second_pair = plan.core ^ first_pair;
                for (int root = 1; root <= graph.n; ++root)
                {
                    const Witness first = BuildWitness(
                        graph, groups, rows, first_pair, root,
                        edge_stamp, vertex_stamp, witness_stamp);
                    for (int attachment : first.vertices)
                    {
                        const Witness second = BuildWitness(
                            graph, groups, rows, second_pair, attachment,
                            edge_stamp, vertex_stamp, witness_stamp);
                        Witness core;
                        std::set_union(
                            first.edges.begin(), first.edges.end(),
                            second.edges.begin(), second.edges.end(),
                            std::back_inserter(core.edges));
                        std::set_union(
                            first.vertices.begin(), first.vertices.end(),
                            second.vertices.begin(), second.vertices.end(),
                            std::back_inserter(core.vertices));
                        const double cost = EdgeCost(graph, core.edges);
                        if (cost + gst::fp::kEps >= known_optimum)
                            continue;
                        double left = gst::fp::kInf;
                        double right = gst::fp::kInf;
                        for (int vertex : core.vertices)
                        {
                            left = std::min(
                                left,
                                Value(rows, groups, plan.left, vertex));
                            right = std::min(
                                right,
                                Value(rows, groups, plan.right, vertex));
                            all_attachment_core_profile_probes += 2;
                        }
                        priced = std::min(priced, cost + left + right);
                        ++all_attachment_core_candidates;
                    }
                }
            }
            all_attachment_core_family_best = std::min(
                all_attachment_core_family_best, priced);
            if (!all_attachment_core_first_exact_rank &&
                priced <= known_optimum + gst::fp::kEps)
                all_attachment_core_first_exact_rank = rank;
            ++all_attachment_core_calls;
        }
        all_attachment_core_pricing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       pricing_begin)
                .count();
    }
    double oracle_tree_core_family_best = gst::fp::kInf;
    double oracle_tree_strong_best = gst::fp::kInf;
    double oracle_tree_core_contracted_best = gst::fp::kInf;
    double oracle_tree_cost = gst::fp::kInf;
    int oracle_tree_cycle_rank = -1;
    int oracle_tree_first_exact_rank = 0;
    int oracle_tree_best_core = 0;
    int oracle_tree_best_left = 0;
    int oracle_tree_best_right = 0;
    long long oracle_tree_core_calls = 0;
    long long oracle_tree_core_edges = 0;
    double oracle_tree_core_pricing_ms = 0.0;
    if (price_oracle_tree_core_plans)
    {
        if (core_size != 4)
            throw std::runtime_error(
                "oracle-tree pricing currently requires a four-group core");
        const auto pricing_begin = Clock::now();
        Witness full;
        std::vector<int> selected_terminal(g, -1);
        for (int block : {three_block.first,
                          three_block.second,
                          three_block.third})
        {
            std::vector<int> local_terminal;
            const Witness part = BuildWitness(
                graph, groups, rows, block, three_block.root,
                edge_stamp, vertex_stamp, witness_stamp, &local_terminal);
            Witness merged;
            std::set_union(
                full.edges.begin(), full.edges.end(),
                part.edges.begin(), part.edges.end(),
                std::back_inserter(merged.edges));
            std::set_union(
                full.vertices.begin(), full.vertices.end(),
                part.vertices.begin(), part.vertices.end(),
                std::back_inserter(merged.vertices));
            full = std::move(merged);
            for (int group = 0; group < g; ++group)
                if (local_terminal[group] >= 0)
                    selected_terminal[group] = local_terminal[group];
        }
        oracle_tree_cost = EdgeCost(graph, full.edges);
        oracle_tree_cycle_rank = static_cast<int>(full.edges.size()) -
            static_cast<int>(full.vertices.size()) + 1;

        std::vector<std::vector<std::pair<int, int>>> tree(graph.n + 1);
        std::vector<int> full_degree(graph.n + 1, 0);
        for (int index = 0; index < static_cast<int>(full.edges.size());
             ++index)
        {
            const gst::UndirectedEdge& edge = graph.edges[full.edges[index]];
            tree[edge.u].push_back({edge.v, index});
            tree[edge.v].push_back({edge.u, index});
            ++full_degree[edge.u];
            ++full_degree[edge.v];
        }
        std::vector<Witness> core_cache(1 << g);
        std::vector<unsigned char> core_ready(1 << g, 0);
        auto ExtractCore = [&](int core_mask) -> const Witness&
        {
            if (core_ready[core_mask])
                return core_cache[core_mask];
            std::vector<int> degree = full_degree;
            std::vector<unsigned char> required(graph.n + 1, 0);
            for (int bits = core_mask; bits; bits &= bits - 1)
            {
                const int terminal = selected_terminal[FirstBit(bits)];
                if (terminal < 0)
                    throw std::runtime_error(
                        "exact witness did not expose every group terminal");
                required[terminal] = 1;
            }
            std::vector<unsigned char> active(full.edges.size(), 1);
            std::queue<int> leaves;
            for (int vertex : full.vertices)
                if (degree[vertex] <= 1 && !required[vertex])
                    leaves.push(vertex);
            while (!leaves.empty())
            {
                const int vertex = leaves.front();
                leaves.pop();
                if (degree[vertex] > 1 || required[vertex])
                    continue;
                for (const auto [next, index] : tree[vertex])
                {
                    if (!active[index])
                        continue;
                    active[index] = 0;
                    --degree[vertex];
                    --degree[next];
                    if (degree[next] <= 1 && !required[next])
                        leaves.push(next);
                }
            }
            Witness& core = core_cache[core_mask];
            for (int index = 0; index < static_cast<int>(full.edges.size());
                 ++index)
                if (active[index])
                    core.edges.push_back(full.edges[index]);
            for (int vertex : full.vertices)
                if (degree[vertex] > 0 || required[vertex])
                    core.vertices.push_back(vertex);
            core_ready[core_mask] = 1;
            return core;
        };

        int component_rank = 0;
        for (const CorePlanLower& plan : component_plans)
        {
            ++component_rank;
            const Witness& core = ExtractCore(plan.core);
            const double cost = EdgeCost(graph, core.edges);
            double left = gst::fp::kInf;
            double right = gst::fp::kInf;
            for (int vertex : core.vertices)
            {
                left = std::min(
                    left, Value(rows, groups, plan.left, vertex));
                right = std::min(
                    right, Value(rows, groups, plan.right, vertex));
            }
            const double priced = cost + left + right;
            if (priced < oracle_tree_core_family_best)
            {
                oracle_tree_core_family_best = priced;
                oracle_tree_best_core = plan.core;
                oracle_tree_best_left = plan.left;
                oracle_tree_best_right = plan.right;
            }
            if (!oracle_tree_first_exact_rank &&
                priced <= known_optimum + gst::fp::kEps)
                oracle_tree_first_exact_rank = component_rank;
        }

        std::vector<int> paid_edge_stamp(graph.m, 0);
        int paid_generation = 0;
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            const Witness& core = ExtractCore(plan.core);
            const double cost = EdgeCost(graph, core.edges);
            double left = gst::fp::kInf;
            double right = gst::fp::kInf;
            for (int vertex : core.vertices)
            {
                left = std::min(
                    left, Value(rows, groups, plan.left, vertex));
                right = std::min(
                    right, Value(rows, groups, plan.right, vertex));
            }
            oracle_tree_strong_best = std::min(
                oracle_tree_strong_best, cost + left + right);
            const ContractedBlockResult contracted_left =
                PriceContractedBlock(
                    graph, groups, plan.left, core,
                    paid_edge_stamp, paid_generation);
            const ContractedBlockResult contracted_right =
                PriceContractedBlock(
                    graph, groups, plan.right, core,
                    paid_edge_stamp, paid_generation);
            const ContractedBlockResult contracted_joint =
                PriceContractedPair(
                    graph, contracted_left, contracted_right, core,
                    paid_edge_stamp, paid_generation);
            oracle_tree_core_contracted_best = std::min(
                oracle_tree_core_contracted_best,
                cost + contracted_joint.value);
            oracle_tree_core_edges += core.edges.size();
            ++oracle_tree_core_calls;
        }
        oracle_tree_core_pricing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       pricing_begin)
                .count();
    }
    double strong_family_best = gst::fp::kInf;
    int strong_family_first_exact_rank = 0;
    long long strong_family_unpriced_plans = 0;
    long long strong_family_pair_roots = 0;
    long long strong_family_core_roots = 0;
    long long strong_family_core_candidates = 0;
    long long strong_family_completion_probes = 0;
    double strong_family_pricing_ms = 0.0;
    std::vector<double> strong_plan_prices;
    if (price_strong_plans)
    {
        const auto pricing_begin = Clock::now();
        std::vector<CorePlanLower> candidates;
        for (const CorePlanLower& plan : strong_plans)
        {
            if (plan.lower + gst::fp::kEps >= known_optimum)
                break;
            candidates.push_back(plan);
        }
        std::vector<std::vector<int>> plans_by_core(1 << g);
        for (int index = 0; index < static_cast<int>(candidates.size()); ++index)
            plans_by_core[candidates[index].core].push_back(index);
        std::vector<double> priced(candidates.size(), gst::fp::kInf);

        for (size_t pair_begin = 0; pair_begin < unique_pair_cores.size();)
        {
            size_t pair_end = pair_begin + 1;
            while (pair_end < unique_pair_cores.size() &&
                   unique_pair_cores[pair_end].pair ==
                       unique_pair_cores[pair_begin].pair)
                ++pair_end;
            const int pair = unique_pair_cores[pair_begin].pair;
            double pair_root_limit = -gst::fp::kInf;
            for (size_t index = pair_begin; index < pair_end; ++index)
                pair_root_limit = std::max(
                    pair_root_limit, unique_pair_cores[index].root_limit);
            for (int pair_root = 1; pair_root <= graph.n; ++pair_root)
            {
                const double pair_distance = rows.row[pair].distance[pair_root];
                if (pair_distance + gst::fp::kEps >= pair_root_limit)
                    continue;
                ++strong_family_pair_roots;
                Witness pair_witness = BuildWitness(
                    graph, groups, rows, pair, pair_root,
                    edge_stamp, vertex_stamp, witness_stamp);

                for (size_t budget_index = pair_begin;
                     budget_index < pair_end; ++budget_index)
                {
                    const PairCoreBudget& budget =
                        unique_pair_cores[budget_index];
                    if (pair_distance + gst::fp::kEps >= budget.root_limit)
                        continue;
                    ++strong_family_core_roots;
                    const int added = budget.core ^ pair;
                    std::vector<int> attachment_roots;
                    double attachment = 0.0;
                    if (!added)
                        attachment_roots.push_back(0);
                    else
                    {
                        attachment = gst::fp::kInf;
                        for (int vertex : pair_witness.vertices)
                            attachment = std::min(
                                attachment,
                                Value(rows, groups, added, vertex));
                        for (int vertex : pair_witness.vertices)
                            if (!gst::fp::Eq(
                                    Value(rows, groups, added, vertex),
                                    attachment))
                                continue;
                            else
                                attachment_roots.push_back(vertex);
                    }
                    if (pair_distance + attachment + gst::fp::kEps >=
                        budget.root_limit)
                        continue;
                    strong_family_core_candidates += attachment_roots.size();
                    for (int attachment_root : attachment_roots)
                    {
                        std::vector<int> core_vertices = pair_witness.vertices;
                        if (attachment_root)
                        {
                            if (Bits(added) == 1)
                            {
                                const int singleton = FirstBit(added);
                                int vertex = attachment_root;
                                while (groups.parent[singleton][vertex] > 0)
                                {
                                    vertex = groups.parent[singleton][vertex];
                                    core_vertices.push_back(vertex);
                                }
                            }
                            else
                            {
                                const Witness attachment_witness = BuildWitness(
                                    graph, groups, rows, added, attachment_root,
                                    edge_stamp, vertex_stamp, witness_stamp);
                                core_vertices.insert(
                                    core_vertices.end(),
                                    attachment_witness.vertices.begin(),
                                    attachment_witness.vertices.end());
                            }
                        }
                        for (int plan_index : plans_by_core[budget.core])
                        {
                            const CorePlanLower& plan = candidates[plan_index];
                            if (pair_distance + attachment +
                                    block_optimum[plan.left] +
                                    block_optimum[plan.right] +
                                    gst::fp::kEps >=
                                known_optimum)
                                continue;
                            double left_value = gst::fp::kInf;
                            double right_value = gst::fp::kInf;
                            for (int vertex : core_vertices)
                            {
                                left_value = std::min(
                                    left_value,
                                    Value(rows, groups, plan.left, vertex));
                                right_value = std::min(
                                    right_value,
                                    Value(rows, groups, plan.right, vertex));
                            }
                            priced[plan_index] = std::min(
                                priced[plan_index],
                                pair_distance + attachment +
                                    left_value + right_value);
                            ++strong_family_completion_probes;
                        }
                    }
                }
            }
            pair_begin = pair_end;
        }
        for (int index = 0; index < static_cast<int>(priced.size()); ++index)
        {
            strong_family_best = std::min(strong_family_best, priced[index]);
            if (priced[index] >= gst::fp::kInf)
                ++strong_family_unpriced_plans;
            if (!strong_family_first_exact_rank &&
                priced[index] <= known_optimum + gst::fp::kEps)
                strong_family_first_exact_rank = index + 1;
        }
        strong_family_pricing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       pricing_begin)
                .count();
        strong_plan_prices = std::move(priced);
    }
    long long factorized_price_compared = 0;
    long long factorized_price_mismatches = 0;
    long long factorized_price_lower = 0;
    long long factorized_price_higher = 0;
    double factorized_price_max_gap = 0.0;
    if (!factorized_plan_prices.empty() && !strong_plan_prices.empty())
    {
        if (factorized_plan_prices.size() != strong_plan_prices.size())
            throw std::runtime_error("factorized/explicit plan count mismatch");
        factorized_price_compared = factorized_plan_prices.size();
        for (size_t index = 0; index < factorized_plan_prices.size(); ++index)
        {
            const double factorized = factorized_plan_prices[index];
            const double explicit_price = strong_plan_prices[index];
            if (gst::fp::Eq(factorized, explicit_price))
                continue;
            ++factorized_price_mismatches;
            if (factorized < explicit_price)
                ++factorized_price_lower;
            else
                ++factorized_price_higher;
            factorized_price_max_gap = std::max(
                factorized_price_max_gap,
                std::abs(factorized - explicit_price));
        }
    }
    double three_block_priced = gst::fp::kInf;
    long long three_block_priced_candidates = 0;
    double three_block_pricing_ms = 0.0;
    if (price_best_plan && three_block.first)
    {
        const auto pricing_begin = Clock::now();
        int priced_core = three_block.first;
        int priced_left = three_block.second;
        int priced_right = three_block.third;
        if (Bits(priced_left) < Bits(priced_core))
        {
            std::swap(priced_core, priced_left);
        }
        if (Bits(priced_right) < Bits(priced_core))
        {
            std::swap(priced_core, priced_right);
        }
        for (int core_root = 1; core_root <= graph.n; ++core_root)
        {
            const Witness witness = BuildWitness(
                graph, groups, rows, priced_core, core_root,
                edge_stamp, vertex_stamp, witness_stamp);
            double left_value = gst::fp::kInf;
            double right_value = gst::fp::kInf;
            for (int vertex : witness.vertices)
            {
                left_value = std::min(
                    left_value, Value(rows, groups, priced_left, vertex));
                right_value = std::min(
                    right_value, Value(rows, groups, priced_right, vertex));
            }
            three_block_priced = std::min(
                three_block_priced,
                EdgeCost(graph, witness.edges) + left_value + right_value);
            ++three_block_priced_candidates;
        }
        three_block_pricing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       pricing_begin)
                .count();
    }
    long long raw_seed_states = 0;
    for (int first = 0; first < g; ++first)
    {
        if (first == anchor_group)
            continue;
        for (int second = first + 1; second < g; ++second)
        {
            if (second == anchor_group)
                continue;
            const int pair = (1 << first) | (1 << second);
            std::vector<int> roots(graph.n);
            std::iota(roots.begin(), roots.end(), 1);
            std::sort(roots.begin(), roots.end(), [&](int a, int b)
            {
                if (rows.row[pair].distance[a] != rows.row[pair].distance[b])
                    return rows.row[pair].distance[a] < rows.row[pair].distance[b];
                if (groups.distance[anchor_group][a] !=
                    groups.distance[anchor_group][b])
                    return groups.distance[anchor_group][a] <
                           groups.distance[anchor_group][b];
                return a < b;
            });
            double best_anchor = gst::fp::kInf;
            for (int pair_root : roots)
            {
                if (!profile_seeds)
                {
                    if (groups.distance[anchor_group][pair_root] + gst::fp::kEps >=
                        best_anchor)
                        continue;
                    best_anchor = groups.distance[anchor_group][pair_root];
                }
                Witness witness = BuildWitness(
                    graph, groups, rows, pair, pair_root,
                    edge_stamp, vertex_stamp, witness_stamp);
                State state;
                state.declared = pair;
                state.origin_pair = pair;
                state.origin_root = pair_root;
                state.edges = std::move(witness.edges);
                state.vertices = std::move(witness.vertices);
                state.cost = EdgeCost(graph, state.edges);
                states[pair].push_back(std::move(state));
                ++raw_seed_states;
            }
        }
    }
    double priced_completion = gst::fp::kInf;
    long long priced_candidates = 0;
    double pricing_ms = 0.0;
    long long generated = 0;
    long long seed_states = 0;
    long long maximum_front = 0;
    const int groups_to_grow = core_size - 2;
    const int growth_count = (groups_to_grow + q - 1) / q;
    std::vector<int> growth_sizes;
    if (growth_count)
    {
        const int base = groups_to_grow / growth_count;
        const int extra = groups_to_grow % growth_count;
        for (int index = 0; index < growth_count; ++index)
            growth_sizes.push_back(base + (index < extra ? 1 : 0));
    }
    for (int size = 2; size <= core_size; ++size)
    {
        for (int mask = 1; mask <= full_mask; ++mask)
        {
            if (Bits(mask) != size || states[mask].empty())
                continue;
            Prune(states[mask], rows, groups, g, low_masks);
            if (size == 2)
                seed_states += states[mask].size();
            maximum_front =
                std::max(maximum_front, static_cast<long long>(states[mask].size()));
            if (size == core_size)
                continue;
            const int available = full_mask ^ mask;
            int grown = size - 2;
            int growth_index = 0;
            while (growth_index < static_cast<int>(growth_sizes.size()) && grown > 0)
            {
                grown -= growth_sizes[growth_index];
                ++growth_index;
            }
            if (grown != 0 || growth_index >= static_cast<int>(growth_sizes.size()))
                continue;
            const int next_block_size = growth_sizes[growth_index];
            for (const State& state : states[mask])
                for (int block = available; block; block = (block - 1) & available)
                {
                    const int block_size = Bits(block);
                    if (block_size != next_block_size)
                        continue;
                    std::vector<State> local;
                    double best_attachment = gst::fp::kInf;
                    for (int vertex : state.vertices)
                        best_attachment = std::min(
                            best_attachment, Value(rows, groups, block, vertex));
                    for (int vertex : state.vertices)
                    {
                        if (Value(rows, groups, block, vertex) >
                            best_attachment + gst::fp::kEps)
                            continue;
                        const Witness witness = BuildWitness(
                            graph, groups, rows, block, vertex,
                            edge_stamp, vertex_stamp, witness_stamp);
                        local.push_back(
                            Unite(graph, state, witness, mask | block));
                        local.back().growth_blocks.push_back(block);
                        ++generated;
                    }
                    std::sort(local.begin(), local.end(), [](const State& a, const State& b)
                    {
                        return a.edges < b.edges;
                    });
                    local.erase(std::unique(
                        local.begin(), local.end(), [](const State& a, const State& b)
                        {
                            return a.edges == b.edges;
                        }), local.end());
                    for (State& candidate : local)
                        states[mask | block].push_back(std::move(candidate));
                }
        }
    }

    double completion = gst::fp::kInf;
    int best_core_mask = 0;
    int best_left = 0;
    int best_right = 0;
    int best_origin_pair = 0;
    int best_origin_root = 0;
    std::vector<int> best_growth_blocks;
    long long core_states = 0;
    for (int mask = 1; mask <= full_mask; ++mask)
    {
        if (Bits(mask) != core_size || states[mask].empty())
            continue;
        Prune(states[mask], rows, groups, g, low_masks);
        core_states += states[mask].size();
        const int remaining = full_mask ^ mask;
        for (const State& state : states[mask])
            for (int left = remaining;; left = (left - 1) & remaining)
            {
                const int right = remaining ^ left;
                if (left <= right && Bits(left) <= q && Bits(right) <= q)
                {
                    const double candidate =
                        state.cost + state.profile[low_index[left]] +
                        state.profile[low_index[right]];
                    if (candidate < completion)
                    {
                        completion = candidate;
                        best_core_mask = mask;
                        best_left = left;
                        best_right = right;
                        best_origin_pair = state.origin_pair;
                        best_origin_root = state.origin_root;
                        best_growth_blocks = state.growth_blocks;
                    }
                }
                if (!left)
                    break;
            }
    }
    int best_component_rank = 0;
    int best_strong_rank = 0;
    for (size_t index = 0; index < component_plans.size(); ++index)
    {
        const CorePlanLower& plan = component_plans[index];
        if (plan.core == best_core_mask &&
            ((plan.left == best_left && plan.right == best_right) ||
             (plan.left == best_right && plan.right == best_left)))
        {
            best_component_rank = static_cast<int>(index) + 1;
            break;
        }
    }
    for (size_t index = 0; index < strong_plans.size(); ++index)
    {
        const CorePlanLower& plan = strong_plans[index];
        if (plan.core == best_core_mask &&
            ((plan.left == best_left && plan.right == best_right) ||
             (plan.left == best_right && plan.right == best_left)))
        {
            best_strong_rank = static_cast<int>(index) + 1;
            break;
        }
    }
    if (price_best_plan && best_core_mask)
    {
        const auto pricing_begin = Clock::now();
        auto GrowBlock = [&](const State& state, int block)
        {
            std::vector<State> result;
            double best_attachment = gst::fp::kInf;
            for (int vertex : state.vertices)
                best_attachment = std::min(
                    best_attachment, Value(rows, groups, block, vertex));
            for (int vertex : state.vertices)
            {
                if (Value(rows, groups, block, vertex) >
                    best_attachment + gst::fp::kEps)
                    continue;
                const Witness witness = BuildWitness(
                    graph, groups, rows, block, vertex,
                    edge_stamp, vertex_stamp, witness_stamp);
                result.push_back(
                    Unite(graph, state, witness, state.declared | block));
            }
            std::sort(result.begin(), result.end(), [](const State& a, const State& b)
            {
                return a.edges < b.edges;
            });
            result.erase(std::unique(
                result.begin(), result.end(), [](const State& a, const State& b)
                {
                    return a.edges == b.edges;
                }), result.end());
            return result;
        };
        for (int pair = best_core_mask; pair; pair = (pair - 1) & best_core_mask)
        {
            if (Bits(pair) != 2 || (pair & (1 << anchor_group)))
                continue;
            const int added = best_core_mask ^ pair;
            if (!added)
            {
                for (int pair_root = 1; pair_root <= graph.n; ++pair_root)
                {
                    const Witness witness = BuildWitness(
                        graph, groups, rows, pair, pair_root,
                        edge_stamp, vertex_stamp, witness_stamp);
                    double left_value = gst::fp::kInf;
                    double right_value = gst::fp::kInf;
                    for (int vertex : witness.vertices)
                    {
                        left_value = std::min(
                            left_value,
                            Value(rows, groups, best_left, vertex));
                        right_value = std::min(
                            right_value,
                            Value(rows, groups, best_right, vertex));
                    }
                    priced_completion = std::min(
                        priced_completion,
                        EdgeCost(graph, witness.edges) +
                            left_value + right_value);
                    ++priced_candidates;
                }
                continue;
            }
            const int first_size = std::min(q, Bits(added));
            for (int first_block = added; first_block;
                 first_block = (first_block - 1) & added)
            {
                if (Bits(first_block) != first_size)
                    continue;
                const int second_block = added ^ first_block;
                for (int pair_root = 1; pair_root <= graph.n; ++pair_root)
                {
                    Witness pair_witness = BuildWitness(
                        graph, groups, rows, pair, pair_root,
                        edge_stamp, vertex_stamp, witness_stamp);
                    State seed;
                    seed.declared = pair;
                    seed.edges = std::move(pair_witness.edges);
                    seed.vertices = std::move(pair_witness.vertices);
                    seed.cost = EdgeCost(graph, seed.edges);
                    std::vector<State> first_states = GrowBlock(seed, first_block);
                    for (const State& first_state : first_states)
                    {
                        std::vector<State> core_candidates = second_block
                            ? GrowBlock(first_state, second_block)
                            : std::vector<State>{first_state};
                        for (State& candidate : core_candidates)
                        {
                            double left_value = gst::fp::kInf;
                            double right_value = gst::fp::kInf;
                            for (int vertex : candidate.vertices)
                            {
                                left_value = std::min(
                                    left_value,
                                    Value(rows, groups, best_left, vertex));
                                right_value = std::min(
                                    right_value,
                                    Value(rows, groups, best_right, vertex));
                            }
                            priced_completion = std::min(
                                priced_completion,
                                candidate.cost + left_value + right_value);
                            ++priced_candidates;
                        }
                    }
                }
            }
        }
        pricing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - pricing_begin)
                .count();
    }
    double half_priced_completion = gst::fp::kInf;
    long long half_priced_candidates = 0;
    double half_pricing_ms = 0.0;
    if (price_best_plan && best_growth_blocks.size() == 2)
    {
        const auto half_pricing_begin = Clock::now();
        std::vector<std::pair<int, int>> half_partitions;
        const int growth_first = best_growth_blocks[0];
        const int growth_second = best_growth_blocks[1];
        const int candidates[2][2] = {
            {growth_first | best_left, growth_second | best_right},
            {growth_first | best_right, growth_second | best_left}};
        for (const auto& partition : candidates)
        {
            const int left = partition[0];
            const int right = partition[1];
            if (!(left & right) &&
                (left | right | best_origin_pair) == full_mask &&
                Bits(left) <= half && Bits(right) <= half)
                half_partitions.push_back({left, right});
        }
        std::sort(half_partitions.begin(), half_partitions.end());
        half_partitions.erase(
            std::unique(half_partitions.begin(), half_partitions.end()),
            half_partitions.end());
        std::vector<std::vector<double>> priced_rows(1 << g);
        std::vector<char> priced_ready(1 << g);
        auto GetPricedRow = [&](int mask) -> const std::vector<double>&
        {
            if (!priced_ready[mask])
            {
                priced_rows[mask] = BuildTopRow(graph, groups, rows, mask);
                priced_ready[mask] = 1;
            }
            return priced_rows[mask];
        };
        for (const auto [left, right] : half_partitions)
        {
            const auto& left_row = GetPricedRow(left);
            const auto& right_row = GetPricedRow(right);
            for (int pair_root = 1; pair_root <= graph.n; ++pair_root)
            {
                const Witness witness = BuildWitness(
                    graph, groups, rows, best_origin_pair, pair_root,
                    edge_stamp, vertex_stamp, witness_stamp);
                double left_value = gst::fp::kInf;
                double right_value = gst::fp::kInf;
                for (int vertex : witness.vertices)
                {
                    left_value = std::min(left_value, left_row[vertex]);
                    right_value = std::min(right_value, right_row[vertex]);
                }
                half_priced_completion = std::min(
                    half_priced_completion,
                    EdgeCost(graph, witness.edges) + left_value + right_value);
                ++half_priced_candidates;
            }
        }
        half_pricing_ms =
            std::chrono::duration<double, std::milli>(Clock::now() -
                                                       half_pricing_begin)
                .count();
    }
    const double total_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    std::cout << std::fixed << std::setprecision(10)
              << "paid_core_growth query=" << query_id
              << " n=" << graph.n
              << " m=" << graph.m
              << " g=" << g
              << " q=" << q
              << " core_size=" << core_size
              << " anchor_group=" << (anchor_group + 1)
              << " known_optimum=" << known_optimum
              << " completion=" << completion
              << " gap=" << (completion - known_optimum)
              << " three_block_upper=" << three_block.upper
              << " three_block_gap=" <<
                     (three_block.upper - known_optimum)
              << " three_block_partitions=" << three_block.partitions
              << " three_block_probes=" << three_block.probes
              << " three_block_ms=" << three_block.milliseconds
              << " three_block_first=" << three_block.first
              << " three_block_second=" << three_block.second
              << " three_block_third=" << three_block.third
              << " three_block_root=" << three_block.root
              << " three_block_priced="
              << (price_best_plan ? three_block_priced : -1.0)
              << " three_block_priced_gap="
              << (price_best_plan
                      ? three_block_priced - known_optimum
                      : -1.0)
              << " three_block_priced_candidates="
              << three_block_priced_candidates
              << " three_block_pricing_ms=" << three_block_pricing_ms
              << " component_plan_count=" << component_plans.size()
              << " component_best_lower="
              << (component_plans.empty() ? gst::fp::kInf
                                          : component_plans.front().lower)
              << " component_below_optimum=" << component_below_optimum
              << " best_component_rank=" << best_component_rank
              << " strong_best_lower="
              << (strong_plans.empty() ? gst::fp::kInf
                                       : strong_plans.front().lower)
              << " strong_below_optimum=" << strong_below_optimum
              << " best_strong_rank=" << best_strong_rank
              << " root_free_probes=" << root_free_probes
              << " root_free_ms=" << root_free_ms
              << " near_full_lower="
              << (measure_near_full_lower ? near_full_lower : -1.0)
              << " near_full_gap="
              << (measure_near_full_lower
                      ? known_optimum - near_full_lower
                      : -1.0)
              << " near_full_masks=" << near_full_masks
              << " near_full_probes=" << near_full_probes
              << " near_full_ms=" << near_full_ms
              << " macro_tour_best_lower="
              << (measure_macro_tour_lower
                      ? macro_tour_best_lower
                      : -1.0)
              << " macro_tour_below_optimum="
              << macro_tour_below_optimum
              << " macro_tour_closed_plans="
              << macro_tour_closed_plans
              << " macro_tour_ms=" << macro_tour_ms
              << " block_anchor_raw_keys=" << block_anchor_raw_keys
              << " block_anchor_unique_keys="
              << block_anchor_unique_keys
              << " block_anchor_distinct_blocks="
              << block_anchor_distinct_blocks
              << " block_anchor_max_keys_per_block="
              << block_anchor_max_keys_per_block
              << " block_anchor_keys_d0=" << block_anchor_keys_by_size[0]
              << " block_anchor_keys_d1=" << block_anchor_keys_by_size[1]
              << " block_anchor_keys_d2=" << block_anchor_keys_by_size[2]
              << " block_anchor_keys_d3=" << block_anchor_keys_by_size[3]
              << " block_anchor_keys_d4=" << block_anchor_keys_by_size[4]
              << " block_anchor_closed_keys="
              << block_anchor_closed_keys
              << " block_anchor_closed_d0="
              << block_anchor_closed_by_size[0]
              << " block_anchor_closed_d1="
              << block_anchor_closed_by_size[1]
              << " block_anchor_closed_d2="
              << block_anchor_closed_by_size[2]
              << " block_anchor_key_ms=" << block_anchor_key_ms
              << " first_order_anchor_family_best="
              << (price_first_order_anchor_plans
                      ? first_order_anchor_family_best
                      : -1.0)
              << " first_order_anchor_first_exact_rank="
              << first_order_anchor_first_exact_rank
              << " first_order_anchor_rows_d1="
              << first_order_anchor_rows_d1
              << " first_order_anchor_rows_d2="
              << first_order_anchor_rows_d2
              << " first_order_anchor_candidate_plans="
              << first_order_anchor_candidate_plans
              << " first_order_anchor_row_values="
              << first_order_anchor_row_values
              << " first_order_anchor_queue_pops="
              << first_order_anchor_queue_pops
              << " first_order_anchor_plan_probes="
              << first_order_anchor_plan_probes
              << " first_order_anchor_build_ms="
              << first_order_anchor_build_ms
              << " first_order_anchor_pricing_ms="
              << first_order_anchor_pricing_ms
              << " three_pendant_min_block="
              << three_pendant_min_block
              << " three_pendant_max_block="
              << three_pendant_max_block
              << " three_pendant_plan_count="
              << three_pendant_plan_count
              << " three_pendant_below_optimum="
              << three_pendant_below_optimum
              << " three_pendant_below_upper="
              << three_pendant_below_upper
              << " three_pendant_best_lower="
              << (measure_three_pendant_plans
                      ? three_pendant_best_lower
                      : -1.0)
              << " three_pendant_anchor_keys="
              << three_pendant_unique_anchor_keys
              << " three_pendant_anchor_keys_d0="
              << three_pendant_anchor_keys_by_size[0]
              << " three_pendant_anchor_keys_d1="
              << three_pendant_anchor_keys_by_size[1]
              << " three_pendant_anchor_keys_d2="
              << three_pendant_anchor_keys_by_size[2]
              << " three_pendant_anchor_keys_d3="
              << three_pendant_anchor_keys_by_size[3]
              << " three_pendant_anchor_keys_d4="
              << three_pendant_anchor_keys_by_size[4]
              << " three_pendant_ms=" << three_pendant_ms
              << " pendant_cherry_plan_count="
              << pendant_cherry_plan_count
              << " pendant_cherry_plans_l3="
              << pendant_cherry_plans_by_labels[3]
              << " pendant_cherry_plans_l4="
              << pendant_cherry_plans_by_labels[4]
              << " pendant_cherry_below_optimum="
              << pendant_cherry_below_optimum
              << " pendant_cherry_below_upper="
              << pendant_cherry_below_upper
              << " pendant_cherry_best_lower="
              << (measure_pendant_cherry_plans
                      ? pendant_cherry_best_lower
                      : -1.0)
              << " pendant_cherry_unique_atoms="
              << pendant_cherry_unique_atoms
              << " pendant_cherry_atoms_d1="
              << pendant_cherry_atoms_by_size[1]
              << " pendant_cherry_atoms_d3="
              << pendant_cherry_atoms_by_size[3]
              << " pendant_cherry_atoms_d4="
              << pendant_cherry_atoms_by_size[4]
              << " pendant_cherry_raw_targets="
              << pendant_cherry_raw_targets
              << " pendant_cherry_target_records="
              << pendant_cherry_target_records
              << " pendant_cherry_all_target_records="
              << pendant_cherry_all_target_records
              << " pendant_cherry_half_rows="
              << pendant_cherry_half_rows
              << " pendant_cherry_ordinary_rows="
              << pendant_cherry_ordinary_rows
              << " pendant_cherry_anchored_rows="
              << pendant_cherry_anchored_rows
              << " pendant_cherry_rows_d4="
              << pendant_cherry_rows_by_size[4]
              << " pendant_cherry_rows_d5="
              << pendant_cherry_rows_by_size[5]
              << " pendant_cherry_rows_d6="
              << pendant_cherry_rows_by_size[6]
              << " pendant_cherry_max_targets_per_row="
              << pendant_cherry_max_targets_per_row
              << " pendant_cherry_family_best="
              << (measure_pendant_cherry_plans
                      ? pendant_cherry_family_best
                      : -1.0)
              << " pendant_cherry_price_probes="
              << pendant_cherry_price_probes
              << " pendant_cherry_pricing_ms="
              << pendant_cherry_pricing_ms
              << " pendant_anchor_witness_price="
              << (measure_pendant_cherry_plans && q >= half
                      ? pendant_anchor_witness_price
                      : -1.0)
              << " pendant_anchor_witness_gap="
              << (measure_pendant_cherry_plans && q >= half
                      ? pendant_anchor_witness_price - known_optimum
                      : -1.0)
              << " pendant_anchor_standard_price="
              << (measure_pendant_cherry_plans && q >= half
                      ? pendant_anchor_standard_price
                      : -1.0)
              << " pendant_anchor_witness_edge_cost="
              << pendant_anchor_witness_edge_cost
              << " pendant_anchor_witness_core="
              << pendant_anchor_witness_core
              << " pendant_anchor_witness_block_1="
              << pendant_anchor_witness_blocks[0]
              << " pendant_anchor_witness_block_2="
              << pendant_anchor_witness_blocks[1]
              << " pendant_anchor_witness_block_3="
              << pendant_anchor_witness_blocks[2]
              << " pendant_anchor_witness_queue_pops="
              << pendant_anchor_witness_queue_pops
              << " pendant_anchor_witness_ms="
              << pendant_anchor_witness_ms
              << " pendant_cherry_ms=" << pendant_cherry_ms
              << " macro_packing_best_lower="
              << (measure_macro_packing_lower
                      ? macro_packing_best_lower
                      : -1.0)
              << " macro_packing_below_optimum="
              << macro_packing_below_optimum
              << " macro_packing_closed_plans="
              << macro_packing_closed_plans
              << " macro_packing_root_free_probes="
              << macro_packing_root_free_probes
              << " macro_packing_ms=" << macro_packing_ms
              << " strong_distinct_cores=" << strong_distinct_cores
              << " strong_distinct_pairs=" << strong_distinct_pairs
              << " strong_pair_core_incidence="
              << unique_pair_cores.size()
              << " strong_max_plans_per_core="
              << strong_max_plans_per_core
              << " strong_max_cores_per_pair="
              << strong_max_cores_per_pair
              << " strong_bound_pair_root_scans="
              << strong_bound_pair_root_scans
              << " strong_bound_core_root_scans="
              << strong_bound_core_root_scans
              << " strong_plan_pair_root_probes="
              << strong_plan_pair_root_probes
              << " factor_profile_columns="
              << factor_profile_keys.size()
              << " factor_distinct_pairs=" << factor_distinct_pairs
              << " factor_group_profile_columns="
              << factor_group_profile_keys.size()
              << " factor_max_columns_per_pair="
              << factor_max_columns_per_pair
              << " factor_profile_values="
              << static_cast<long long>(factor_profile_keys.size()) * graph.n
              << " factor_profile_checks=" << factor_profile_checks
              << " factor_profile_errors=" << factor_profile_errors
              << " factor_target_errors=" << factor_target_errors
              << " factor_profile_checksum=" << factor_profile_checksum
              << " factor_profile_ms=" << factor_profile_ms
              << " factor_target_ms=" << factor_target_ms
              << " factor_attachment_targets="
              << factor_attachment_targets
              << " factor_attachment_survivors="
              << factor_attachment_survivors
              << " factor_attachment_ms=" << factor_attachment_ms
              << " factorized_family_best="
              << (price_factorized_plans ? factorized_family_best : -1.0)
              << " factorized_first_exact_rank="
              << factorized_first_exact_rank
              << " factorized_unpriced_plans="
              << factorized_unpriced_plans
              << " factorized_records=" << factorized_records
              << " factorized_tied_targets="
              << factorized_tied_targets
              << " factorized_tied_records="
              << factorized_tied_records
              << " factorized_argmin_checks="
              << factorized_argmin_checks
              << " factorized_argmin_errors="
              << factorized_argmin_errors
              << " factorized_completion_probes="
              << factorized_completion_probes
              << " factorized_attachment_ms="
              << factorized_attachment_ms
              << " factorized_completion_ms="
              << factorized_completion_ms
              << " factorized_price_compared="
              << factorized_price_compared
              << " factorized_price_mismatches="
              << factorized_price_mismatches
              << " factorized_price_lower="
              << factorized_price_lower
              << " factorized_price_higher="
              << factorized_price_higher
              << " factorized_price_max_gap="
              << factorized_price_max_gap
              << " rooted_core_family_best="
              << (price_rooted_core_plans
                      ? rooted_core_family_best
                      : -1.0)
              << " rooted_core_first_exact_rank="
              << rooted_core_first_exact_rank
              << " rooted_core_unpriced_plans="
              << rooted_core_unpriced_plans
              << " rooted_core_roots=" << rooted_core_roots
              << " rooted_core_completion_probes="
              << rooted_core_completion_probes
              << " rooted_core_pricing_ms="
              << rooted_core_pricing_ms
              << " rooted_tour_exact_plans="
              << rooted_tour_exact_plans
              << " rooted_tour_unresolved_plans="
              << rooted_tour_unresolved_plans
              << " optimal_core_family_best="
              << (price_optimal_core_plans
                      ? optimal_core_family_best
                      : -1.0)
              << " optimal_core_first_exact_rank="
              << optimal_core_first_exact_rank
              << " optimal_core_calls=" << optimal_core_calls
              << " optimal_core_join_transitions="
              << optimal_core_join_transitions
              << " optimal_core_edge_transitions="
              << optimal_core_edge_transitions
              << " optimal_core_pricing_ms="
              << optimal_core_pricing_ms
              << " macro_core_family_best="
              << (price_macro_core_plans
                      ? macro_core_family_best
                      : -1.0)
              << " macro_core_first_exact_rank="
              << macro_core_first_exact_rank
              << " macro_core_calls=" << macro_core_calls
              << " macro_core_join_probes="
              << macro_core_join_probes
              << " macro_core_queue_pops="
              << macro_core_queue_pops
              << " macro_core_pricing_ms="
              << macro_core_pricing_ms
              << " contracted_core_family_best="
              << (price_contracted_core_plans
                      ? contracted_core_family_best
                      : -1.0)
              << " contracted_core_separate_best="
              << (price_contracted_core_plans
                      ? contracted_core_separate_best
                      : -1.0)
              << " contracted_core_first_exact_rank="
              << contracted_core_first_exact_rank
              << " contracted_core_calls=" << contracted_core_calls
              << " contracted_core_roots=" << contracted_core_roots
              << " contracted_core_join_probes="
              << contracted_core_join_probes
              << " contracted_core_queue_pops="
              << contracted_core_queue_pops
              << " contracted_core_pricing_ms="
              << contracted_core_pricing_ms
              << " pair_union_core_family_best="
              << (price_pair_union_core_plans
                      ? pair_union_core_family_best
                      : -1.0)
              << " pair_union_core_first_exact_rank="
              << pair_union_core_first_exact_rank
              << " pair_union_core_calls=" << pair_union_core_calls
              << " pair_union_core_candidates="
              << pair_union_core_candidates
              << " pair_union_core_profile_probes="
              << pair_union_core_profile_probes
              << " pair_union_core_pricing_ms="
              << pair_union_core_pricing_ms
              << " all_attachment_core_family_best="
              << (price_all_attachment_core_plans
                      ? all_attachment_core_family_best
                      : -1.0)
              << " all_attachment_core_first_exact_rank="
              << all_attachment_core_first_exact_rank
              << " all_attachment_core_calls="
              << all_attachment_core_calls
              << " all_attachment_core_candidates="
              << all_attachment_core_candidates
              << " all_attachment_core_profile_probes="
              << all_attachment_core_profile_probes
              << " all_attachment_core_pricing_ms="
              << all_attachment_core_pricing_ms
              << " oracle_tree_core_family_best="
              << (price_oracle_tree_core_plans
                      ? oracle_tree_core_family_best
                      : -1.0)
              << " oracle_tree_core_contracted_best="
              << (price_oracle_tree_core_plans
                      ? oracle_tree_core_contracted_best
                      : -1.0)
              << " oracle_tree_strong_best="
              << (price_oracle_tree_core_plans
                      ? oracle_tree_strong_best
                      : -1.0)
              << " oracle_tree_cost="
              << (price_oracle_tree_core_plans ? oracle_tree_cost : -1.0)
              << " oracle_tree_cycle_rank=" << oracle_tree_cycle_rank
              << " oracle_tree_first_exact_rank="
              << oracle_tree_first_exact_rank
              << " oracle_tree_best_core=" << oracle_tree_best_core
              << " oracle_tree_best_left=" << oracle_tree_best_left
              << " oracle_tree_best_right=" << oracle_tree_best_right
              << " oracle_tree_core_calls=" << oracle_tree_core_calls
              << " oracle_tree_core_edges=" << oracle_tree_core_edges
              << " oracle_tree_core_pricing_ms="
              << oracle_tree_core_pricing_ms
              << " correction_plans=" << correction_plans.size()
              << " correction_distinct_cores="
              << correction_distinct_cores
              << " correction_strong_below_optimum="
              << correction_strong_below_optimum
              << " correction_root_free_probes="
              << correction_root_free_probes
              << " correction_lower_ms=" << correction_lower_ms
              << " correction_pair_core_incidence="
              << unique_correction_budgets.size()
              << " correction_max_plans_per_core="
              << correction_max_plans_per_core
              << " correction_bound_core_root_scans="
              << correction_bound_core_root_scans
              << " strong_family_best="
              << (price_strong_plans ? strong_family_best : -1.0)
              << " strong_family_first_exact_rank="
              << strong_family_first_exact_rank
              << " strong_family_unpriced_plans="
              << strong_family_unpriced_plans
              << " strong_family_pair_roots="
              << strong_family_pair_roots
              << " strong_family_core_roots="
              << strong_family_core_roots
              << " strong_family_core_candidates="
              << strong_family_core_candidates
              << " strong_family_completion_probes="
              << strong_family_completion_probes
              << " strong_family_pricing_ms="
              << strong_family_pricing_ms
              << " raw_seed_states=" << raw_seed_states
              << " seed_states=" << seed_states
              << " generated=" << generated
              << " core_states=" << core_states
              << " max_front=" << maximum_front
              << " best_core_mask=" << best_core_mask
              << " best_left=" << best_left
              << " best_right=" << best_right
              << " best_origin_pair=" << best_origin_pair
              << " best_origin_root=" << best_origin_root
              << " best_growth_1="
              << (best_growth_blocks.empty() ? 0 : best_growth_blocks[0])
              << " best_growth_2="
              << (best_growth_blocks.size() < 2 ? 0 : best_growth_blocks[1])
              << " priced_completion="
              << (price_best_plan ? priced_completion : -1.0)
              << " priced_gap="
              << (price_best_plan ? priced_completion - known_optimum : -1.0)
              << " priced_candidates=" << priced_candidates
              << " pricing_ms=" << pricing_ms
              << " half_priced_completion="
              << (price_best_plan && half_priced_candidates
                      ? half_priced_completion
                      : -1.0)
              << " half_priced_gap="
              << (price_best_plan && half_priced_candidates
                      ? half_priced_completion - known_optimum
                      : -1.0)
              << " half_priced_candidates=" << half_priced_candidates
              << " half_pricing_ms=" << half_pricing_ms
              << " low_seed_probes=" << rows.seed_probes
              << " low_queue_pops=" << rows.queue_pops
              << " low_ms=" << rows.milliseconds
              << " total_ms=" << total_ms << '\n';
}
}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 5 || argc > 28)
        {
            std::cerr << "usage: " << argv[0]
                      << " <data_root> <graph_selector> <query_selector>"
                      << " <known_optimum> [query_begin_1based=1]"
                      << " [--profile-seeds] [--price-best-plan]"
                      << " [--price-strong-plans] [--price-factorized-plans]"
                      << " [--price-rooted-core-plans]"
                      << " [--price-optimal-core-plans]"
                      << " [--price-macro-core-plans]"
                      << " [--price-contracted-core-plans]"
                      << " [--price-pair-union-core-plans]"
                      << " [--price-all-attachment-core-plans]"
                      << " [--price-oracle-tree-core-plans]"
                      << " [--measure-near-full-lower]"
                      << " [--measure-macro-tour-lower]"
                      << " [--measure-block-anchor-keys]"
                      << " [--price-first-order-anchor-plans]"
                      << " [--measure-three-pendant-plans]"
                      << " [--measure-pendant-cherry-plans]"
                      << " [--measure-macro-packing-lower]"
                      << " [--measure-factor-columns]"
                      << " [--measure-factor-targets]"
                      << " [--q value]"
                      << " [--core-size value]\n";
            return 2;
        }
        const std::string graph_folder = gst::ResolveGraphFolder(argv[1], argv[2]);
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const std::vector<gst::Query> queries =
            gst::LoadQueriesFromFolder(graph_folder, argv[3]);
        const double known_optimum = std::stod(argv[4]);
        const int query_id = argc == 6 ? std::stoi(argv[5]) : 1;
        bool profile_seeds = false;
        bool price_best_plan = false;
        bool price_strong_plans = false;
        bool price_factorized_plans = false;
        bool price_rooted_core_plans = false;
        bool price_optimal_core_plans = false;
        bool price_macro_core_plans = false;
        bool price_contracted_core_plans = false;
        bool price_pair_union_core_plans = false;
        bool price_all_attachment_core_plans = false;
        bool price_oracle_tree_core_plans = false;
        bool measure_near_full_lower = false;
        bool measure_macro_tour_lower = false;
        bool measure_block_anchor_keys = false;
        bool price_first_order_anchor_plans = false;
        bool measure_three_pendant_plans = false;
        bool measure_pendant_cherry_plans = false;
        bool measure_macro_packing_lower = false;
        bool measure_factor_columns = false;
        bool measure_factor_targets = false;
        int q_override = 0;
        int core_size_override = 0;
        for (int index = 6; index < argc; ++index)
        {
            const std::string option = argv[index];
            if (option == "--profile-seeds")
                profile_seeds = true;
            else if (option == "--price-best-plan")
                price_best_plan = true;
            else if (option == "--price-strong-plans")
                price_strong_plans = true;
            else if (option == "--price-factorized-plans")
                price_factorized_plans = true;
            else if (option == "--price-rooted-core-plans")
                price_rooted_core_plans = true;
            else if (option == "--price-optimal-core-plans")
                price_optimal_core_plans = true;
            else if (option == "--price-macro-core-plans")
                price_macro_core_plans = true;
            else if (option == "--price-contracted-core-plans")
                price_contracted_core_plans = true;
            else if (option == "--price-pair-union-core-plans")
                price_pair_union_core_plans = true;
            else if (option == "--price-all-attachment-core-plans")
                price_all_attachment_core_plans = true;
            else if (option == "--price-oracle-tree-core-plans")
                price_oracle_tree_core_plans = true;
            else if (option == "--measure-near-full-lower")
                measure_near_full_lower = true;
            else if (option == "--measure-macro-tour-lower")
                measure_macro_tour_lower = true;
            else if (option == "--measure-block-anchor-keys")
                measure_block_anchor_keys = true;
            else if (option == "--price-first-order-anchor-plans")
                price_first_order_anchor_plans = true;
            else if (option == "--measure-three-pendant-plans")
                measure_three_pendant_plans = true;
            else if (option == "--measure-pendant-cherry-plans")
                measure_pendant_cherry_plans = true;
            else if (option == "--measure-macro-packing-lower")
                measure_macro_packing_lower = true;
            else if (option == "--measure-factor-columns")
                measure_factor_columns = true;
            else if (option == "--measure-factor-targets")
                measure_factor_targets = true;
            else if (option == "--q")
            {
                if (++index >= argc)
                    throw std::runtime_error("--q requires a value");
                q_override = std::stoi(argv[index]);
            }
            else if (option == "--core-size")
            {
                if (++index >= argc)
                    throw std::runtime_error("--core-size requires a value");
                core_size_override = std::stoi(argv[index]);
            }
            else
                throw std::runtime_error("unknown option: " + option);
        }
        if (query_id < 1 || query_id > static_cast<int>(queries.size()))
            throw std::runtime_error("query index out of range");
        Probe(graph, queries[query_id - 1], query_id, known_optimum,
              profile_seeds, price_best_plan, price_strong_plans,
              price_factorized_plans, price_rooted_core_plans,
              price_optimal_core_plans, price_macro_core_plans,
              price_contracted_core_plans,
              price_pair_union_core_plans,
              price_all_attachment_core_plans,
              price_oracle_tree_core_plans,
              measure_near_full_lower,
              measure_macro_tour_lower,
              measure_block_anchor_keys,
              price_first_order_anchor_plans,
              measure_three_pendant_plans,
              measure_pendant_cherry_plans,
              measure_macro_packing_lower,
              measure_factor_columns, measure_factor_targets,
              q_override, core_size_override);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
