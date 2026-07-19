#include "pruned_dp_solver.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace gst::methods::pruned_dp
{
namespace
{
constexpr int kNoWitness = -1;

int CountBits(int mask)
{
#if defined(_MSC_VER)
    return static_cast<int>(__popcnt(static_cast<unsigned int>(mask)));
#else
    return __builtin_popcount(static_cast<unsigned int>(mask));
#endif
}

struct StateEntry
{
    bool present = false;
    bool open = false;
    bool closed = false;
    double best_cost = fp::kInf;
    double open_cost = fp::kInf;
    double open_priority = fp::kInf;
    double settled_cost = fp::kInf;
    int settled_witness = kNoWitness;
    std::uint64_t version = 0;
};

class StateStore
{
public:
    StateStore(StateStorage storage, int n, int subset_count)
        : storage_(storage), n_(n), subset_count_(subset_count)
    {
        if (storage_ == StateStorage::Dense)
        {
            const std::size_t rows = static_cast<std::size_t>(subset_count_);
            const std::size_t width = static_cast<std::size_t>(n_) + 1;
            if (rows > std::numeric_limits<std::size_t>::max() / width)
                throw std::runtime_error("PrunedDP dense state table is too large.");
            dense_.resize(rows * width);
        }
    }

    StateEntry* Find(int vertex, int mask)
    {
        if (storage_ == StateStorage::Dense)
        {
            StateEntry& entry = dense_[DenseIndex(vertex, mask)];
            return entry.present ? &entry : nullptr;
        }
        const auto it = sparse_.find(Key(vertex, mask));
        return it == sparse_.end() ? nullptr : &it->second;
    }

    StateEntry& Get(int vertex, int mask)
    {
        if (storage_ == StateStorage::Dense)
        {
            StateEntry& entry = dense_[DenseIndex(vertex, mask)];
            if (!entry.present)
            {
                entry.present = true;
                ++discovered_;
            }
            return entry;
        }
        auto [it, inserted] = sparse_.try_emplace(Key(vertex, mask));
        if (inserted)
        {
            it->second.present = true;
            ++discovered_;
        }
        return it->second;
    }

    long long Discovered() const { return discovered_; }

private:
    std::size_t DenseIndex(int vertex, int mask) const
    {
        return static_cast<std::size_t>(mask) * (static_cast<std::size_t>(n_) + 1) +
               static_cast<std::size_t>(vertex);
    }

    static std::uint64_t Key(int vertex, int mask)
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(mask)) << 32) |
               static_cast<std::uint32_t>(vertex);
    }

    StateStorage storage_;
    int n_ = 0;
    int subset_count_ = 0;
    long long discovered_ = 0;
    std::vector<StateEntry> dense_;
    std::unordered_map<std::uint64_t, StateEntry> sparse_;
};

enum class WitnessKind
{
    Seed,
    Edge,
    Merge,
};

struct Witness
{
    WitnessKind kind = WitnessKind::Seed;
    int left = kNoWitness;
    int right = kNoWitness;
    int edge_id = -1;
};

struct WitnessSpec
{
    WitnessKind kind = WitnessKind::Seed;
    int left = kNoWitness;
    int right = kNoWitness;
    int edge_id = -1;
};

struct QueueNode
{
    int vertex = 0;
    int mask = 0;
    double cost = 0.0;
    double priority = 0.0;
    int witness = kNoWitness;
    std::uint64_t version = 0;

    bool operator<(const QueueNode& other) const
    {
        if (priority != other.priority)
            return priority > other.priority;
        return cost > other.cost;
    }
};

struct RouteNode
{
    int start = 0;
    int end = 0;
    int mask = 0;
    double cost = 0.0;

    bool operator<(const RouteNode& other) const { return cost > other.cost; }
};

class DisjointSet
{
public:
    explicit DisjointSet(int size) : parent_(size), rank_(size)
    {
        for (int i = 0; i < size; ++i)
            parent_[i] = i;
    }

    int Find(int x)
    {
        while (parent_[x] != x)
        {
            parent_[x] = parent_[parent_[x]];
            x = parent_[x];
        }
        return x;
    }

    bool Unite(int left, int right)
    {
        left = Find(left);
        right = Find(right);
        if (left == right)
            return false;
        if (rank_[left] < rank_[right])
            std::swap(left, right);
        parent_[right] = left;
        if (rank_[left] == rank_[right])
            ++rank_[left];
        return true;
    }

private:
    std::vector<int> parent_;
    std::vector<unsigned char> rank_;
};

struct SolverContext
{
    const Graph* graph = nullptr;
    const Query* query = nullptr;
    PrunedDpOptions options;
    int n = 0;
    int group_count = 0;
    int full_mask = 0;
    int subset_count = 0;

    std::vector<std::vector<double>> group_distance;
    std::vector<std::vector<int>> next_vertex;
    std::vector<std::vector<int>> next_edge;
    std::vector<std::vector<double>> group_metric;
    std::vector<double> route_pair;
    std::vector<double> route_single;

    std::unique_ptr<StateStore> states;
    std::priority_queue<QueueNode> queue;
    std::vector<Witness> witnesses;
    std::vector<int> witness_epoch;
    std::vector<int> edge_epoch;
    std::vector<int> mst_local_index;
    int current_witness_epoch = 0;
    int current_edge_epoch = 0;

    double best = fp::kInf;
    PrunedDpStats stats;
};

std::size_t RoutePairIndex(const SolverContext& ctx, int start, int end, int mask)
{
    return (static_cast<std::size_t>(start) * ctx.group_count + end) *
               ctx.subset_count +
           mask;
}

std::size_t RouteSingleIndex(const SolverContext& ctx, int start, int mask)
{
    return static_cast<std::size_t>(start) * ctx.subset_count + mask;
}

double RoutePair(const SolverContext& ctx, int start, int end, int mask)
{
    return ctx.route_pair[RoutePairIndex(ctx, start, end, mask)];
}

double RouteSingle(const SolverContext& ctx, int start, int mask)
{
    return ctx.route_single[RouteSingleIndex(ctx, start, mask)];
}

void BuildGroupDistances(SolverContext& ctx)
{
    using Item = std::pair<double, int>;
    ctx.group_distance.assign(
        ctx.group_count, std::vector<double>(ctx.n + 1, fp::kInf));
    ctx.next_vertex.assign(ctx.group_count, std::vector<int>(ctx.n + 1));
    ctx.next_edge.assign(ctx.group_count, std::vector<int>(ctx.n + 1, -1));

    for (int group = 0; group < ctx.group_count; ++group)
    {
        auto& distance = ctx.group_distance[group];
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap;
        for (int terminal : ctx.query->groups[group])
        {
            if (distance[terminal] == 0.0)
                continue;
            distance[terminal] = 0.0;
            heap.push({0.0, terminal});
        }
        while (!heap.empty())
        {
            const auto [value, vertex] = heap.top();
            heap.pop();
            if (value != distance[vertex])
                continue;
            for (const AdjEdge& edge : ctx.graph->adj[vertex])
            {
                const double next = value + edge.w;
                if (next < distance[edge.to])
                {
                    distance[edge.to] = next;
                    ctx.next_vertex[group][edge.to] = vertex;
                    ctx.next_edge[group][edge.to] = edge.edge_id;
                    heap.push({next, edge.to});
                }
            }
        }
    }

    ctx.group_metric.assign(
        ctx.group_count, std::vector<double>(ctx.group_count, fp::kInf));
    for (int left = 0; left < ctx.group_count; ++left)
    {
        for (int right = 0; right < ctx.group_count; ++right)
        {
            for (int terminal : ctx.query->groups[right])
            {
                ctx.group_metric[left][right] =
                    std::min(ctx.group_metric[left][right],
                             ctx.group_distance[left][terminal]);
            }
        }
    }
}

void BuildAllPaths(SolverContext& ctx)
{
    const std::size_t pair_size = static_cast<std::size_t>(ctx.group_count) *
                                  ctx.group_count * ctx.subset_count;
    ctx.route_pair.assign(pair_size, fp::kInf);
    ctx.route_single.assign(
        static_cast<std::size_t>(ctx.group_count) * ctx.subset_count, fp::kInf);
    std::priority_queue<RouteNode> queue;

    for (int group = 0; group < ctx.group_count; ++group)
    {
        const int mask = 1 << group;
        ctx.route_pair[RoutePairIndex(ctx, group, group, mask)] = 0.0;
        queue.push({group, group, mask, 0.0});
    }

    while (!queue.empty())
    {
        const RouteNode current = queue.top();
        queue.pop();
        if (current.cost != RoutePair(
                                ctx, current.start, current.end, current.mask))
            continue;
        double& single =
            ctx.route_single[RouteSingleIndex(ctx, current.start, current.mask)];
        single = std::min(single, current.cost);

        int remaining = ctx.full_mask ^ current.mask;
        while (remaining)
        {
            const int bit = remaining & -remaining;
            remaining ^= bit;
            const int next_group = CountBits(bit - 1);
            const int next_mask = current.mask | bit;
            const double next_cost =
                current.cost + ctx.group_metric[current.end][next_group];
            double& stored = ctx.route_pair[RoutePairIndex(
                ctx, current.start, next_group, next_mask)];
            if (next_cost < stored)
            {
                stored = next_cost;
                queue.push({current.start, next_group, next_mask, next_cost});
            }
        }
    }
}

double RawPriority(const SolverContext& ctx, int vertex, int mask, double cost)
{
    const int remaining = ctx.full_mask ^ mask;
    if (!remaining)
        return cost;

    double one_label = 0.0;
    double nearest = fp::kInf;
    for (int bits = remaining; bits; bits &= bits - 1)
    {
        const int bit = bits & -bits;
        const int group = CountBits(bit - 1);
        one_label = std::max(one_label, ctx.group_distance[group][vertex]);
        nearest = std::min(nearest, ctx.group_distance[group][vertex]);
    }

    double tour_one = fp::kInf;
    double tour_two = 0.0;
    for (int left_bits = remaining; left_bits; left_bits &= left_bits - 1)
    {
        const int left_bit = left_bits & -left_bits;
        const int left = CountBits(left_bit - 1);
        for (int right_bits = remaining; right_bits; right_bits &= right_bits - 1)
        {
            const int right_bit = right_bits & -right_bits;
            const int right = CountBits(right_bit - 1);
            tour_one = std::min(
                tour_one,
                ctx.group_distance[left][vertex] +
                    RoutePair(ctx, left, right, remaining) +
                    ctx.group_distance[right][vertex]);
        }
        tour_two = std::max(
            tour_two,
            ctx.group_distance[left][vertex] +
                RouteSingle(ctx, left, remaining) + nearest);
    }
    const double heuristic =
        std::max(one_label, std::max(tour_one * 0.5, tour_two * 0.5));
    return cost + heuristic;
}

int MaterializeWitness(SolverContext& ctx, const WitnessSpec& spec)
{
    if (!ctx.options.use_mst_upper_bound)
        return kNoWitness;
    const int id = static_cast<int>(ctx.witnesses.size());
    ctx.witnesses.push_back({spec.kind, spec.left, spec.right, spec.edge_id});
    ctx.witness_epoch.push_back(0);
    return id;
}

void ResetEpoch(std::vector<int>& values, int& epoch)
{
    if (epoch == std::numeric_limits<int>::max())
    {
        std::fill(values.begin(), values.end(), 0);
        epoch = 1;
    }
    else
    {
        ++epoch;
    }
}

double BuildMstUpper(SolverContext& ctx, const QueueNode& state)
{
    ++ctx.stats.mst_calls;
    ResetEpoch(ctx.witness_epoch, ctx.current_witness_epoch);
    ResetEpoch(ctx.edge_epoch, ctx.current_edge_epoch);
    std::vector<int> selected_edges;
    std::vector<int> stack;
    if (state.witness != kNoWitness)
        stack.push_back(state.witness);

    auto AddEdge = [&](int edge_id)
    {
        if (edge_id < 0 || ctx.edge_epoch[edge_id] == ctx.current_edge_epoch)
            return;
        ctx.edge_epoch[edge_id] = ctx.current_edge_epoch;
        selected_edges.push_back(edge_id);
    };

    while (!stack.empty())
    {
        const int witness_id = stack.back();
        stack.pop_back();
        if (ctx.witness_epoch[witness_id] == ctx.current_witness_epoch)
            continue;
        ctx.witness_epoch[witness_id] = ctx.current_witness_epoch;
        const Witness& witness = ctx.witnesses[witness_id];
        if (witness.kind == WitnessKind::Edge)
        {
            AddEdge(witness.edge_id);
            stack.push_back(witness.left);
        }
        else if (witness.kind == WitnessKind::Merge)
        {
            stack.push_back(witness.left);
            stack.push_back(witness.right);
        }
    }

    int remaining = ctx.full_mask ^ state.mask;
    while (remaining)
    {
        const int bit = remaining & -remaining;
        remaining ^= bit;
        const int group = CountBits(bit - 1);
        int vertex = state.vertex;
        int steps = 0;
        while (ctx.next_edge[group][vertex] >= 0)
        {
            AddEdge(ctx.next_edge[group][vertex]);
            vertex = ctx.next_vertex[group][vertex];
            if (++steps > ctx.n)
                throw std::runtime_error("PrunedDP shortest-path witness contains a cycle.");
        }
    }

    ctx.stats.mst_input_edges += static_cast<long long>(selected_edges.size());
    std::sort(selected_edges.begin(), selected_edges.end(), [&](int left, int right)
    {
        const UndirectedEdge& lhs = ctx.graph->edges[left];
        const UndirectedEdge& rhs = ctx.graph->edges[right];
        if (lhs.w != rhs.w)
            return lhs.w < rhs.w;
        return left < right;
    });

    std::vector<int> vertices;
    vertices.reserve(selected_edges.size() * 2);
    for (int edge_id : selected_edges)
    {
        const UndirectedEdge& edge = ctx.graph->edges[edge_id];
        if (ctx.mst_local_index[edge.u] < 0)
        {
            ctx.mst_local_index[edge.u] = static_cast<int>(vertices.size());
            vertices.push_back(edge.u);
        }
        if (ctx.mst_local_index[edge.v] < 0)
        {
            ctx.mst_local_index[edge.v] = static_cast<int>(vertices.size());
            vertices.push_back(edge.v);
        }
    }

    DisjointSet dsu(static_cast<int>(vertices.size()));
    double mst_cost = 0.0;
    for (int edge_id : selected_edges)
    {
        const UndirectedEdge& edge = ctx.graph->edges[edge_id];
        if (dsu.Unite(ctx.mst_local_index[edge.u], ctx.mst_local_index[edge.v]))
            mst_cost += edge.w;
    }
    for (int vertex : vertices)
        ctx.mst_local_index[vertex] = -1;
    return mst_cost;
}

bool AcceptCandidate(SolverContext& ctx,
                     int vertex,
                     int mask,
                     double cost,
                     double parent_priority,
                     const WitnessSpec& witness_spec,
                     bool initial)
{
    ++ctx.stats.update_calls;
    double priority = RawPriority(ctx, vertex, mask, cost);
    if (ctx.options.enforce_lb2_pathmax)
        priority = std::max(priority, parent_priority);
    if (priority >= ctx.best)
    {
        ++ctx.stats.update_bound_pruned;
        return false;
    }
    if (mask == ctx.full_mask)
    {
        if (cost < ctx.best)
        {
            ctx.best = cost;
            ++ctx.stats.best_full_updates;
        }
        return true;
    }

    StateEntry* existing = ctx.states->Find(vertex, mask);
    if (ctx.options.enforce_lb2_pathmax)
    {
        if (existing != nullptr && existing->closed)
        {
            ++ctx.stats.update_finalized_skip;
            return false;
        }
        if (existing != nullptr && existing->open &&
            !(priority < existing->open_priority))
            return false;
    }
    else
    {
        if (existing != nullptr && !(cost < existing->best_cost))
            return false;
        if (existing != nullptr && existing->closed)
        {
            existing->closed = false;
            ++ctx.stats.reopened_states;
        }
    }

    StateEntry& entry = existing == nullptr ? ctx.states->Get(vertex, mask) : *existing;
    if (!ctx.options.enforce_lb2_pathmax)
        entry.best_cost = cost;
    entry.open = true;
    entry.open_cost = cost;
    entry.open_priority = priority;
    ++entry.version;
    const int witness = MaterializeWitness(ctx, witness_spec);
    ctx.queue.push({vertex, mask, cost, priority, witness, entry.version});
    ++ctx.stats.pq_pushes;
    ++ctx.stats.update_pushes;
    if (initial)
        ++ctx.stats.initial_pushes;
    const int size = CountBits(mask);
    ++ctx.stats.update_push_by_size[size];
    return true;
}

bool PopCurrent(SolverContext& ctx, QueueNode& current)
{
    while (!ctx.queue.empty())
    {
        current = ctx.queue.top();
        if (current.priority >= ctx.best)
            return false;
        ctx.queue.pop();
        ++ctx.stats.pq_pops;
        StateEntry* entry = ctx.states->Find(current.vertex, current.mask);
        if (entry == nullptr || !entry->open || entry->version != current.version ||
            entry->open_cost != current.cost || entry->open_priority != current.priority)
        {
            ++ctx.stats.stale_pops;
            continue;
        }
        entry->open = false;
        if (ctx.options.enforce_lb2_pathmax && entry->closed)
        {
            ++ctx.stats.stale_pops;
            continue;
        }
        if (!ctx.options.enforce_lb2_pathmax && current.cost != entry->best_cost)
        {
            ++ctx.stats.stale_pops;
            continue;
        }
        entry->closed = true;
        entry->settled_cost = current.cost;
        entry->settled_witness = current.witness;
        ++ctx.stats.finalized_labels;
        ++ctx.stats.finalized_by_size[CountBits(current.mask)];
        return true;
    }
    return false;
}

double Search(SolverContext& ctx)
{
    for (int group = 0; group < ctx.group_count; ++group)
    {
        const int mask = 1 << group;
        for (int vertex : ctx.query->groups[group])
        {
            AcceptCandidate(ctx,
                            vertex,
                            mask,
                            0.0,
                            0.0,
                            {WitnessKind::Seed, kNoWitness, kNoWitness, -1},
                            true);
        }
    }

    QueueNode current;
    while (!ctx.queue.empty() && ctx.queue.top().priority < ctx.best &&
           PopCurrent(ctx, current))
    {
        if (current.cost >= ctx.best)
        {
            ++ctx.stats.cost_ge_best_skips;
            continue;
        }

        if (ctx.options.use_mst_upper_bound)
        {
            const double upper = BuildMstUpper(ctx, current);
            if (upper < ctx.best)
            {
                ctx.best = upper;
                ++ctx.stats.mst_improvements;
                ++ctx.stats.best_expect_updates;
            }
        }

        const int complement = ctx.full_mask ^ current.mask;
        if (StateEntry* other = ctx.states->Find(current.vertex, complement);
            other != nullptr && other->closed)
        {
            AcceptCandidate(ctx,
                            current.vertex,
                            ctx.full_mask,
                            current.cost + other->settled_cost,
                            current.priority,
                            {WitnessKind::Merge,
                             current.witness,
                             other->settled_witness,
                             -1},
                            false);
        }

        if (current.cost <= ctx.best * 0.5 + fp::kEps)
        {
            for (const AdjEdge& edge : ctx.graph->adj[current.vertex])
            {
                ++ctx.stats.edge_relax_attempts;
                AcceptCandidate(ctx,
                                edge.to,
                                current.mask,
                                current.cost + edge.w,
                                current.priority,
                                {WitnessKind::Edge,
                                 current.witness,
                                 kNoWitness,
                                 edge.edge_id},
                                false);
            }

            for (int subset = complement; subset; subset = (subset - 1) & complement)
            {
                ++ctx.stats.merge_submask_attempts;
                StateEntry* other = ctx.states->Find(current.vertex, subset);
                if (other == nullptr || !other->closed)
                    continue;
                ++ctx.stats.merge_state_hits;
                const double merged_cost = current.cost + other->settled_cost;
                if (merged_cost <= (2.0 / 3.0) * ctx.best + fp::kEps)
                {
                    ++ctx.stats.merge_cost_gate_pass;
                    AcceptCandidate(ctx,
                                    current.vertex,
                                    current.mask | subset,
                                    merged_cost,
                                    current.priority,
                                    {WitnessKind::Merge,
                                     current.witness,
                                     other->settled_witness,
                                     -1},
                                    false);
                }
            }
        }
    }
    return ctx.best;
}

void InitializeContext(SolverContext& ctx,
                       const Graph& graph,
                       const Query& query,
                       const PrunedDpOptions& options)
{
    ctx.graph = &graph;
    ctx.query = &query;
    ctx.options = options;
    ctx.n = graph.n;
    ctx.group_count = static_cast<int>(query.groups.size());
    ctx.subset_count = 1 << ctx.group_count;
    ctx.full_mask = ctx.subset_count - 1;
    ctx.states = std::make_unique<StateStore>(options.state_storage,
                                              ctx.n,
                                              ctx.subset_count);
    ctx.edge_epoch.assign(graph.m, 0);
    ctx.mst_local_index.assign(graph.n + 1, -1);
    ctx.stats.n = graph.n;
    ctx.stats.m = graph.m;
    ctx.stats.g = ctx.group_count;
    ctx.stats.state_storage = StateStorageName(options.state_storage);
    ctx.stats.use_mst_upper_bound = options.use_mst_upper_bound;
    ctx.stats.enforce_lb2_pathmax = options.enforce_lb2_pathmax;
    ctx.stats.finalized_by_size.assign(ctx.group_count + 1, 0);
    ctx.stats.update_push_by_size.assign(ctx.group_count + 1, 0);
    for (const auto& group : query.groups)
    {
        ctx.stats.total_group_vertices += static_cast<int>(group.size());
        ctx.stats.max_group_size =
            std::max(ctx.stats.max_group_size, static_cast<int>(group.size()));
    }
}

double SolveWeight(const Graph& graph,
                   const Query& query,
                   const PrunedDpOptions& options,
                   PrunedDpStats& stats)
{
    const auto total_start = std::chrono::steady_clock::now();
    SolverContext ctx;
    const auto build_start = std::chrono::steady_clock::now();
    InitializeContext(ctx, graph, query, options);
    ctx.stats.build_ms = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - build_start)
                             .count();

    auto phase_start = std::chrono::steady_clock::now();
    BuildGroupDistances(ctx);
    ctx.stats.dist_ms = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - phase_start)
                            .count();

    phase_start = std::chrono::steady_clock::now();
    BuildAllPaths(ctx);
    ctx.stats.calw_ms = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - phase_start)
                            .count();

    phase_start = std::chrono::steady_clock::now();
    const double answer = Search(ctx);
    ctx.stats.search_ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - phase_start)
                              .count();
    ctx.stats.discovered_states = ctx.states->Discovered();
    ctx.stats.total_ms = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - total_start)
                             .count();
    stats = std::move(ctx.stats);
    return answer;
}
}  // namespace

const char* StateStorageName(StateStorage storage)
{
    return storage == StateStorage::Hash ? "hash" : "dense";
}

SolveResult SolveOneQuery(const Graph& graph, const Query& query)
{
    return SolveOneQuery(graph, query, PrunedDpOptions{});
}

SolveResult SolveOneQuery(const Graph& graph,
                          const Query& query,
                          const PrunedDpOptions& options)
{
    SolveResult result;
    const int group_count = static_cast<int>(query.groups.size());
    if (!group_count)
        return {0.0, true, {}};
    if (group_count >= 31)
        throw std::runtime_error("PrunedDP currently supports group count <= 30.");
    if (!IsQueryFeasible(graph, query))
        return result;

    const double answer = SolveWeight(graph, query, options, result.stats);
    if (answer >= fp::kInf / 4)
        return result;
    result.best_weight = answer;
    result.feasible = true;
    return result;
}

}  // namespace gst::methods::pruned_dp
