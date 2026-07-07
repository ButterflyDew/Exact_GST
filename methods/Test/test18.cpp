#include "test18.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <utility>

#include "../../float_compare.h"
#include "../../memory_usage.h"
#include "../../query_feasibility.h"

namespace gst::methods::test18
{
namespace
{
struct State
{
    std::vector<int> v, cover;
    std::vector<double> d, need, dense;
    int count = 0;
    bool ready = false, light = false, dense_row = false;
};

long long LogCost(size_t x)
{
    long long r = 1;
    for (size_t p = 2; p < x; p <<= 1)
        ++r;
    return r;
}

template <class Use>
void JoinRows(const std::vector<int>& av,
              const std::vector<double>& ad,
              const std::vector<int>& bv,
              const std::vector<double>& bd,
              Use&& use,
              long long& scans,
              long long& hits)
{
    const long long linear = static_cast<long long>(av.size() + bv.size());
    const long long abin = static_cast<long long>(av.size()) * LogCost(bv.size() + 1);
    const long long bbin = static_cast<long long>(bv.size()) * LogCost(av.size() + 1);

    if (!av.empty() && !bv.empty() && abin * 3 < linear * 2)
    {
        scans += abin;
        for (size_t i = 0; i < av.size(); ++i)
        {
            auto it = std::lower_bound(bv.begin(), bv.end(), av[i]);
            if (it != bv.end() && *it == av[i])
            {
                size_t j = it - bv.begin();
                use(av[i], ad[i], bd[j], i, j), ++hits;
            }
        }
        return;
    }
    if (!av.empty() && !bv.empty() && bbin * 3 < linear * 2)
    {
        scans += bbin;
        for (size_t j = 0; j < bv.size(); ++j)
        {
            auto it = std::lower_bound(av.begin(), av.end(), bv[j]);
            if (it != av.end() && *it == bv[j])
            {
                size_t i = it - av.begin();
                use(bv[j], ad[i], bd[j], i, j), ++hits;
            }
        }
        return;
    }

    scans += linear;
    size_t i = 0, j = 0;
    while (i < av.size() && j < bv.size())
    {
        if (av[i] < bv[j])
            ++i;
        else if (bv[j] < av[i])
            ++j;
        else
            use(av[i], ad[i], bd[j], i, j), ++hits, ++i, ++j;
    }
}
}  // namespace

SolveResult SolveOneQuery(const Graph& graph, const Query& query)
{
    using Clock = std::chrono::steady_clock;
    using P = std::pair<double, int>;
    using Heap = std::priority_queue<P, std::vector<P>, std::greater<P>>;

    SolveResult res;
    auto& st = res.stats;
    const int n = graph.n, g = static_cast<int>(query.groups.size());
    st.n = n, st.m = graph.m, st.g = g;
    if (!g)
        return {0.0, true, {}};
    if (g > 22)
        throw std::runtime_error("Test18 supports group count <= 22.");
    if (!IsQueryFeasible(graph, query))
        return res;

    const auto solve_start = Clock::now();
    const int H = g / 2, S = 1 << g, U = S - 1, N = n + 1;
    std::vector<int> pc(S), first_bit(S), order;
    st.total_by_size.assign(H + 1, 0);
    st.active_by_size.assign(H + 1, 0);
    st.inqueue_by_size.assign(H + 1, 0);
    st.merge_by_size.assign(H + 1, 0);
    st.best_after_size.assign(H + 1, fp::kInf);
    st.early_cover_by_rem_size.assign(g + 1, 0);
    st.early_cover_ready_by_rem_size.assign(g + 1, 0);
    st.early_cover_better_by_rem_size.assign(g + 1, 0);
    st.pair_saved_by_cover_size.assign(g + 1, 0);
    st.pair_saved_slack_rel_bucket.assign(6, 0);
    st.dense_rows_by_size.assign(H + 1, 0);
    st.dense_states_by_size.assign(H + 1, 0);
    for (int s = 1; s < S; ++s)
    {
        pc[s] = pc[s >> 1] + (s & 1);
        first_bit[s] = (s & 1) ? 0 : first_bit[s >> 1] + 1;
        if (pc[s] <= H)
            st.total_by_size[pc[s]] += n, order.push_back(s);
    }
    std::sort(order.begin(), order.end(), [&](int a, int b)
    { return pc[a] != pc[b] ? pc[a] < pc[b] : a < b; });

    std::vector<int> color(N);
    for (int a = 0; a < g; ++a)
    {
        st.total_group_vertices += static_cast<int>(query.groups[a].size());
        st.max_group_size = std::max(st.max_group_size, static_cast<int>(query.groups[a].size()));
        for (int v : query.groups[a])
            color[v] |= 1 << a;
    }

    const auto gd_start = Clock::now();
    std::vector<std::vector<double>> gd(g, std::vector<double>(N, fp::kInf));
    for (int a = 0; a < g; ++a)
    {
        Heap q;
        for (int v : query.groups[a])
            if (gd[a][v])
                gd[a][v] = 0, q.push({0, v});
        while (!q.empty())
        {
            auto [d, u] = q.top();
            q.pop();
            if (d != gd[a][u])
                continue;
            for (auto e : graph.adj[u])
                if (d + e.w < gd[a][e.to])
                    gd[a][e.to] = d + e.w, q.push({gd[a][e.to], e.to});
        }
    }
    st.group_dist_ms = std::chrono::duration<double, std::milli>(Clock::now() - gd_start).count();

    std::vector<std::vector<double>> gp(g, std::vector<double>(g, fp::kInf));
    for (int a = 0; a < g; ++a)
        for (int b = 0; b < g; ++b)
            for (int v : query.groups[b])
                gp[a][b] = std::min(gp[a][b], gd[a][v]);

    std::vector<double> mst(S, -1);
    auto MstHalf = [&](int s)
    {
        if (!(s & (s - 1)))
            return 0.0;
        if (mst[s] >= 0)
            return mst[s];
        double sum = 0;
        std::vector<double> d(g, fp::kInf);
        std::vector<char> used(g);
        int z = 0;
        while (!(s >> z & 1))
            ++z;
        d[z] = 0;
        for (int it = 0; it < pc[s]; ++it)
        {
            int u = -1;
            for (int a = 0; a < g; ++a)
                if ((s >> a & 1) && !used[a] && (u < 0 || d[a] < d[u]))
                    u = a;
            used[u] = 1, sum += d[u];
            for (int a = 0; a < g; ++a)
                if ((s >> a & 1) && !used[a])
                    d[a] = std::min(d[a], gp[u][a]);
        }
        return mst[s] = sum * .5;
    };
    auto LowerBound = [&](int v, int s)
    {
        if (!s)
            return 0.0;
        double far = 0, x = fp::kInf, y = fp::kInf;
        for (int t = s; t; t &= t - 1)
        {
            double z = gd[first_bit[t]][v];
            far = std::max(far, z);
            if (z < x)
                y = x, x = z;
            else if (z < y)
                y = z;
        }
        return !(s & (s - 1)) ? far : std::max(far, MstHalf(s) + (x + y) * .5);
    };

    double best = fp::kInf;
    int best_root = 1;
    for (int v = 1; v <= n; ++v)
    {
        double cur = 0;
        for (int a = 0; a < g; ++a)
            cur += gd[a][v];
        if (cur < best)
            best = cur, best_root = v;
    }
    st.root_star_upper = best;

    auto GrowGreedy = [&](std::vector<int> src, int covered, double ans, std::vector<int>* chosen)
    {
        std::vector<int> seen(N), parent(N), in_tree(N);
        std::vector<double> d(N);
        if (chosen)
            chosen->clear();
        int w = 0;
        for (int v : src)
            if (!in_tree[v])
            {
                in_tree[v] = 1, src[w++] = v, covered |= color[v];
                if (chosen && color[v])
                    chosen->push_back(v);
            }
        src.resize(w);
        for (int stamp = 1; covered != U && ans < best; ++stamp)
        {
            Heap q;
            for (int v : src)
                seen[v] = stamp, d[v] = 0, parent[v] = 0, q.push({0, v});
            bool ok = false;
            while (!q.empty())
            {
                auto [du, u] = q.top();
                q.pop(), ++st.greedy_pops;
                if (seen[u] != stamp || du != d[u])
                    continue;
                if (color[u] & (U ^ covered))
                {
                    ans += du, ok = true;
                    for (int x = u; !in_tree[x]; x = parent[x])
                    {
                        int add = color[x] & (U ^ covered);
                        in_tree[x] = 1, src.push_back(x), covered |= color[x];
                        if (chosen && add)
                            chosen->push_back(x);
                    }
                    break;
                }
                for (auto e : graph.adj[u])
                    if (seen[e.to] != stamp || du + e.w < d[e.to])
                        seen[e.to] = stamp, d[e.to] = du + e.w, parent[e.to] = u, q.push({d[e.to], e.to});
            }
            if (!ok)
                return fp::kInf;
        }
        return covered == U ? ans : fp::kInf;
    };
    auto Greedy = [&](int root, std::vector<int>* chosen)
    {
        return GrowGreedy(std::vector<int>{root}, color[root], 0.0, chosen);
    };
    const auto greedy_start = Clock::now();
    std::vector<int> greedy_roots;
    st.greedy_upper = Greedy(best_root, &greedy_roots);
    st.greedy_ms = std::chrono::duration<double, std::milli>(Clock::now() - greedy_start).count();
    best = std::min(best, st.greedy_upper);
    std::sort(greedy_roots.begin(), greedy_roots.end());
    greedy_roots.erase(std::unique(greedy_roots.begin(), greedy_roots.end()), greedy_roots.end());
    st.multi_greedy_upper = best;
    for (int root : greedy_roots)
    {
        if (root == best_root)
            continue;
        ++st.multi_greedy_roots;
        double val = Greedy(root, nullptr);
        if (val < best)
            best = st.multi_greedy_upper = val;
    }
    std::vector<int> pair_partition_roots;
    {
        std::vector<char> pair_root_seen(N);
        auto AddPairRoot = [&](int v)
        {
            if (!pair_root_seen[v])
                pair_root_seen[v] = 1, pair_partition_roots.push_back(v);
        };
        AddPairRoot(best_root);
        for (int root : greedy_roots)
            AddPairRoot(root);
        int min_group = 0;
        for (int a = 1; a < g; ++a)
            if (query.groups[a].size() < query.groups[min_group].size())
                min_group = a;
        for (int root : query.groups[min_group])
            AddPairRoot(root);
    }
    st.greedy_ms = std::chrono::duration<double, std::milli>(Clock::now() - greedy_start).count();
    st.preprocess_ms = std::chrono::duration<double, std::milli>(Clock::now() - solve_start).count();

    const auto dp_start = Clock::now();
    std::vector<double> full_lb(N);
    for (int v = 1; v <= n; ++v)
    {
        full_lb[v] = LowerBound(v, U);
        if (full_lb[v] <= best)
            ++st.global_root_alive;
        else
            ++st.global_root_pruned;
    }

    std::vector<State> state(S);
    auto Available = [&](int s) { return s && pc[s] <= H && (pc[s] == 1 || state[s].ready); };
    auto Lookup = [&](int s, int v) -> double
    {
        if (!s)
            return 0.0;
        if (full_lb[v] > best)
            return fp::kInf;
        if (pc[s] == 1)
            return gd[first_bit[s]][v];
        if (!Available(s))
            return fp::kInf;
        const auto& z = state[s];
        if (z.dense_row)
            return z.dense[v];
        auto it = std::lower_bound(z.v.begin(), z.v.end(), v);
        if (it == z.v.end() || *it != v)
            return fp::kInf;
        size_t i = it - z.v.begin();
        if (!z.light && z.need[i] > best)
        {
            ++st.lookup_need_skips;
            return fp::kInf;
        }
        return z.d[i];
    };
    auto RawLookup = [&](int s, int v) -> double
    {
        if (!s)
            return 0.0;
        if (full_lb[v] > best)
            return fp::kInf;
        if (pc[s] == 1)
            return gd[first_bit[s]][v];
        if (!Available(s))
            return fp::kInf;
        const auto& z = state[s];
        if (z.dense_row)
            return z.dense[v];
        auto it = std::lower_bound(z.v.begin(), z.v.end(), v);
        if (it == z.v.end() || *it != v)
            return fp::kInf;
        return z.d[it - z.v.begin()];
    };
    bool pair_partition_done = false;
    auto PairPartitionUpper = [&]()
    {
        if (pair_partition_done || g < 3)
            return;
        pair_partition_done = true;
        const auto begin = Clock::now();
        std::vector<double> f(S);
        for (int root : pair_partition_roots)
        {
            if (full_lb[root] > best)
                continue;
            ++st.pair_partition_roots;
            std::fill(f.begin(), f.end(), fp::kInf);
            f[0] = 0.0;
            for (int mask = 1; mask < S; ++mask)
            {
                int a = first_bit[mask], single = 1 << a;
                double val = f[mask ^ single] + gd[a][root];
                for (int t = mask ^ single; t; t &= t - 1)
                {
                    int b = first_bit[t], pair = single | (1 << b);
                    double d = Lookup(pair, root);
                    if (d < fp::kInf)
                        val = std::min(val, f[mask ^ pair] + d);
                }
                f[mask] = val;
            }
            if (f[U] < best)
            {
                if (!st.best_updates)
                    st.first_best_update_size = 2;
                st.last_best_update_size = 2;
                ++st.best_updates;
                ++st.pair_partition_updates;
                best = f[U];
            }
        }
        st.pair_partition_upper = best;
        st.pair_partition_ms += std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    };
    std::vector<double> dist(N, fp::kInf), lb(N), far_cache(N);
    std::vector<int> cov(N), touched, kept, lb_seen(N), far_seen(N);
    std::vector<std::pair<int, int>> complete_pairs;
    int stamp = 0;
    int current_size = 0;
    double last_compact_best = best;
    auto last_progress = Clock::now();

    auto CompactRows = [&]()
    {
        const auto begin = Clock::now();
        long long removed = 0;
        for (int t = 1; t < S; ++t)
        {
            if (pc[t] <= 1 || pc[t] > H || !state[t].ready || state[t].light)
                continue;
            auto& z = state[t];
            int w = 0;
            for (int i = 0; i < static_cast<int>(z.v.size()); ++i)
            {
                int v = z.v[i];
                if (full_lb[v] > best || z.need[i] > best)
                {
                    ++removed;
                    continue;
                }
                if (w != i)
                    z.v[w] = z.v[i], z.d[w] = z.d[i], z.cover[w] = z.cover[i], z.need[w] = z.need[i];
                ++w;
            }
            z.v.resize(w), z.d.resize(w), z.cover.resize(w), z.need.resize(w);
            z.count = w;
        }
        ++st.compact_calls;
        st.compact_removed += removed;
        st.compact_ms += std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        last_compact_best = best;
    };

    for (int s : order)
    {
        const int k = pc[s], rem = U ^ s;
        if (k == 1)
            continue;
        if (k != current_size)
        {
            if (current_size && best + 1e-12 < last_compact_best)
                CompactRows();
            if (current_size == 2)
                PairPartitionUpper();
            current_size = k;
            if (g >= 15)
            {
                const double elapsed = std::chrono::duration<double>(Clock::now() - dp_start).count();
                std::cerr << "[Test18] enter k=" << k
                          << " elapsed=" << elapsed
                          << " best=" << best
                          << " finite=" << st.finite_states
                          << " inqueue=" << st.total_inqueue
                          << " active_seed=" << st.active_seed
                          << "\n";
                last_progress = Clock::now();
            }
        }
        ++stamp, ++st.masks_processed;
        touched.clear();
        kept.clear();

        int rem_bits[22], rem_cnt = 0;
        for (int t = rem; t; t &= t - 1)
            rem_bits[rem_cnt++] = first_bit[t];
        const double rem_mst = rem_cnt <= 1 ? 0.0 : MstHalf(rem);

        auto Lb = [&](int v)
        {
            if (lb_seen[v] == stamp)
                return lb[v];
            double far = 0, x = fp::kInf, y = fp::kInf;
            for (int i = 0; i < rem_cnt; ++i)
            {
                double z = gd[rem_bits[i]][v];
                far = std::max(far, z);
                if (z < x)
                    y = x, x = z;
                else if (z < y)
                    y = z;
            }
            ++st.lb_calls, lb_seen[v] = stamp;
            return lb[v] = rem_cnt <= 1 ? far : std::max(far, rem_mst + (x + y) * .5);
        };
        auto Far = [&](int v)
        {
            if (far_seen[v] == stamp)
                return far_cache[v];
            double ans = 0;
            for (int i = 0; i < rem_cnt; ++i)
                ans = std::max(ans, gd[rem_bits[i]][v]);
            far_seen[v] = stamp;
            return far_cache[v] = ans;
        };

        auto Set = [&](int v, double w, int c)
        {
            if (w >= dist[v])
                return;
            if (dist[v] == fp::kInf)
                touched.push_back(v);
            dist[v] = w, cov[v] = c;
        };
        auto TrySet = [&](int v, double w, int c)
        {
            ++st.tryset_calls;
            if (full_lb[v] > best)
            {
                ++st.tryset_pruned_full;
                return;
            }
            if (w >= best)
            {
                ++st.tryset_pruned_ge_best;
                return;
            }
            if (w + Far(v) > best)
            {
                ++st.tryset_pruned_far;
                return;
            }
            double old_lb = Lb(v);
            if (w + old_lb > best)
            {
                ++st.tryset_pruned_lb;
                return;
            }
            ++st.tryset_keep;
            Set(v, w, c);
        };

        auto RowCover = [&](const State& row, int mask, int i, int v)
        {
            return row.light ? (mask | color[v]) : row.cover[i];
        };
        auto RowAlive = [&](const State& row, int i)
        {
            if (!row.light && row.need[i] > best)
            {
                ++st.stale_need_skips;
                return false;
            }
            return true;
        };
        auto JoinWithSingleton = [&](int single, int row_mask, const State& row)
        {
            int bit = first_bit[single];
            if (row.dense_row)
            {
                st.pull_scan += n;
                for (int v = 1; v <= n; ++v)
                    if (row.dense[v] < fp::kInf)
                        ++st.pull_hits, TrySet(v, gd[bit][v] + row.dense[v], row_mask | single | color[v]);
                return;
            }
            st.pull_scan += row.v.size();
            for (size_t i = 0; i < row.v.size(); ++i)
            {
                ++st.pull_hits;
                if (!RowAlive(row, static_cast<int>(i)))
                    continue;
                int v = row.v[i];
                TrySet(v, gd[bit][v] + row.d[i], RowCover(row, row_mask, static_cast<int>(i), v) | single | color[v]);
            }
        };
        auto JoinStates = [&](int am, const State& x, int bm, const State& y)
        {
            if (x.dense_row && y.dense_row)
            {
                st.pull_scan += n;
                for (int v = 1; v <= n; ++v)
                    if (x.dense[v] < fp::kInf && y.dense[v] < fp::kInf)
                        ++st.pull_hits, TrySet(v, x.dense[v] + y.dense[v], am | bm | color[v]);
                return;
            }
            if (x.dense_row || y.dense_row)
            {
                const State& den = x.dense_row ? x : y;
                const State& sp = x.dense_row ? y : x;
                int den_mask = x.dense_row ? am : bm;
                int sp_mask = x.dense_row ? bm : am;
                st.pull_scan += sp.v.size();
                for (int i = 0; i < static_cast<int>(sp.v.size()); ++i)
                {
                    int v = sp.v[i];
                    double da = den.dense[v];
                    if (da >= fp::kInf)
                        continue;
                    ++st.pull_hits;
                    if (!RowAlive(sp, i))
                        continue;
                    TrySet(v, da + sp.d[i], (den_mask | color[v]) | RowCover(sp, sp_mask, i, v));
                }
                return;
            }
            JoinRows(x.v, x.d, y.v, y.d,
                     [&](int v, double da, double db, size_t ia, size_t ib)
                     {
                         if (!RowAlive(x, static_cast<int>(ia)) || !RowAlive(y, static_cast<int>(ib)))
                             return;
                         double cand = da + db;
                         TrySet(v, cand, RowCover(x, am, static_cast<int>(ia), v) |
                                             RowCover(y, bm, static_cast<int>(ib), v));
                     },
                     st.pull_scan, st.pull_hits);
        };

        const auto pull_begin = Clock::now();
        const long long hits_before = st.pull_hits;
        if (k == 2)
        {
            int a = first_bit[s], b = first_bit[s ^ (1 << a)];
            ++st.pull_pairs;
            st.pull_scan += n;
            for (int v = 1; v <= n; ++v)
                ++st.pull_hits, TrySet(v, gd[a][v] + gd[b][v], s | color[v]);
        }
        else
        {
            for (int a = (s - 1) & s; a; a = (a - 1) & s)
            {
                int b = s ^ a;
                if (!b || a > b || !Available(a) || !Available(b))
                    continue;
                ++st.pull_pairs;
                if (pc[a] == 1)
                {
                    JoinWithSingleton(a, b, state[b]);
                    continue;
                }
                if (pc[b] == 1)
                {
                    JoinWithSingleton(b, a, state[a]);
                    continue;
                }
                JoinStates(a, state[a], b, state[b]);
            }
        }
        st.pull_ms += std::chrono::duration<double, std::milli>(Clock::now() - pull_begin).count();
        st.merge_by_size[k] += st.pull_hits - hits_before;

        st.active_seed += touched.size();
        st.active_by_size[k] += touched.size();

        const bool can_complete = k * 3 >= g;
        complete_pairs.clear();
        if (can_complete)
        {
            for (int x = rem;; x = (x - 1) & rem)
            {
                int y = rem ^ x;
                if (x <= y && pc[x] <= H && pc[y] <= H &&
                    (!x || Available(x)) && (!y || Available(y)))
                    complete_pairs.push_back({x, y});
                if (!x)
                    break;
            }
            st.complement_mask_pairs += complete_pairs.size();
            if (complete_pairs.empty())
                ++st.complement_mask_empty;
        }

        auto Complete = [&](int u, double d)
        {
            ++st.complement_calls;
            if (!can_complete)
            {
                ++st.complement_skip_early;
                int cover_rem = U ^ (cov[u] & U);
                if (cover_rem != rem)
                {
                    ++st.complement_cover_smaller;
                    ++st.early_cover_extra;
                    st.early_cover_extra_groups += pc[rem] - pc[cover_rem];
                    if (!st.early_cover_min_rem || pc[cover_rem] < st.early_cover_min_rem)
                        st.early_cover_min_rem = pc[cover_rem];
                    ++st.early_cover_by_rem_size[pc[cover_rem]];
                    if (pc[cover_rem] <= 2 * k)
                    {
                        ++st.early_cover_rem_le_2k;
                        ++st.early_cover_pair_checks;
                        bool ready = false;
                        double best_cand = fp::kInf;
                        for (int x = cover_rem;; x = (x - 1) & cover_rem)
                        {
                            int y = cover_rem ^ x;
                            if (x <= y && pc[x] <= H && pc[y] <= H &&
                                (!x || Available(x)) && (!y || Available(y)))
                            {
                                ++st.early_cover_pair_splits;
                                double dx = RawLookup(x, u), dy = RawLookup(y, u);
                                if (dx < fp::kInf && dy < fp::kInf)
                                {
                                    ready = true;
                                    double cand = d + dx + dy;
                                    if (cand < best_cand)
                                        best_cand = cand;
                                }
                            }
                            if (!x)
                                break;
                        }
                        if (ready)
                        {
                            ++st.early_cover_pair_ready;
                            ++st.early_cover_ready_by_rem_size[pc[cover_rem]];
                        }
                        if (best_cand < fp::kInf &&
                            (st.early_cover_best_candidate < 0 || best_cand < st.early_cover_best_candidate))
                            st.early_cover_best_candidate = best_cand;
                        if (best_cand < best)
                        {
                            if (!st.best_updates)
                                st.first_best_update_size = k;
                            st.last_best_update_size = k;
                            ++st.best_updates;
                            ++st.early_cover_best_updates;
                            ++st.early_cover_pair_better;
                            ++st.early_cover_better_by_rem_size[pc[cover_rem]];
                            if (!st.early_cover_first_better_size)
                                st.early_cover_first_better_size = k;
                            best = best_cand;
                        }
                    }
                    if (pc[cover_rem] <= 2 * k)
                        ++st.complement_cover_possible;
                }
                return;
            }
            const auto begin = Clock::now();
            double other = fp::kInf;
            for (auto [x, y] : complete_pairs)
            {
                ++st.complement_pairs;
                double dx = Lookup(x, u), dy = Lookup(y, u);
                ++st.complement_scan;
                if (dx < fp::kInf && dy < fp::kInf)
                {
                    other = std::min(other, dx + dy);
                    ++st.complement_hits;
                }
            }
            if (other < fp::kInf)
            {
                double cand = d + other;
                if (cand < best)
                {
                    if (!st.best_updates)
                        st.first_best_update_size = k;
                    st.last_best_update_size = k;
                    ++st.best_updates;
                    best = cand;
                }
            }
            st.complement_ms += std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        };

        const auto search_begin = Clock::now();
        Heap q;
        for (int v : touched)
        {
            double d = dist[v];
            ++st.seed_try;
            if (d + Far(v) > best)
            {
                ++st.seed_block_far;
                continue;
            }
            double old_lb = Lb(v);
            if (d + old_lb > best)
            {
                ++st.seed_block_lb;
                continue;
            }
            q.push({d, v}), ++st.pq_push, ++st.seed_push;
        }

        while (!q.empty())
        {
            auto [key, u] = q.top();
            q.pop(), ++st.pq_pop;
            double d = dist[u];
            if (key != d)
                continue;
            if (full_lb[u] > best)
            {
                ++st.pop_pruned_full;
                continue;
            }
            if (d + Far(u) > best)
            {
                ++st.pop_pruned_far;
                continue;
            }
            double old_lb = Lb(u);
            if (d + old_lb > best)
            {
                ++st.pop_pruned_lb;
                continue;
            }
            Complete(u, d);
            kept.push_back(u);
            ++st.total_inqueue, ++st.inqueue_by_size[k];
            for (auto e : graph.adj[u])
            {
                ++st.relax_try;
                double nd = d + e.w;
                if (nd >= dist[e.to])
                    continue;
                if (full_lb[e.to] > best)
                {
                    ++st.tryset_pruned_full;
                    continue;
                }
                if (nd >= best)
                {
                    ++st.prune_ge_best;
                    continue;
                }
                if (nd + Far(e.to) > best)
                {
                    ++st.prune_far;
                    continue;
                }
                if (dist[e.to] == fp::kInf)
                    touched.push_back(e.to);
                dist[e.to] = nd, cov[e.to] = cov[u] | color[e.to];
                ++st.relax_ok, ++st.pq_push;
                q.push({nd, e.to});
            }
        }
        st.search_ms += std::chrono::duration<double, std::milli>(Clock::now() - search_begin).count();

        std::sort(kept.begin(), kept.end());
        kept.erase(std::unique(kept.begin(), kept.end()), kept.end());
        int keep_w = 0;
        for (int v : kept)
        {
            double need = dist[v] + Lb(v);
            if (full_lb[v] > best)
            {
                ++st.final_pruned_full;
                continue;
            }
            if (need > best)
            {
                ++st.final_pruned_need;
                continue;
            }
            kept[keep_w++] = v;
        }
        kept.resize(keep_w);

        auto& z = state[s];
        z.v.clear(), z.d.clear(), z.cover.clear(), z.need.clear(), z.dense.clear();
        z.count = static_cast<int>(kept.size());
        const bool dense_by_cost = static_cast<long long>(z.count) * 3 > static_cast<long long>(N) * 2;
        z.light = k == 2 || dense_by_cost;
        z.dense_row = dense_by_cost;
        if (z.dense_row)
            z.dense.assign(N, fp::kInf);
        else
            z.v.reserve(kept.size()), z.d.reserve(kept.size());
        if (!z.light)
            z.cover.reserve(kept.size()), z.need.reserve(kept.size());
        if (z.dense_row)
        {
            ++st.dense_rows;
            ++st.dense_rows_by_size[k];
        }
        if (z.dense_row && k == 2)
            ++st.pair_dense_rows;
        for (int v : kept)
        {
            double need = dist[v] + Lb(v);
            if (z.light)
            {
                if (z.dense_row)
                    z.dense[v] = dist[v];
                else
                    z.v.push_back(v), z.d.push_back(dist[v]);
                if (k == 2)
                {
                    int cover_size = pc[cov[v] & U];
                    ++st.pair_saved_by_cover_size[cover_size];
                    if (cover_size > 2)
                    {
                        ++st.pair_saved_cover_extra;
                        st.pair_saved_cover_extra_groups += cover_size - 2;
                    }
                    double rel = best > 0 ? (best - need) / best : best - need;
                    if (rel < 0)
                        rel = 0;
                    ++st.pair_saved_slack_count;
                    st.pair_saved_slack_rel_sum += rel;
                    st.pair_saved_slack_rel_max = std::max(st.pair_saved_slack_rel_max, rel);
                    int b = rel <= .01 ? 0 : rel <= .05 ? 1 : rel <= .10 ? 2 : rel <= .25 ? 3 : rel <= .50 ? 4 : 5;
                    ++st.pair_saved_slack_rel_bucket[b];
                }
            }
            else
                z.v.push_back(v), z.d.push_back(dist[v]), z.cover.push_back(cov[v]), z.need.push_back(need);
        }
        for (int v : touched)
            dist[v] = fp::kInf;
        z.ready = true;
        if (z.dense_row)
        {
            st.dense_states += z.count;
            st.dense_states_by_size[k] += z.count;
        }
        if (z.dense_row && k == 2)
            st.pair_dense_states += z.count;
        st.finite_states += z.count;
        st.best_after_size[k] = best;
        if (g >= 15 && std::chrono::duration<double>(Clock::now() - last_progress).count() >= 5.0)
        {
            const double elapsed = std::chrono::duration<double>(Clock::now() - dp_start).count();
            const auto& sb = st.pair_saved_slack_rel_bucket;
            long long le1 = sb[0];
            long long le5 = le1 + sb[1];
            long long le10 = le5 + sb[2];
            long long le25 = le10 + sb[3];
            long long le50 = le25 + sb[4];
            double slack_avg = st.pair_saved_slack_count
                                   ? st.pair_saved_slack_rel_sum / st.pair_saved_slack_count
                                   : -1.0;
            const auto mem = gst::GetProcessMemoryUsage();
            std::cerr << "[Test18] k=" << k
                      << " masks=" << st.masks_processed
                      << " elapsed=" << elapsed
                      << " best=" << best
                      << " finite=" << st.finite_states
                      << " inqueue=" << st.total_inqueue
                      << " active_seed=" << st.active_seed
                      << " try_keep=" << st.tryset_keep
                      << " full_prune=" << st.tryset_pruned_full
                      << " far_prune=" << st.tryset_pruned_far
                      << " lb_prune=" << st.tryset_pruned_lb
                      << " seed_push=" << st.seed_push
                      << " seed_block_lb=" << st.seed_block_lb
                      << " early_cover=" << st.early_cover_extra
                      << " early_ready=" << st.early_cover_pair_ready
                      << " early_better=" << st.early_cover_pair_better
                      << " early_updates=" << st.early_cover_best_updates
                      << " early_cand=" << st.early_cover_best_candidate
                      << " pair_cover_extra=" << st.pair_saved_cover_extra
                      << " pair_cover_extra_groups=" << st.pair_saved_cover_extra_groups
                      << " pair_dense_rows=" << st.pair_dense_rows
                      << " dense_rows=" << st.dense_rows
                      << " dense_states=" << st.dense_states
                      << " rss_mb=" << gst::BytesToMiB(mem.current_rss_bytes)
                      << " peak_mb=" << gst::BytesToMiB(mem.peak_rss_bytes)
                      << " pair_slack_avg=" << slack_avg
                      << " pair_slack_le1p=" << le1
                      << " pair_slack_le5p=" << le5
                      << " pair_slack_le10p=" << le10
                      << " pair_slack_le25p=" << le25
                      << " pair_slack_le50p=" << le50
                      << " pair_slack_gt50p=" << sb[5]
                      << " stale_skip=" << st.stale_need_skips
                      << "\n";
            last_progress = Clock::now();
        }
    }

    st.dp_ms = std::chrono::duration<double, std::milli>(Clock::now() - dp_start).count();
    res.best_weight = best;
    res.feasible = true;
    return res;
}

}  // namespace gst::methods::test18
