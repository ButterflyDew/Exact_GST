#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr double kInf = std::numeric_limits<double>::infinity();

struct Edge
{
    int u = 0;
    int v = 0;
    int w = 0;
};

struct Candidate
{
    double cost = 0.0;
    std::vector<double> profile;
};

struct Dsu
{
    explicit Dsu(int n) : parent(n), rank(n) { std::iota(parent.begin(), parent.end(), 0); }

    int Find(int x)
    {
        return parent[x] == x ? x : parent[x] = Find(parent[x]);
    }

    void Unite(int a, int b)
    {
        a = Find(a);
        b = Find(b);
        if (a == b)
            return;
        if (rank[a] < rank[b])
            std::swap(a, b);
        parent[b] = a;
        if (rank[a] == rank[b])
            ++rank[a];
    }

    std::vector<int> parent;
    std::vector<int> rank;
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
    while ((mask & 1) == 0)
    {
        mask >>= 1;
        ++bit;
    }
    return bit;
}

bool Dominates(const Candidate& a, const Candidate& b, int remaining)
{
    if (a.cost > b.cost)
        return false;
    for (int mask = remaining; mask; mask = (mask - 1) & remaining)
        if (a.profile[mask] > b.profile[mask])
            return false;
    return true;
}

std::vector<std::vector<double>> AllPairs(
    int n, const std::vector<std::vector<std::pair<int, int>>>& adj)
{
    std::vector<std::vector<double>> distance(n, std::vector<double>(n, kInf));
    using Item = std::pair<double, int>;
    for (int source = 0; source < n; ++source)
    {
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap;
        distance[source][source] = 0.0;
        heap.push({0.0, source});
        while (!heap.empty())
        {
            const auto [value, vertex] = heap.top();
            heap.pop();
            if (value != distance[source][vertex])
                continue;
            for (const auto [to, weight] : adj[vertex])
            {
                const double next = value + weight;
                if (next < distance[source][to])
                {
                    distance[source][to] = next;
                    heap.push({next, to});
                }
            }
        }
    }
    return distance;
}

std::vector<std::vector<double>> RootedSteiner(
    int n,
    const std::vector<std::vector<int>>& groups,
    const std::vector<std::vector<double>>& metric)
{
    const int g = static_cast<int>(groups.size());
    const int count = 1 << g;
    std::vector<std::vector<double>> dp(count, std::vector<double>(n, kInf));
    for (int bit = 0; bit < g; ++bit)
        for (int vertex = 0; vertex < n; ++vertex)
            for (int terminal : groups[bit])
                dp[1 << bit][vertex] = std::min(
                    dp[1 << bit][vertex], metric[terminal][vertex]);

    for (int mask = 1; mask < count; ++mask)
    {
        if ((mask & (mask - 1)) == 0)
            continue;
        const int pivot = mask & -mask;
        for (int left = (mask - 1) & mask; left; left = (left - 1) & mask)
        {
            if (!(left & pivot))
                continue;
            const int right = mask ^ left;
            if (!right)
                continue;
            for (int vertex = 0; vertex < n; ++vertex)
                dp[mask][vertex] =
                    std::min(dp[mask][vertex], dp[left][vertex] + dp[right][vertex]);
        }
        std::vector<double> closed(n, kInf);
        for (int root = 0; root < n; ++root)
            for (int vertex = 0; vertex < n; ++vertex)
                closed[vertex] =
                    std::min(closed[vertex], dp[mask][root] + metric[root][vertex]);
        dp[mask].swap(closed);
    }
    return dp;
}

double MacroLabels(const std::vector<std::vector<double>>& rooted,
                   const std::vector<std::vector<double>>& metric,
                   const std::vector<int>& labels)
{
    const int n = static_cast<int>(metric.size());
    const int label_count = static_cast<int>(labels.size());
    std::vector<std::vector<double>> dp(
        1 << label_count, std::vector<double>(n, kInf));
    for (int label = 0; label < label_count; ++label)
        dp[1 << label] = rooted[labels[label]];
    for (int mask = 1; mask < (1 << label_count); ++mask)
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
                for (int vertex = 0; vertex < n; ++vertex)
                    dp[mask][vertex] = std::min(
                        dp[mask][vertex],
                        dp[left][vertex] + dp[right][vertex]);
            }
        }
        std::vector<double> closed(n, kInf);
        for (int root = 0; root < n; ++root)
            for (int vertex = 0; vertex < n; ++vertex)
                closed[vertex] = std::min(
                    closed[vertex], dp[mask][root] + metric[root][vertex]);
        dp[mask].swap(closed);
    }
    return *std::min_element(dp.back().begin(), dp.back().end());
}

double MacroPlan(const std::vector<std::vector<double>>& rooted,
                 const std::vector<std::vector<double>>& metric,
                 int core,
                 int first_block,
                 int second_block)
{
    std::vector<int> labels;
    for (int bits = core; bits; bits &= bits - 1)
        labels.push_back(bits & -bits);
    labels.push_back(first_block);
    labels.push_back(second_block);
    return labels.size() == 6
        ? MacroLabels(rooted, metric, labels)
        : kInf;
}

std::vector<std::vector<double>> BlockAnchoredRows(
    const std::vector<std::vector<double>>& rooted,
    const std::vector<std::vector<double>>& metric,
    int block,
    int core)
{
    const int n = static_cast<int>(metric.size());
    std::vector<int> labels;
    for (int bits = core; bits; bits &= bits - 1)
        labels.push_back(bits & -bits);
    const int subset_count = 1 << static_cast<int>(labels.size());
    std::vector<int> original_mask(subset_count);
    for (int mask = 1; mask < subset_count; ++mask)
    {
        const int bit = FirstBit(mask);
        original_mask[mask] = original_mask[mask ^ (1 << bit)] | labels[bit];
    }
    std::vector<std::vector<double>> anchored(
        subset_count, std::vector<double>(n, kInf));
    anchored[0] = rooted[block];
    for (int mask = 1; mask < subset_count; ++mask)
    {
        for (int branch = mask; branch;
             branch = (branch - 1) & mask)
        {
            const int base = mask ^ branch;
            for (int vertex = 0; vertex < n; ++vertex)
                anchored[mask][vertex] = std::min(
                    anchored[mask][vertex],
                    anchored[base][vertex] +
                        rooted[original_mask[branch]][vertex]);
        }
        std::vector<double> closed(n, kInf);
        for (int root = 0; root < n; ++root)
            for (int vertex = 0; vertex < n; ++vertex)
                closed[vertex] = std::min(
                    closed[vertex],
                    anchored[mask][root] + metric[root][vertex]);
        anchored[mask].swap(closed);
    }
    return anchored;
}

double AnchoredMacroPlan(const std::vector<std::vector<double>>& rooted,
                         const std::vector<std::vector<double>>& metric,
                         int core,
                         int first_block,
                         int second_block)
{
    const auto first = BlockAnchoredRows(
        rooted, metric, first_block, core);
    const auto second = BlockAnchoredRows(
        rooted, metric, second_block, core);
    const int full = static_cast<int>(first.size()) - 1;
    double result = kInf;
    for (int left = 0; left <= full; ++left)
    {
        const int right = full ^ left;
        for (int vertex = 0; vertex < static_cast<int>(metric.size()); ++vertex)
            result = std::min(
                result, first[left][vertex] + second[right][vertex]);
    }
    return result;
}

double FirstOrderAnchoredMacroPlan(
    const std::vector<std::vector<double>>& rooted,
    const std::vector<std::vector<double>>& metric,
    int core,
    int first_block,
    int second_block)
{
    const int n = static_cast<int>(metric.size());
    std::vector<int> labels;
    for (int bits = core; bits; bits &= bits - 1)
        labels.push_back(bits & -bits);
    const int subset_count = 1 << static_cast<int>(labels.size());
    const int full = subset_count - 1;
    std::vector<int> original_mask(subset_count);
    for (int mask = 1; mask < subset_count; ++mask)
    {
        const int bit = FirstBit(mask);
        original_mask[mask] = original_mask[mask ^ (1 << bit)] | labels[bit];
    }
    auto BuildAnchored = [&](int block, int maximum_size)
    {
        std::vector<std::vector<double>> anchored(
            subset_count, std::vector<double>(n, kInf));
        anchored[0] = rooted[block];
        for (int size = 1; size <= maximum_size; ++size)
            for (int mask = 1; mask < subset_count; ++mask)
            {
                if (Bits(mask) != size)
                    continue;
                for (int branch = mask; branch;
                     branch = (branch - 1) & mask)
                {
                    const int base = mask ^ branch;
                    for (int vertex = 0; vertex < n; ++vertex)
                        anchored[mask][vertex] = std::min(
                            anchored[mask][vertex],
                            anchored[base][vertex] +
                                rooted[original_mask[branch]][vertex]);
                }
                std::vector<double> closed(n, kInf);
                for (int root = 0; root < n; ++root)
                    for (int vertex = 0; vertex < n; ++vertex)
                        closed[vertex] = std::min(
                            closed[vertex],
                            anchored[mask][root] + metric[root][vertex]);
                anchored[mask].swap(closed);
        }
        return anchored;
    };
    const auto first = BuildAnchored(first_block, 1);
    const auto second = BuildAnchored(second_block, 2);

    double result = kInf;
    for (int first_mask = 0; first_mask <= full; ++first_mask)
    {
        if (Bits(first_mask) > 1)
            continue;
        const int after_first = full ^ first_mask;
        for (int second_mask = after_first;;
             second_mask = (second_mask - 1) & after_first)
        {
            if (Bits(second_mask) <= 2)
            {
                const int ordinary_mask = after_first ^ second_mask;
                if (ordinary_mask)
                    for (int vertex = 0; vertex < n; ++vertex)
                        result = std::min(
                            result,
                            first[first_mask][vertex] +
                                second[second_mask][vertex] +
                                rooted[original_mask[ordinary_mask]][vertex]);
            }
            if (!second_mask)
                break;
        }
    }
    return result;
}

double MacroCherryPlan(
    const std::vector<std::vector<double>>& rooted,
    const std::vector<std::vector<double>>& metric,
    const std::vector<int>& labels)
{
    const int n = static_cast<int>(metric.size());
    const int label_count = static_cast<int>(labels.size());
    if (label_count < 3 || label_count > 5)
        return kInf;

    auto Close = [&](const std::vector<double>& seed)
    {
        std::vector<double> closed(n, kInf);
        for (int root = 0; root < n; ++root)
            for (int vertex = 0; vertex < n; ++vertex)
                closed[vertex] = std::min(
                    closed[vertex], seed[root] + metric[root][vertex]);
        return closed;
    };

    std::vector<std::vector<double>> pair_rows(
        label_count * label_count);
    for (int first = 0; first < label_count; ++first)
        for (int second = first + 1; second < label_count; ++second)
        {
            std::vector<double> seed(n);
            for (int vertex = 0; vertex < n; ++vertex)
                seed[vertex] = rooted[labels[first]][vertex] +
                    rooted[labels[second]][vertex];
            pair_rows[first * label_count + second] = Close(seed);
        }
    auto Pair = [&](int first, int second) -> const std::vector<double>&
    {
        if (first > second)
            std::swap(first, second);
        return pair_rows[first * label_count + second];
    };

    double result = kInf;
    if (label_count == 3)
    {
        for (int vertex = 0; vertex < n; ++vertex)
            result = std::min(result,
                rooted[labels[0]][vertex] + rooted[labels[1]][vertex] +
                    rooted[labels[2]][vertex]);
        return result;
    }

    auto PricePairing = [&](int first, int second, int third, int fourth,
                            int singleton)
    {
        const auto& left = Pair(first, second);
        const auto& right = Pair(third, fourth);
        for (int vertex = 0; vertex < n; ++vertex)
        {
            double value = left[vertex] + right[vertex];
            if (singleton >= 0)
                value += rooted[labels[singleton]][vertex];
            result = std::min(result, value);
        }
    };
    auto PriceFour = [&](const std::vector<int>& four, int singleton)
    {
        for (int partner = 1; partner < 4; ++partner)
        {
            std::vector<int> rest;
            for (int index = 1; index < 4; ++index)
                if (index != partner)
                    rest.push_back(four[index]);
            PricePairing(four[0], four[partner], rest[0], rest[1],
                         singleton);
        }
    };
    if (label_count == 4)
    {
        for (int partner = 1; partner < 4; ++partner)
        {
            std::vector<int> rest;
            for (int label = 1; label < 4; ++label)
                if (label != partner)
                    rest.push_back(label);
            const auto& pair = Pair(0, partner);
            for (int vertex = 0; vertex < n; ++vertex)
                result = std::min(
                    result,
                    pair[vertex] + rooted[labels[rest[0]]][vertex] +
                        rooted[labels[rest[1]]][vertex]);
        }
    }
    else
        for (int singleton = 0; singleton < 5; ++singleton)
        {
            std::vector<int> four;
            for (int label = 0; label < 5; ++label)
                if (label != singleton)
                    four.push_back(label);
            PriceFour(four, singleton);
        }
    return result;
}

double ThreeBlockAnchoredPlan(
    const std::vector<std::vector<double>>& rooted,
    const std::vector<std::vector<double>>& metric,
    int core,
    const std::vector<int>& blocks)
{
    if (blocks.size() != 3)
        return kInf;
    std::vector<std::vector<std::vector<double>>> anchored;
    for (int block : blocks)
        anchored.push_back(BlockAnchoredRows(rooted, metric, block, core));

    const int full = static_cast<int>(anchored[0].size()) - 1;
    const int n = static_cast<int>(metric.size());
    double result = kInf;
    for (int first = 0; first <= full; ++first)
    {
        const int remaining = full ^ first;
        for (int second = remaining;;
             second = (second - 1) & remaining)
        {
            const int third = remaining ^ second;
            for (int vertex = 0; vertex < n; ++vertex)
                result = std::min(
                    result,
                    anchored[0][first][vertex] +
                        anchored[1][second][vertex] +
                        anchored[2][third][vertex]);
            if (!second)
                break;
        }
    }
    return result;
}

double NestedPendantFamily(
    const std::vector<std::vector<double>>& rooted,
    const std::vector<std::vector<double>>& metric,
    int group_count)
{
    const int n = static_cast<int>(metric.size());
    const int full = (1 << group_count) - 1;
    auto Close = [&](const std::vector<double>& seed)
    {
        std::vector<double> closed(n, kInf);
        for (int root = 0; root < n; ++root)
            for (int vertex = 0; vertex < n; ++vertex)
                closed[vertex] = std::min(
                    closed[vertex], seed[root] + metric[root][vertex]);
        return closed;
    };
    auto IsBlock = [](int mask)
    {
        const int size = Bits(mask);
        return size == 3 || size == 4;
    };

    double result = kInf;
    for (int core = 0; core <= full; ++core)
    {
        if (Bits(core) > 4)
            continue;
        const int outside_core = full ^ core;
        std::vector<double> core_row(n, 0.0);
        if (core)
            core_row = rooted[core];
        for (int third = outside_core; third;
             third = (third - 1) & outside_core)
        {
            if (!IsBlock(third))
                continue;
            std::vector<double> seed(n);
            for (int vertex = 0; vertex < n; ++vertex)
                seed[vertex] = core_row[vertex] + rooted[third][vertex];
            const auto third_row = Close(seed);
            const int first_two = outside_core ^ third;
            for (int second = first_two; second;
                 second = (second - 1) & first_two)
            {
                const int first = first_two ^ second;
                if (!IsBlock(first) || !IsBlock(second))
                    continue;
                for (int vertex = 0; vertex < n; ++vertex)
                    seed[vertex] = third_row[vertex] + rooted[second][vertex];
                const auto second_row = Close(seed);
                for (int vertex = 0; vertex < n; ++vertex)
                    result = std::min(
                        result,
                        second_row[vertex] + rooted[first][vertex]);
            }
        }
    }
    return result;
}

std::vector<double> SoftBlockTransform(
    const std::vector<std::vector<double>>& rooted,
    const std::vector<std::vector<double>>& metric,
    int block,
    const std::vector<double>& soft_base)
{
    const int n = static_cast<int>(metric.size());
    std::vector<int> bits;
    for (int remaining = block; remaining; remaining &= remaining - 1)
        bits.push_back(remaining & -remaining);
    const int local_count = 1 << bits.size();
    std::vector<int> original_mask(local_count);
    for (int local = 1; local < local_count; ++local)
    {
        const int bit = local & -local;
        original_mask[local] =
            original_mask[local ^ bit] + bits[FirstBit(bit)];
    }

    auto Close = [&](const std::vector<double>& seed)
    {
        std::vector<double> closed(n, kInf);
        for (int root = 0; root < n; ++root)
            for (int vertex = 0; vertex < n; ++vertex)
                closed[vertex] = std::min(
                    closed[vertex], seed[root] + metric[root][vertex]);
        return closed;
    };

    std::vector<std::vector<double>> soft(local_count);
    soft[0] = Close(soft_base);
    std::vector<double> seed(n);
    for (int size = 1; size <= static_cast<int>(bits.size()); ++size)
        for (int local = 1; local < local_count; ++local)
        {
            if (Bits(local) != size)
                continue;
            std::fill(seed.begin(), seed.end(), kInf);
            for (int ordinary = local; ordinary;
                 ordinary = (ordinary - 1) & local)
            {
                const int soft_side = local ^ ordinary;
                for (int vertex = 0; vertex < n; ++vertex)
                    seed[vertex] = std::min(
                        seed[vertex],
                        rooted[original_mask[ordinary]][vertex] +
                            soft[soft_side][vertex]);
            }
            soft[local] = Close(seed);
        }
    return soft.back();
}

double SoftPendantFamily(
    const std::vector<std::vector<double>>& rooted,
    const std::vector<std::vector<double>>& metric,
    int group_count)
{
    const int n = static_cast<int>(metric.size());
    const int full = (1 << group_count) - 1;
    auto IsBlock = [](int mask)
    {
        const int size = Bits(mask);
        return size == 3 || size == 4;
    };

    double result = kInf;
    for (int core = 0; core <= full; ++core)
    {
        if (Bits(core) > 4)
            continue;
        std::vector<double> core_row(n, 0.0);
        if (core)
            core_row = rooted[core];
        std::function<void(int, const std::vector<double>&)> Extend =
            [&](int remaining, const std::vector<double>& row)
        {
            if (!remaining)
            {
                for (double value : row)
                    result = std::min(result, value);
                return;
            }
            for (int block = remaining; block;
                 block = (block - 1) & remaining)
            {
                if (!IsBlock(block))
                    continue;
                const auto next =
                    SoftBlockTransform(rooted, metric, block, row);
                Extend(remaining ^ block, next);
            }
        };
        Extend(full ^ core, core_row);
    }
    return result;
}

struct InstanceResult
{
    bool exact = false;
    bool pair_block_exact = false;
    bool anchor_skyline_pair_exact = false;
    bool low_core_exact = false;
    bool generated_core_exact = false;
    bool middle_q_generated_core_exact = false;
    bool high_q_generated_core_exact = false;
    bool high_q_all_attachment_exact = false;
    bool high_q_intermediate_core_exact = false;
    bool high_q_intermediate_block_exact = false;
    bool high_q_intermediate_block_all_exact = false;
    bool rooted_core4_exact = false;
    bool skyline_generated_core_exact = false;
    bool anchor_pair_generated_core_exact = false;
    long long connected_candidates = 0;
    long long pair_candidates = 0;
    long long generated_states = 0;
    long long generated_core_states = 0;
    long long skyline_generated_states = 0;
    long long skyline_generated_core_states = 0;
    long long pareto_candidates = 0;
    int maximum_front = 0;
    bool macro_plan_exact = false;
    bool cherry_plan_exact = false;
    bool three_anchor_plan_exact = false;
    bool nested_pendant_family_exact = false;
    bool soft_pendant_family_exact = false;
};

InstanceResult RunInstance(std::mt19937_64& rng,
                           int n,
                           int g,
                           int edge_limit,
                           bool high_core4_only,
                           bool multi_terminal_groups,
                           bool macro_plan_check,
                           bool cherry_plan_check,
                           bool three_anchor_plan_check,
                           bool nested_pendant_family_check,
                           bool soft_pendant_family_check)
{
    std::uniform_int_distribution<int> weight(1, 20);
    std::vector<Edge> edges;
    std::vector<std::vector<std::pair<int, int>>> adj(n);
    auto AddEdge = [&](int u, int v)
    {
        const int w = weight(rng);
        edges.push_back({u, v, w});
        adj[u].push_back({v, w});
        adj[v].push_back({u, w});
    };
    for (int vertex = 1; vertex < n; ++vertex)
    {
        std::uniform_int_distribution<int> parent(0, vertex - 1);
        AddEdge(vertex, parent(rng));
    }
    std::uniform_int_distribution<int> vertex(0, n - 1);
    const int target_edges = std::min(edge_limit, n * (n - 1) / 2);
    while (static_cast<int>(edges.size()) < target_edges)
    {
        const int u = vertex(rng);
        const int v = vertex(rng);
        if (u == v)
            continue;
        bool exists = false;
        for (const Edge& edge : edges)
            exists |= (edge.u == u && edge.v == v) || (edge.u == v && edge.v == u);
        if (!exists)
            AddEdge(u, v);
    }

    std::vector<int> terminals(n);
    std::iota(terminals.begin(), terminals.end(), 0);
    std::shuffle(terminals.begin(), terminals.end(), rng);
    terminals.resize(g);
    std::vector<std::vector<int>> group_vertices(g);
    std::vector<int> alternatives(n);
    if (multi_terminal_groups)
    {
        std::iota(alternatives.begin(), alternatives.end(), 0);
        bool fixed_point = false;
        do
        {
            std::shuffle(alternatives.begin(), alternatives.end(), rng);
            fixed_point = false;
            for (int group = 0; group < g; ++group)
                fixed_point |= alternatives[group] == terminals[group];
        } while (fixed_point);
    }
    for (int group = 0; group < g; ++group)
    {
        group_vertices[group].push_back(terminals[group]);
        if (multi_terminal_groups)
            group_vertices[group].push_back(alternatives[group]);
    }
    const auto metric = AllPairs(n, adj);
    const auto rooted = RootedSteiner(n, group_vertices, metric);
    if (soft_pendant_family_check)
    {
        double exact = kInf;
        for (double value : rooted[(1 << g) - 1])
            exact = std::min(exact, value);
        const double soft = SoftPendantFamily(rooted, metric, g);
        InstanceResult result;
        result.soft_pendant_family_exact =
            std::abs(exact - soft) <= 1e-9;
        if (!result.soft_pendant_family_exact)
        {
            std::cout << "soft_pendant_exact=" << exact
                      << " soft_pendant_price=" << soft
                      << " g=" << g << '\n';
            for (const Edge& edge : edges)
                std::cout << "edge " << edge.u << ' ' << edge.v << ' '
                          << edge.w << '\n';
            for (int group = 0; group < g; ++group)
            {
                std::cout << "group " << group;
                for (int terminal : group_vertices[group])
                    std::cout << ' ' << terminal;
                std::cout << '\n';
            }
        }
        return result;
    }
    if (nested_pendant_family_check)
    {
        double exact = kInf;
        for (double value : rooted[(1 << g) - 1])
            exact = std::min(exact, value);
        const double nested = NestedPendantFamily(rooted, metric, g);
        InstanceResult result;
        result.nested_pendant_family_exact =
            std::abs(exact - nested) <= 1e-9;
        if (!result.nested_pendant_family_exact)
        {
            std::cout << "nested_pendant_exact=" << exact
                      << " nested_pendant_price=" << nested
                      << " g=" << g << '\n';
            for (const Edge& edge : edges)
                std::cout << "edge " << edge.u << ' ' << edge.v << ' '
                          << edge.w << '\n';
            for (int group = 0; group < g; ++group)
            {
                std::cout << "group " << group;
                for (int terminal : group_vertices[group])
                    std::cout << ' ' << terminal;
                std::cout << '\n';
            }
            std::uint64_t best_edges = 0;
            double best_cost = kInf;
            const std::uint64_t subset_count =
                std::uint64_t{1} << edges.size();
            for (std::uint64_t chosen = 0; chosen < subset_count; ++chosen)
            {
                Dsu dsu(n);
                std::vector<char> included(n);
                double cost = 0.0;
                int first = -1;
                for (size_t id = 0; id < edges.size(); ++id)
                    if (chosen & (std::uint64_t{1} << id))
                    {
                        const Edge& edge = edges[id];
                        included[edge.u] = included[edge.v] = 1;
                        dsu.Unite(edge.u, edge.v);
                        cost += edge.w;
                        first = edge.u;
                    }
                if (first < 0 || cost >= best_cost)
                    continue;
                bool connected = true;
                for (int vertex = 0; vertex < n; ++vertex)
                    if (included[vertex] &&
                        dsu.Find(vertex) != dsu.Find(first))
                        connected = false;
                for (const auto& group : group_vertices)
                {
                    bool hit = false;
                    for (int terminal : group)
                        hit |= included[terminal] != 0;
                    connected &= hit;
                }
                if (connected)
                {
                    best_cost = cost;
                    best_edges = chosen;
                }
            }
            std::cout << "witness";
            for (size_t id = 0; id < edges.size(); ++id)
                if (best_edges & (std::uint64_t{1} << id))
                    std::cout << ' ' << id;
            std::cout << '\n';
        }
        return result;
    }
    bool cherry_plan_exact = true;
    if (cherry_plan_check)
    {
        const int block_count = g / 3;
        const int core_size = g - 3 * block_count;
        std::vector<int> labels;
        for (int group = 0; group < core_size; ++group)
            labels.push_back(1 << group);
        for (int block = 0; block < block_count; ++block)
        {
            int mask = 0;
            for (int offset = 0; offset < 3; ++offset)
                mask |= 1 << (core_size + 3 * block + offset);
            labels.push_back(mask);
        }
        if (labels.size() < 3 || labels.size() > 5)
        {
            cherry_plan_exact = false;
            std::cout << "cherry_plan_requires_3_to_5_labels g=" << g
                      << " labels=" << labels.size() << '\n';
        }
        else
        {
            const double macro = MacroLabels(rooted, metric, labels);
            const double cherry = MacroCherryPlan(rooted, metric, labels);
            cherry_plan_exact = std::abs(macro - cherry) <= 1e-9;
            if (!cherry_plan_exact)
                std::cout << "cherry_plan_dp=" << macro
                          << " cherry_plan_price=" << cherry
                          << " labels=" << labels.size() << '\n';
        }
        InstanceResult result;
        result.cherry_plan_exact = cherry_plan_exact;
        return result;
    }
    bool three_anchor_plan_exact = true;
    if (three_anchor_plan_check)
    {
        const int core_size = g - 9;
        if (core_size < 0 || core_size > 4)
        {
            three_anchor_plan_exact = false;
            std::cout << "three_anchor_plan_requires_9_to_13_groups g="
                      << g << '\n';
        }
        else
        {
            const int core = (1 << core_size) - 1;
            std::vector<int> blocks(3);
            for (int block = 0; block < 3; ++block)
                for (int offset = 0; offset < 3; ++offset)
                    blocks[block] |=
                        1 << (core_size + 3 * block + offset);
            std::vector<int> labels;
            for (int bits = core; bits; bits &= bits - 1)
                labels.push_back(bits & -bits);
            labels.insert(labels.end(), blocks.begin(), blocks.end());
            const double macro = MacroLabels(rooted, metric, labels);
            const double anchored = ThreeBlockAnchoredPlan(
                rooted, metric, core, blocks);
            three_anchor_plan_exact =
                std::abs(macro - anchored) <= 1e-9;
            if (!three_anchor_plan_exact)
                std::cout << "three_anchor_plan_dp=" << macro
                          << " three_anchor_plan_price=" << anchored
                          << " core=" << core << '\n';
        }
        InstanceResult result;
        result.three_anchor_plan_exact = three_anchor_plan_exact;
        return result;
    }
    const int nonanchor_count = g - 1;
    const int subset_count = 1 << nonanchor_count;
    const int full = subset_count - 1;
    const int half = g / 2;
    const int high_profile_q = std::max(1, half - 1);
    struct MacroPlanCheck
    {
        int core = 0;
        int left = 0;
        int right = 0;
        double explicit_value = kInf;
    };
    std::vector<MacroPlanCheck> macro_plans;
    if (macro_plan_check)
        for (int core = 1; core < (1 << g); ++core)
        {
            if (Bits(core) != 4)
                continue;
            const int remaining = ((1 << g) - 1) ^ core;
            for (int left = remaining; left;
                 left = (left - 1) & remaining)
            {
                const int right = remaining ^ left;
                if (!right || left > right ||
                    Bits(left) > high_profile_q ||
                    Bits(right) > high_profile_q)
                    continue;
                macro_plans.push_back({core, left, right, kInf});
            }
        }
    std::vector<std::vector<Candidate>> fronts(subset_count);

    double exact = kInf;
    double pair_completion = kInf;
    double anchor_skyline_pair_completion = kInf;
    double low_core_completion = kInf;
    double generated_core_completion = kInf;
    double skyline_generated_core_completion = kInf;
    double rooted_core4_completion = kInf;
    long long connected_candidates = 0;
    long long pair_candidates = 0;
    const std::uint64_t edge_subset_count = std::uint64_t{1} << edges.size();
    std::vector<char> connected_by_edge(edge_subset_count);
    std::vector<std::uint64_t> vertices_by_edge(edge_subset_count);
    std::vector<double> cost_by_edge(edge_subset_count, kInf);
    std::vector<std::vector<double>> profile_by_edge(edge_subset_count);
    std::vector<std::pair<std::uint64_t, int>> pair_bases;
    std::vector<std::pair<std::uint64_t, int>> skyline_pair_bases;
    std::vector<std::pair<std::uint64_t, int>> anchor_pair_bases;
    std::vector<std::vector<std::vector<std::uint64_t>>> rooted_witnesses(
        1 << g, std::vector<std::vector<std::uint64_t>>(n));
    int root_star_root = 0;
    double root_star = kInf;
    auto GroupDistance = [&](int group, int root)
    {
        double result = kInf;
        for (int terminal : group_vertices[group])
            result = std::min(result, metric[terminal][root]);
        return result;
    };
    for (int root = 0; root < n; ++root)
    {
        double value = 0.0;
        for (int group = 0; group < g; ++group)
            value += GroupDistance(group, root);
        if (value < root_star)
        {
            root_star = value;
            root_star_root = root;
        }
    }
    int anchor_group = 0;
    for (int group = 1; group < g; ++group)
        if (GroupDistance(group, root_star_root) >
            GroupDistance(anchor_group, root_star_root))
            anchor_group = group;
    std::vector<std::vector<char>> anchor_skyline(1 << g,
                                                   std::vector<char>(n));
    if (!high_core4_only)
      for (int first = 0; first < g; ++first)
        for (int second = first + 1; second < g; ++second)
        {
            if (first == anchor_group || second == anchor_group)
                continue;
            const int pair = (1 << first) | (1 << second);
            std::vector<int> roots(n);
            std::iota(roots.begin(), roots.end(), 0);
            std::sort(roots.begin(), roots.end(), [&](int a, int b)
            {
                if (rooted[pair][a] != rooted[pair][b])
                    return rooted[pair][a] < rooted[pair][b];
                if (GroupDistance(anchor_group, a) !=
                    GroupDistance(anchor_group, b))
                    return GroupDistance(anchor_group, a) <
                           GroupDistance(anchor_group, b);
                return a < b;
            });
            double best_anchor = kInf;
            for (int root : roots)
                if (GroupDistance(anchor_group, root) < best_anchor)
                {
                    best_anchor = GroupDistance(anchor_group, root);
                    anchor_skyline[pair][root] = 1;
                }
        }
    for (std::uint64_t chosen = 0; chosen < edge_subset_count; ++chosen)
    {
        Dsu dsu(n);
        std::vector<char> included(n);
        double cost = 0.0;
        int first = -1;
        if (!chosen)
        {
            first = terminals[0];
            included[first] = 1;
        }
        for (size_t id = 0; id < edges.size(); ++id)
        {
            if (!(chosen & (std::uint64_t{1} << id)))
                continue;
            const Edge& edge = edges[id];
            included[edge.u] = included[edge.v] = 1;
            dsu.Unite(edge.u, edge.v);
            cost += edge.w;
            first = edge.u;
        }
        bool connected = first >= 0;
        for (int v = 0; v < n && connected; ++v)
            if (included[v] && dsu.Find(v) != dsu.Find(first))
                connected = false;
        if (!connected)
            continue;

        int actual_original = 0;
        for (int bit = 0; bit < g; ++bit)
            for (int terminal : group_vertices[bit])
                if (included[terminal])
                {
                    actual_original |= 1 << bit;
                    break;
                }
        if (actual_original == (1 << g) - 1)
            exact = std::min(exact, cost);

        connected_by_edge[chosen] = 1;
        cost_by_edge[chosen] = cost;
        std::uint64_t included_mask = 0;
        for (int v = 0; v < n; ++v)
            if (included[v])
                included_mask |= std::uint64_t{1} << v;
        vertices_by_edge[chosen] = included_mask;

        std::vector<double> full_profile(1 << g, kInf);
        full_profile[0] = 0.0;
        for (int mask = 1; mask < (1 << g); ++mask)
            for (int v = 0; v < n; ++v)
                if (included[v])
                    full_profile[mask] =
                        std::min(full_profile[mask], rooted[mask][v]);
        profile_by_edge[chosen] = full_profile;
        if (macro_plan_check)
            for (MacroPlanCheck& plan : macro_plans)
                if ((actual_original & plan.core) == plan.core)
                    plan.explicit_value = std::min(
                        plan.explicit_value,
                        cost + full_profile[plan.left] +
                            full_profile[plan.right]);
        if (high_core4_only)
            for (int declared = actual_original; declared;
                 declared = (declared - 1) & actual_original)
            {
                if (Bits(declared) != 4)
                    continue;
                bool rooted_core = false;
                for (int root = 0; root < n && !rooted_core; ++root)
                    rooted_core = included[root] &&
                        std::abs(cost - rooted[declared][root]) <= 1e-9;
                if (!rooted_core)
                    continue;
                const int remaining = ((1 << g) - 1) ^ declared;
                for (int left = remaining;; left = (left - 1) & remaining)
                {
                    const int right = remaining ^ left;
                    if (left <= right && Bits(left) <= high_profile_q &&
                        Bits(right) <= high_profile_q)
                        rooted_core4_completion = std::min(
                            rooted_core4_completion,
                            cost + full_profile[left] +
                                full_profile[right]);
                    if (!left)
                        break;
                }
            }
        const int quarter = (half + 1) / 2;
        for (int mask = actual_original; mask; mask = (mask - 1) & actual_original)
        {
            if (Bits(mask) > quarter)
                continue;
            for (int x = 0; x < n; ++x)
                if (included[x] && std::abs(cost - rooted[mask][x]) <= 1e-9)
                    rooted_witnesses[mask][x].push_back(chosen);
        }
        if (!high_core4_only)
        {
            const int core_size = g - 2 * quarter;
            for (int declared = actual_original;;
                 declared = (declared - 1) & actual_original)
            {
                if (Bits(declared) == core_size)
                {
                    const int remaining = ((1 << g) - 1) ^ declared;
                    for (int left = remaining;; left = (left - 1) & remaining)
                    {
                        const int right = remaining ^ left;
                        if (left <= right && Bits(left) <= quarter &&
                            Bits(right) <= quarter)
                            low_core_completion =
                                std::min(low_core_completion,
                                         cost + full_profile[left] +
                                             full_profile[right]);
                        if (!left)
                            break;
                    }
                }
                if (!declared)
                    break;
            }
        }
        for (int first = 0; first < g; ++first)
            for (int second = first + 1; second < g; ++second)
            {
                const int pair = (1 << first) | (1 << second);
                if ((actual_original & pair) != pair)
                    continue;
                bool rooted_pair_witness = false;
                bool anchor_skyline_witness = false;
                for (int v = 0; v < n; ++v)
                    if (included[v] && std::abs(cost - rooted[pair][v]) <= 1e-9)
                    {
                        rooted_pair_witness = true;
                        anchor_skyline_witness |= anchor_skyline[pair][v] != 0;
                    }
                if (!rooted_pair_witness)
                    continue;
                ++pair_candidates;
                pair_bases.push_back({chosen, pair});
                if (high_core4_only)
                    continue;
                if (anchor_skyline_witness)
                    skyline_pair_bases.push_back({chosen, pair});
                if (pair & (1 << anchor_group))
                    anchor_pair_bases.push_back({chosen, pair});
                const int remaining = ((1 << g) - 1) ^ pair;
                for (int left = remaining;; left = (left - 1) & remaining)
                {
                    const int right = remaining ^ left;
                    if (left <= right && Bits(left) <= half && Bits(right) <= half)
                    {
                        pair_completion =
                            std::min(pair_completion,
                                     cost + full_profile[left] + full_profile[right]);
                        if (anchor_skyline_witness)
                            anchor_skyline_pair_completion =
                                std::min(anchor_skyline_pair_completion,
                                         cost + full_profile[left] +
                                             full_profile[right]);
                    }
                    if (!left)
                        break;
                }
            }

        if (high_core4_only)
            continue;

        if (!included[terminals[0]])
            continue;
        int actual_covered = 0;
        for (int bit = 0; bit < nonanchor_count; ++bit)
            if (included[terminals[bit + 1]])
                actual_covered |= 1 << bit;
        Candidate base_candidate;
        base_candidate.cost = cost;
        base_candidate.profile.assign(subset_count, kInf);
        base_candidate.profile[0] = 0.0;
        for (int mask = 1; mask < subset_count; ++mask)
        {
            int original_mask = 0;
            for (int bits = mask; bits; bits &= bits - 1)
            {
                const int bit = FirstBit(bits & -bits);
                original_mask |= 1 << (bit + 1);
            }
            for (int v = 0; v < n; ++v)
                if (included[v])
                    base_candidate.profile[mask] =
                        std::min(base_candidate.profile[mask], rooted[original_mask][v]);
        }

        for (int declared = actual_covered;; declared = (declared - 1) & actual_covered)
        {
            if (Bits(declared) <= half - 1)
            {
                ++connected_candidates;
                Candidate candidate = base_candidate;
                const int remaining = full ^ declared;
                auto& front = fronts[declared];
                bool dominated = false;
                for (const Candidate& other : front)
                    if (Dominates(other, candidate, remaining))
                    {
                        dominated = true;
                        break;
                    }
                if (!dominated)
                {
                    front.erase(std::remove_if(
                        front.begin(), front.end(), [&](const Candidate& other)
                        {
                            return Dominates(candidate, other, remaining);
                        }), front.end());
                    front.push_back(std::move(candidate));
                }
            }
            if (!declared)
                break;
        }
    }

    struct GrowResult
    {
        double completion = kInf;
        long long states = 0;
        long long core_states = 0;
    };
    auto GrowCore = [&](const std::vector<std::pair<std::uint64_t, int>>& bases,
                        int quarter,
                        bool all_attachments = false,
                        int core_size_override = 0,
                        bool one_growth_block = false)
    {
        if (g < 5)
            return GrowResult{exact, 0, 0};
        GrowResult result;
        const int core_size = core_size_override > 0
            ? core_size_override
            : g - 2 * quarter;
        const int mask_count = 1 << g;
        std::vector<std::vector<char>> generated(
            edge_subset_count, std::vector<char>(mask_count));
        std::deque<std::pair<std::uint64_t, int>> queue;
        for (const auto [edge_mask, pair] : bases)
            if (Bits(pair) <= core_size && !generated[edge_mask][pair])
            {
                generated[edge_mask][pair] = 1;
                queue.push_back({edge_mask, pair});
                ++result.states;
            }
        while (!queue.empty())
        {
            const auto [tree_edges, declared] = queue.front();
            queue.pop_front();
            if (Bits(declared) == core_size)
                continue;
            const int available = ((1 << g) - 1) ^ declared;
            const int declared_size = Bits(declared);
            const int next_block_size = declared_size == 2
                ? (one_growth_block
                       ? core_size - 2
                       : (core_size - 2 + 1) / 2)
                : core_size - declared_size;
            for (int block = available; block; block = (block - 1) & available)
            {
                if (Bits(block) != next_block_size)
                    continue;
                double best_attachment = kInf;
                for (int x = 0; x < n; ++x)
                    if (vertices_by_edge[tree_edges] & (std::uint64_t{1} << x))
                        best_attachment =
                            std::min(best_attachment, rooted[block][x]);
                for (int x = 0; x < n; ++x)
                {
                    if (!(vertices_by_edge[tree_edges] & (std::uint64_t{1} << x)))
                        continue;
                    if (!all_attachments &&
                        rooted[block][x] > best_attachment + 1e-9)
                        continue;
                    if (rooted[block][x] == 0.0)
                    {
                        const int next_declared = declared | block;
                        if (!generated[tree_edges][next_declared])
                        {
                            generated[tree_edges][next_declared] = 1;
                            queue.push_back({tree_edges, next_declared});
                            ++result.states;
                        }
                    }
                    for (std::uint64_t witness : rooted_witnesses[block][x])
                    {
                        const std::uint64_t next_edges = tree_edges | witness;
                        const int next_declared = declared | block;
                        if (!generated[next_edges][next_declared])
                        {
                            generated[next_edges][next_declared] = 1;
                            queue.push_back({next_edges, next_declared});
                            ++result.states;
                        }
                    }
                }
            }
        }
        for (std::uint64_t tree_edges = 0; tree_edges < edge_subset_count;
             ++tree_edges)
        {
            if (!connected_by_edge[tree_edges])
                continue;
            for (int declared = 0; declared < mask_count; ++declared)
            {
                if (!generated[tree_edges][declared] ||
                    Bits(declared) != core_size)
                    continue;
                ++result.core_states;
                const int remaining = (mask_count - 1) ^ declared;
                for (int left = remaining;; left = (left - 1) & remaining)
                {
                    const int right = remaining ^ left;
                    if (left <= right && Bits(left) <= quarter &&
                        Bits(right) <= quarter)
                        result.completion = std::min(
                            result.completion,
                            cost_by_edge[tree_edges] +
                                profile_by_edge[tree_edges][left] +
                                profile_by_edge[tree_edges][right]);
                    if (!left)
                        break;
                }
            }
        }
        return result;
    };
    const int quarter = (half + 1) / 2;
    const GrowResult skipped{exact, 0, 0};
    const GrowResult generated_result = high_core4_only
        ? skipped
        : GrowCore(pair_bases, quarter);
    const GrowResult skyline_generated_result = high_core4_only
        ? skipped
        : GrowCore(skyline_pair_bases, quarter);
    const GrowResult anchor_pair_generated_result = high_core4_only
        ? skipped
        : GrowCore(anchor_pair_bases, quarter);
    const int high_q = std::max(1, half - 1);
    const int middle_q = std::max(quarter, high_q - 1);
    const GrowResult middle_q_generated_result = high_core4_only
        ? skipped
        : GrowCore(pair_bases, middle_q);
    const GrowResult high_q_generated_result = high_core4_only
        ? skipped
        : GrowCore(pair_bases, high_q);
    const GrowResult high_q_all_attachment_result = high_core4_only
        ? skipped
        : GrowCore(pair_bases, high_q, true);
    const GrowResult high_q_intermediate_result = high_core4_only
        ? skipped
        : GrowCore(pair_bases, high_q, false, g - 2 * high_q + 1);
    const int intermediate_core_size = g - 2 * high_q + 1;
    const GrowResult high_q_intermediate_block_result =
        intermediate_core_size - 2 <= high_q
        ? GrowCore(pair_bases, high_q, false, intermediate_core_size, true)
        : skipped;
    const GrowResult high_q_intermediate_block_all_result =
        high_core4_only && intermediate_core_size - 2 <= high_q
        ? GrowCore(pair_bases, high_q, true, intermediate_core_size, true)
        : skipped;
    generated_core_completion = generated_result.completion;
    skyline_generated_core_completion = skyline_generated_result.completion;
    const double anchor_pair_generated_core_completion =
        anchor_pair_generated_result.completion;

    double completion = kInf;
    long long pareto_candidates = 0;
    int maximum_front = 0;
    for (int covered = 0; covered < subset_count; ++covered)
    {
        pareto_candidates += fronts[covered].size();
        maximum_front = std::max(maximum_front, static_cast<int>(fronts[covered].size()));
        const int remaining = full ^ covered;
        for (const Candidate& candidate : fronts[covered])
        {
            for (int left = remaining;; left = (left - 1) & remaining)
            {
                const int right = remaining ^ left;
                if (left <= right && Bits(left) <= half && Bits(right) <= half)
                    completion = std::min(
                        completion,
                        candidate.cost + candidate.profile[left] + candidate.profile[right]);
                if (!left)
                    break;
            }
        }
    }
    const bool is_exact =
        high_core4_only || std::abs(exact - completion) <= 1e-9;
    const bool pair_is_exact =
        high_core4_only || std::abs(exact - pair_completion) <= 1e-9;
    const bool anchor_pair_is_exact =
        high_core4_only ||
        std::abs(exact - anchor_skyline_pair_completion) <= 1e-9;
    const bool low_core_is_exact =
        high_core4_only || std::abs(exact - low_core_completion) <= 1e-9;
    const bool generated_core_is_exact =
        std::abs(exact - generated_core_completion) <= 1e-9;
    const bool high_q_generated_core_is_exact =
        std::abs(exact - high_q_generated_result.completion) <= 1e-9;
    const bool middle_q_generated_core_is_exact =
        std::abs(exact - middle_q_generated_result.completion) <= 1e-9;
    const bool high_q_all_attachment_is_exact =
        std::abs(exact - high_q_all_attachment_result.completion) <= 1e-9;
    const bool high_q_intermediate_is_exact =
        std::abs(exact - high_q_intermediate_result.completion) <= 1e-9;
    const bool high_q_intermediate_block_is_exact =
        std::abs(exact - high_q_intermediate_block_result.completion) <= 1e-9;
    const bool high_q_intermediate_block_all_is_exact =
        std::abs(exact -
                 high_q_intermediate_block_all_result.completion) <= 1e-9;
    const bool rooted_core4_is_exact =
        !high_core4_only ||
        std::abs(exact - rooted_core4_completion) <= 1e-9;
    static bool reported_rooted_core4_mismatch = false;
    if (!rooted_core4_is_exact && !reported_rooted_core4_mismatch)
    {
        reported_rooted_core4_mismatch = true;
        std::cout << "rooted_core4_exact=" << exact
                  << " rooted_core4_completion=" << rooted_core4_completion
                  << " g=" << g << " terminals";
        for (int terminal : terminals)
            std::cout << ' ' << terminal;
        std::cout << " edges";
        for (const Edge& edge : edges)
            std::cout << ' ' << '(' << edge.u << ',' << edge.v << ','
                      << edge.w << ')';
        if (multi_terminal_groups)
        {
            std::cout << " groups";
            for (const auto& group : group_vertices)
                std::cout << " [" << group[0] << ',' << group[1] << ']';
        }
        std::cout << '\n';
    }
    bool macro_plan_exact = true;
    if (macro_plan_check)
        for (const MacroPlanCheck& plan : macro_plans)
        {
            const double macro = MacroPlan(
                rooted, metric, plan.core, plan.left, plan.right);
            const double anchored = AnchoredMacroPlan(
                rooted, metric, plan.core, plan.left, plan.right);
            const double first_order = FirstOrderAnchoredMacroPlan(
                rooted, metric, plan.core, plan.left, plan.right);
            if (std::abs(macro - plan.explicit_value) <= 1e-9 &&
                std::abs(anchored - macro) <= 1e-9 &&
                std::abs(first_order - macro) <= 1e-9)
                continue;
            macro_plan_exact = false;
            std::cout << "macro_plan_explicit=" << plan.explicit_value
                      << " macro_plan_dp=" << macro
                      << " macro_plan_anchored=" << anchored
                      << " macro_plan_first_order=" << first_order
                      << " core=" << plan.core
                      << " left=" << plan.left
                      << " right=" << plan.right << '\n';
            break;
        }
    const bool skyline_generated_core_is_exact =
        std::abs(exact - skyline_generated_core_completion) <= 1e-9;
    const bool anchor_pair_generated_core_is_exact =
        std::abs(exact - anchor_pair_generated_core_completion) <= 1e-9;
    if (!is_exact)
    {
        std::cout << "exact=" << exact << " completion=" << completion << " terminals";
        for (int terminal : terminals)
            std::cout << ' ' << terminal;
        std::cout << " edges";
        for (const Edge& edge : edges)
            std::cout << " (" << edge.u << ',' << edge.v << ',' << edge.w << ')';
        std::cout << '\n';
    }
    if (!pair_is_exact)
    {
        std::cout << "pair_exact=" << exact
                  << " pair_completion=" << pair_completion << " terminals";
        for (int terminal : terminals)
            std::cout << ' ' << terminal;
        std::cout << " edges";
        for (const Edge& edge : edges)
            std::cout << ' ' << '(' << edge.u << ',' << edge.v << ',' << edge.w << ')';
        std::cout << '\n';
    }
    static bool reported_anchor_mismatch = false;
    if (!anchor_pair_is_exact && !reported_anchor_mismatch)
    {
        reported_anchor_mismatch = true;
        std::cout << "anchor_pair_exact=" << exact
                  << " anchor_pair_completion=" << anchor_skyline_pair_completion
                  << " anchor_group=" << anchor_group << " terminals";
        for (int terminal : terminals)
            std::cout << ' ' << terminal;
        std::cout << " edges";
        for (const Edge& edge : edges)
            std::cout << ' ' << '(' << edge.u << ',' << edge.v << ',' << edge.w << ')';
        std::cout << '\n';
    }
    if (!low_core_is_exact)
        std::cout << "low_core_exact=" << exact
                  << " low_core_completion=" << low_core_completion
                  << " g=" << g << " quarter=" << ((half + 1) / 2) << '\n';
    if (!generated_core_is_exact)
        std::cout << "generated_core_exact=" << exact
                  << " generated_core_completion=" << generated_core_completion
                  << " g=" << g << '\n';
    static bool reported_middle_q_mismatch = false;
    if (!middle_q_generated_core_is_exact && !reported_middle_q_mismatch)
    {
        reported_middle_q_mismatch = true;
        std::cout << "middle_q_generated_core_exact=" << exact
                  << " middle_q_generated_core_completion="
                  << middle_q_generated_result.completion
                  << " g=" << g << " q=" << middle_q << " terminals";
        for (int terminal : terminals)
            std::cout << ' ' << terminal;
        std::cout << " edges";
        for (const Edge& edge : edges)
            std::cout << ' ' << '(' << edge.u << ',' << edge.v << ','
                      << edge.w << ')';
        std::cout << '\n';
    }
    static bool reported_high_q_mismatch = false;
    if (!high_q_generated_core_is_exact && !reported_high_q_mismatch)
    {
        reported_high_q_mismatch = true;
        std::cout << "high_q_generated_core_exact=" << exact
                  << " high_q_generated_core_completion="
                  << high_q_generated_result.completion
                  << " g=" << g << " q=" << high_q << " terminals";
        for (int terminal : terminals)
            std::cout << ' ' << terminal;
        std::cout << " edges";
        for (const Edge& edge : edges)
            std::cout << ' ' << '(' << edge.u << ',' << edge.v << ','
                      << edge.w << ')';
        std::cout << '\n';
    }
    static bool reported_high_q_all_mismatch = false;
    if (!high_q_all_attachment_is_exact && !reported_high_q_all_mismatch)
    {
        reported_high_q_all_mismatch = true;
        std::cout << "high_q_all_attachment_exact=" << exact
                  << " high_q_all_attachment_completion="
                  << high_q_all_attachment_result.completion
                  << " g=" << g << " q=" << high_q << " terminals";
        for (int terminal : terminals)
            std::cout << ' ' << terminal;
        std::cout << " edges";
        for (const Edge& edge : edges)
            std::cout << ' ' << '(' << edge.u << ',' << edge.v << ','
                      << edge.w << ')';
        std::cout << '\n';
    }
    static bool reported_high_q_intermediate_mismatch = false;
    if (!high_q_intermediate_is_exact &&
        !reported_high_q_intermediate_mismatch)
    {
        reported_high_q_intermediate_mismatch = true;
        std::cout << "high_q_intermediate_core_exact=" << exact
                  << " high_q_intermediate_core_completion="
                  << high_q_intermediate_result.completion
                  << " g=" << g << " q=" << high_q << " core="
                  << (g - 2 * high_q + 1) << " terminals";
        for (int terminal : terminals)
            std::cout << ' ' << terminal;
        std::cout << " edges";
        for (const Edge& edge : edges)
            std::cout << ' ' << '(' << edge.u << ',' << edge.v << ','
                      << edge.w << ')';
        std::cout << '\n';
    }
    static bool reported_high_q_intermediate_block_mismatch = false;
    if (!high_q_intermediate_block_is_exact &&
        !reported_high_q_intermediate_block_mismatch)
    {
        reported_high_q_intermediate_block_mismatch = true;
        std::cout << "high_q_intermediate_block_exact=" << exact
                  << " high_q_intermediate_block_completion="
                  << high_q_intermediate_block_result.completion
                  << " all_attachment_completion="
                  << high_q_intermediate_block_all_result.completion
                  << " g=" << g << " q=" << high_q << " core="
                  << (g - 2 * high_q + 1) << " terminals";
        for (int terminal : terminals)
            std::cout << ' ' << terminal;
        std::cout << " edges";
        for (const Edge& edge : edges)
            std::cout << ' ' << '(' << edge.u << ',' << edge.v << ','
                      << edge.w << ')';
        if (multi_terminal_groups)
        {
            std::cout << " groups";
            for (const auto& group : group_vertices)
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
        }
        std::cout << '\n';
    }
    static bool reported_skyline_core_mismatch = false;
    if (!skyline_generated_core_is_exact && !reported_skyline_core_mismatch)
    {
        reported_skyline_core_mismatch = true;
        std::cout << "skyline_generated_core_exact=" << exact
                  << " skyline_generated_core_completion="
                  << skyline_generated_core_completion << " g=" << g << '\n';
    }
    static bool reported_anchor_core_mismatch = false;
    if (!anchor_pair_generated_core_is_exact && !reported_anchor_core_mismatch)
    {
        reported_anchor_core_mismatch = true;
        std::cout << "anchor_pair_generated_core_exact=" << exact
                  << " anchor_pair_generated_core_completion="
                  << anchor_pair_generated_core_completion << " g=" << g << '\n';
    }
    return {is_exact, pair_is_exact, anchor_pair_is_exact, low_core_is_exact,
            generated_core_is_exact, middle_q_generated_core_is_exact,
            high_q_generated_core_is_exact,
            high_q_all_attachment_is_exact,
            high_q_intermediate_is_exact,
            high_q_intermediate_block_is_exact,
            high_q_intermediate_block_all_is_exact,
            rooted_core4_is_exact,
            skyline_generated_core_is_exact,
            anchor_pair_generated_core_is_exact,
            connected_candidates, pair_candidates,
            generated_result.states, generated_result.core_states,
            skyline_generated_result.states, skyline_generated_result.core_states,
            pareto_candidates, maximum_front, macro_plan_exact,
            cherry_plan_exact, three_anchor_plan_exact, false, false};
}
}  // namespace

int main(int argc, char** argv)
{
    const std::uint64_t seed = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1;
    const int iterations = argc > 2 ? std::atoi(argv[2]) : 1000;
    const int max_n = argc > 3 ? std::atoi(argv[3]) : 8;
    const int max_g = argc > 4 ? std::atoi(argv[4]) : 7;
    const int max_edges = argc > 5 ? std::atoi(argv[5]) : 11;
    const int min_n = argc > 6 ? std::atoi(argv[6]) : 5;
    const int min_g = argc > 7 ? std::atoi(argv[7]) : 3;
    const std::string mode = argc > 8 ? argv[8] : "all";
    const bool high_core4_only =
        mode == "high-core4" || mode == "high-core4-groups" ||
        mode == "rooted-core4-groups";
    const bool multi_terminal_groups =
        mode == "high-core4-groups" || mode == "rooted-core4-groups" ||
        mode == "macro-plan-groups" || mode == "cherry-plan-groups" ||
        mode == "three-anchor-plan-groups" ||
        mode == "nested-pendant-family-groups" ||
        mode == "soft-pendant-family-groups";
    const bool macro_plan_check =
        mode == "macro-plan" || mode == "macro-plan-groups";
    const bool cherry_plan_check =
        mode == "cherry-plan" || mode == "cherry-plan-groups";
    const bool three_anchor_plan_check =
        mode == "three-anchor-plan" ||
        mode == "three-anchor-plan-groups";
    const bool nested_pendant_family_check =
        mode == "nested-pendant-family" ||
        mode == "nested-pendant-family-groups";
    const bool soft_pendant_family_check =
        mode == "soft-pendant-family" ||
        mode == "soft-pendant-family-groups";
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> n_dist(min_n, max_n);

    long long total_connected = 0;
    long long total_pair = 0;
    long long total_generated = 0;
    long long total_generated_core = 0;
    long long total_skyline_generated = 0;
    long long total_skyline_generated_core = 0;
    int anchor_skyline_exact = 0;
    int pair_block_exact = 0;
    int low_core_exact = 0;
    int generated_core_exact = 0;
    int middle_q_generated_core_exact = 0;
    int high_q_generated_core_exact = 0;
    int high_q_all_attachment_exact = 0;
    int high_q_intermediate_core_exact = 0;
    int high_q_intermediate_block_exact = 0;
    int high_q_intermediate_block_all_exact = 0;
    int rooted_core4_exact = 0;
    int skyline_generated_core_exact = 0;
    int anchor_pair_generated_core_exact = 0;
    int macro_plan_exact = 0;
    int cherry_plan_exact = 0;
    int three_anchor_plan_exact = 0;
    int nested_pendant_family_exact = 0;
    int soft_pendant_family_exact = 0;
    long long total_pareto = 0;
    int maximum_front = 0;
    for (int iteration = 1; iteration <= iterations; ++iteration)
    {
        const int n = n_dist(rng);
        std::uniform_int_distribution<int> g_dist(min_g, std::min(max_g, n));
        const int g = g_dist(rng);
        const InstanceResult result =
            RunInstance(rng, n, g, max_edges, high_core4_only,
                        multi_terminal_groups, macro_plan_check,
                        cherry_plan_check, three_anchor_plan_check,
                        nested_pendant_family_check,
                        soft_pendant_family_check);
        const bool selected_exact = mode == "rooted-core4-groups"
            ? result.rooted_core4_exact
            : macro_plan_check
            ? result.macro_plan_exact
            : cherry_plan_check
            ? result.cherry_plan_exact
            : three_anchor_plan_check
            ? result.three_anchor_plan_exact
            : nested_pendant_family_check
            ? result.nested_pendant_family_exact
            : soft_pendant_family_check
            ? result.soft_pendant_family_exact
            : high_core4_only
            ? result.high_q_intermediate_block_exact
            : result.exact && result.low_core_exact &&
                  result.generated_core_exact;
        if (!selected_exact)
        {
            std::cout << "MISMATCH iteration=" << iteration << " n=" << n << " g=" << g
                      << '\n';
            return 1;
        }
        total_connected += result.connected_candidates;
        total_pair += result.pair_candidates;
        total_generated += result.generated_states;
        total_generated_core += result.generated_core_states;
        total_skyline_generated += result.skyline_generated_states;
        total_skyline_generated_core += result.skyline_generated_core_states;
        anchor_skyline_exact += result.anchor_skyline_pair_exact;
        pair_block_exact += result.pair_block_exact;
        low_core_exact += result.low_core_exact;
        generated_core_exact += result.generated_core_exact;
        middle_q_generated_core_exact +=
            result.middle_q_generated_core_exact;
        high_q_generated_core_exact += result.high_q_generated_core_exact;
        high_q_all_attachment_exact +=
            result.high_q_all_attachment_exact;
        high_q_intermediate_core_exact +=
            result.high_q_intermediate_core_exact;
        high_q_intermediate_block_exact +=
            result.high_q_intermediate_block_exact;
        high_q_intermediate_block_all_exact +=
            result.high_q_intermediate_block_all_exact;
        rooted_core4_exact += result.rooted_core4_exact;
        skyline_generated_core_exact += result.skyline_generated_core_exact;
        anchor_pair_generated_core_exact +=
            result.anchor_pair_generated_core_exact;
        macro_plan_exact += result.macro_plan_exact;
        cherry_plan_exact += result.cherry_plan_exact;
        three_anchor_plan_exact += result.three_anchor_plan_exact;
        nested_pendant_family_exact +=
            result.nested_pendant_family_exact;
        soft_pendant_family_exact += result.soft_pendant_family_exact;
        total_pareto += result.pareto_candidates;
        maximum_front = std::max(maximum_front, result.maximum_front);
        if (iteration % 100 == 0)
            std::cout << "ok " << iteration << '\n';
    }
    std::cout << std::fixed << std::setprecision(6)
              << "ALL_OK seed=" << seed
              << " iterations=" << iterations
              << " max_n=" << max_n
              << " max_g=" << max_g
              << " max_edges=" << max_edges
              << " min_n=" << min_n
              << " min_g=" << min_g
              << " mode=" << mode
              << " connected=" << total_connected
              << " pair_candidates=" << total_pair
              << " generated_states=" << total_generated
              << " generated_core_states=" << total_generated_core
              << " skyline_generated_states=" << total_skyline_generated
              << " skyline_generated_core_states="
              << total_skyline_generated_core
              << " anchor_skyline_exact=" << anchor_skyline_exact
              << " pair_block_exact=" << pair_block_exact
              << " low_core_exact=" << low_core_exact
              << " generated_core_exact=" << generated_core_exact
              << " middle_q_generated_core_exact="
              << middle_q_generated_core_exact
              << " high_q_generated_core_exact="
              << high_q_generated_core_exact
              << " high_q_all_attachment_exact="
              << high_q_all_attachment_exact
              << " high_q_intermediate_core_exact="
              << high_q_intermediate_core_exact
              << " high_q_intermediate_block_exact="
              << high_q_intermediate_block_exact
              << " high_q_intermediate_block_all_exact="
              << high_q_intermediate_block_all_exact
              << " rooted_core4_exact=" << rooted_core4_exact
              << " skyline_generated_core_exact="
              << skyline_generated_core_exact
              << " anchor_pair_generated_core_exact="
              << anchor_pair_generated_core_exact
              << " macro_plan_exact=" << macro_plan_exact
              << " cherry_plan_exact=" << cherry_plan_exact
              << " three_anchor_plan_exact="
              << three_anchor_plan_exact
              << " nested_pendant_family_exact="
              << nested_pendant_family_exact
              << " soft_pendant_family_exact="
              << soft_pendant_family_exact
              << " pareto=" << total_pareto
              << " ratio="
              << (total_connected ? static_cast<double>(total_pareto) / total_connected : 0.0)
              << " max_front=" << maximum_front << '\n';
    return 0;
}
