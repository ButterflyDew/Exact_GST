#include "test15.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <utility>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

namespace gst::methods::test15
{
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
        return {0, true, {}};
    if (g > 22)
        throw std::runtime_error("Test15 supports group count <= 22.");
    if (!IsQueryFeasible(graph, query))
        return res;

    const auto solve_start = Clock::now();
    const int H = g / 2, S = 1 << g, U = S - 1, N = n + 1;
    std::vector<int> pc(S), order;
    st.valid_by_size.assign(H + 1, 0);
    st.total_by_size.assign(H + 1, 0);
    st.active_by_size.assign(H + 1, 0);
    st.inqueue_by_size.assign(H + 1, 0);
    st.merge_by_size.assign(H + 1, 0);
    st.certify_try_by_size.assign(H + 1, 0);
    st.certify_success_by_size.assign(H + 1, 0);
    for (int s = 1; s < S; ++s)
    {
        pc[s] = pc[s >> 1] + (s & 1);
        if (pc[s] <= H)
            st.total_by_size[pc[s]] += n;
        order.push_back(s);
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) { return pc[a] < pc[b]; });

    std::vector<int> color(N);
    for (int a = 0; a < g; ++a)
    {
        st.total_group_vertices += static_cast<int>(query.groups[a].size());
        st.max_group_size = std::max(st.max_group_size, static_cast<int>(query.groups[a].size()));
        for (int v : query.groups[a])
            color[v] |= 1 << a;
    }

    auto gd_start = Clock::now();
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

    const int G0 = g / 2, G1 = g - G0, S0 = 1 << G0, S1 = 1 << G1;
    std::vector<int> bit0(S0), bit1(S1);
    for (int s = 1; s < S0; ++s)
        while (!(s >> bit0[s] & 1))
            ++bit0[s];
    for (int s = 1; s < S1; ++s)
        while (!(s >> bit1[s] & 1))
            ++bit1[s];
    std::vector<unsigned char> far0(static_cast<size_t>(N) * S0, 255);
    std::vector<unsigned char> far1(static_cast<size_t>(N) * S1, 255);
    for (int v = 1; v <= n; ++v)
    {
        size_t p0 = static_cast<size_t>(v) * S0;
        for (int s = 1; s < S0; ++s)
        {
            int b = bit0[s], pre = s ^ (1 << b), old = far0[p0 + pre];
            far0[p0 + s] = old == 255 || gd[b][v] > gd[old][v] ? b : old;
        }
        size_t p1 = static_cast<size_t>(v) * S1;
        for (int s = 1; s < S1; ++s)
        {
            int b = G0 + bit1[s], pre = s ^ (1 << bit1[s]), old = far1[p1 + pre];
            far1[p1 + s] = old == 255 || gd[b][v] > gd[old][v] ? b : old;
        }
    }
    auto Far = [&](int v, int s)
    {
        int a = far0[static_cast<size_t>(v) * S0 + (s & (S0 - 1))];
        int b = far1[static_cast<size_t>(v) * S1 + (s >> G0)];
        if (a == 255)
            return b == 255 ? 0.0 : gd[b][v];
        if (b == 255)
            return gd[a][v];
        return std::max(gd[a][v], gd[b][v]);
    };

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
        for (int a = 0; a < g; ++a)
            if (s >> a & 1)
            {
                double z = gd[a][v];
                far = std::max(far, z);
                if (z < x)
                    y = x, x = z;
                else if (z < y)
                    y = z;
        }
        return !(s & (s - 1)) ? far : std::max(far, MstHalf(s) + (x + y) * .5);
    };

    int comb[23][12] = {};
    for (int i = 0; i <= g; ++i)
    {
        comb[i][0] = 1;
        for (int j = 1; j <= std::min(i, H); ++j)
            comb[i][j] = (j == i ? 1 : comb[i - 1][j - 1] + comb[i - 1][j]);
    }
    auto CountRange = [&](int bits, int l, int r)
    {
        if (l < 0)
            l = 0;
        if (r > H)
            r = H;
        if (r > bits)
            r = bits;
        int ans = 0;
        for (int i = l; i <= r; ++i)
            ans += comb[bits][i];
        return ans;
    };
    auto EnumSubsets = [&](int mask, int l, int r, auto&& fn)
    {
        int bits[24], cnt = 0;
        for (int x = mask; x; x ^= x & -x)
            bits[cnt++] = x & -x;
        if (r > cnt)
            r = cnt;
        auto dfs = [&](auto&& self, int p, int left, int cur) -> void
        {
            if (!left)
            {
                fn(cur);
                return;
            }
            for (int i = p; i <= cnt - left; ++i)
                self(self, i + 1, left - 1, cur | bits[i]);
        };
        for (int need = l; need <= r; ++need)
            dfs(dfs, 0, need, 0);
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
    auto Greedy = [&](int root)
    {
        int covered = color[root];
        double ans = 0;
        std::vector<int> src{root}, seen(N);
        std::vector<double> d(N);
        for (int stamp = 1; covered != U && ans < best; ++stamp)
        {
            Heap q;
            for (int v : src)
                seen[v] = stamp, d[v] = 0, q.push({0, v});
            bool ok = false;
            while (!q.empty())
            {
                auto [du, u] = q.top();
                q.pop(), ++st.greedy_pops;
                if (seen[u] != stamp || du != d[u])
                    continue;
                if (color[u] & (U ^ covered))
                {
                    ans += du, covered |= color[u], src.push_back(u), ok = true;
                    break;
                }
                for (auto e : graph.adj[u])
                    if (seen[e.to] != stamp || du + e.w < d[e.to])
                        seen[e.to] = stamp, d[e.to] = du + e.w, q.push({d[e.to], e.to});
            }
            if (!ok)
                return fp::kInf;
        }
        return ans;
    };
    auto greedy_start = Clock::now();
    st.greedy_upper = Greedy(best_root);
    st.greedy_ms = std::chrono::duration<double, std::milli>(Clock::now() - greedy_start).count();
    best = std::min(best, st.greedy_upper);
    st.preprocess_ms = std::chrono::duration<double, std::milli>(Clock::now() - solve_start).count();

    auto dp_start = Clock::now();
    std::vector<int> row(S, -1);
    int rows = 1;
    row[0] = 0;
    for (int s : order)
    {
        if (pc[s] > H)
            break;
        row[s] = rows++;
    }
    std::vector<double> dp(static_cast<size_t>(rows) * N, fp::kInf);
    std::vector<int> cover(static_cast<size_t>(rows) * N);
    std::vector<uint64_t> exact((static_cast<size_t>(rows) * N + 63) >> 6);
    std::vector<uint64_t> in_active((static_cast<size_t>(rows) * N + 63) >> 6);
    long long exact_version = 1;
    std::vector<std::vector<int>> active(S);
    std::vector<std::vector<int>> bucket(static_cast<size_t>(N) * (H + 1));
    std::vector<std::vector<int>> exact_bucket(static_cast<size_t>(N) * (H + 1));
    auto Id = [&](int s, int v) { return static_cast<size_t>(row[s]) * N + v; };
    auto D = [&](int s, int v) -> double& { return dp[Id(s, v)]; };
    auto C = [&](int s, int v) -> int& { return cover[Id(s, v)]; };
    auto Exact = [&](int s, int v)
    {
        size_t id = Id(s, v);
        return exact[id >> 6] >> (id & 63) & 1;
    };
    auto MarkExact = [&](int s, int v)
    {
        size_t id = Id(s, v);
        if (exact[id >> 6] >> (id & 63) & 1)
            return;
        exact[id >> 6] |= 1ULL << (id & 63);
        ++exact_version;
        exact_bucket[static_cast<size_t>(v) * (H + 1) + pc[s]].push_back(s);
    };
    auto Activate = [&](int s, int v)
    {
        size_t id = Id(s, v);
        if (in_active[id >> 6] >> (id & 63) & 1)
            return;
        in_active[id >> 6] |= 1ULL << (id & 63);
        active[s].push_back(v);
    };
    auto Bucket = [&](int v, int k) -> std::vector<int>&
    { return bucket[static_cast<size_t>(v) * (H + 1) + k]; };
    for (int v = 1; v <= n; ++v)
        D(0, v) = 0;
    auto AddKnownState = [&](int s, int v, double w, int c, bool add_active)
    {
        if (D(s, v) == fp::kInf)
        {
            if (add_active)
                Activate(s, v);
            Bucket(v, pc[s]).push_back(s);
        }
        else if (add_active)
            Activate(s, v);
        D(s, v) = w, C(s, v) = c;
    };
    auto SetDp = [&](int s, int v, double w, int c)
    {
        AddKnownState(s, v, w, c, true);
    };

    for (int a = 0; a < g; ++a)
    {
        int s = 1 << a;
        for (int v = 1; v <= n; ++v)
        {
            AddKnownState(s, v, gd[a][v], s | color[v], false);
            MarkExact(s, v);
        }
    }

    auto PairOwnValue = [&](int s, int v)
    {
        if (pc[s] != 2)
            return fp::kInf;
        int a = 0;
        while (!(s >> a & 1))
            ++a;
        int b = a + 1;
        while (!(s >> b & 1))
            ++b;
        return gd[a][v] + gd[b][v];
    };
    auto PairOwnMerge = [&](int s, int v)
    {
        double w = PairOwnValue(s, v);
        if (w < D(s, v))
            SetDp(s, v, w, s | color[v]);
    };

    auto pair_seed_start = Clock::now();
    if (H >= 2)
        for (int a = 0; a < g; ++a)
            for (int b = a + 1; b < g; ++b)
            {
                int s = (1 << a) | (1 << b);
                size_t base = static_cast<size_t>(row[s]) * N;
                active[s].reserve(n);
                for (int v = 1; v <= n; ++v)
                {
                    size_t id = base + v;
                    dp[id] = gd[a][v] + gd[b][v];
                    cover[id] = s | color[v];
                    in_active[id >> 6] |= 1ULL << (id & 63);
                    active[s].push_back(v);
                    Bucket(v, 2).push_back(s);
                }
            }
    st.pair_ms += std::chrono::duration<double, std::milli>(Clock::now() - pair_seed_start).count();

    auto Certify = [&](int s, int v, double w, int c)
    {
        ++st.certify_try, ++st.certify_try_by_size[pc[s]];
        double lower = LowerBound(v, s);
        if (w > lower + 1e-7)
        {
            if (c == s || pc[c] > H)
                return;
        }
        else
        {
            ++st.certify_success, ++st.certify_success_by_size[pc[s]];
            ++st.certify_by_lb;
            MarkExact(s, v);
        }
        if (c != s && pc[c] <= H)
        {
            ++st.cover_upgrade_try;
            if (w <= LowerBound(v, c) + 1e-7)
            {
                ++st.cover_upgrade_success;
                if (w < D(c, v))
                    SetDp(c, v, w, c);
                if (std::abs(w - D(c, v)) <= 1e-7)
                    MarkExact(c, v);
            }
        }
    };
    auto Modify = [&](int s, int v, double w)
    {
        Certify(s, v, w, C(s, v));
        ++st.total_valid, ++st.valid_by_size[pc[s]];
        int lim = H - pc[s], rem = U ^ s;
        auto TryMerge = [&](int t)
        {
            ++st.merge_enum;
            if (t & s)
                return;
            double dt = D(t, v);
            if (dt == fp::kInf)
                return;
            double cand = w + dt;
            if (cand >= best)
            {
                ++st.prune_ge_best;
                return;
            }
            int nxt = s | t;
            if (cand + Far(v, U ^ nxt) >= best)
            {
                ++st.prune_far;
                return;
            }
            ++st.merge_live_checks, ++st.merge_by_size[pc[s]];
            if (cand < D(nxt, v))
                SetDp(nxt, v, cand, C(s, v) | C(t, v));
        };
        long long bucket_cnt = 0;
        for (int sz = 1; sz <= lim; ++sz)
            bucket_cnt += Bucket(v, sz).size();
        if (CountRange(pc[rem], 1, lim) < bucket_cnt)
            EnumSubsets(rem, 1, lim, [&](int t)
            {
                TryMerge(t);
            });
        else
            for (int sz = 1; sz <= lim; ++sz)
                for (int t : Bucket(v, sz))
                    TryMerge(t);
    };
    for (int a = 0; a < g; ++a)
        for (int v : query.groups[a])
        {
            int s = 1 << a;
            if (0 < D(s, v))
                SetDp(s, v, 0, color[v] | s);
            else
                C(s, v) |= color[v] | s;
            Modify(s, v, 0);
        }

    auto OnlineH = [&](int s, int v)
    {
        ++st.h_calls;
        double ans = -1;
        int rem = U ^ s, lim = pc[s];
        auto TryH = [&](int t)
        {
            ++st.h_checks;
            if (t & s)
                return;
            if (!Exact(t, v))
                return;
            ++st.h_hits;
            ans = std::max(ans, D(t, v));
        };
        long long bucket_cnt = 0;
        for (int sz = 1; sz <= lim; ++sz)
            bucket_cnt += exact_bucket[static_cast<size_t>(v) * (H + 1) + sz].size();
        if (CountRange(pc[rem], 1, lim) < bucket_cnt)
            EnumSubsets(rem, 1, lim, [&](int t)
            {
                TryH(t);
            });
        else
            for (int sz = 1; sz <= lim; ++sz)
                for (int t : exact_bucket[static_cast<size_t>(v) * (H + 1) + sz])
                    TryH(t);
        return ans;
    };
    auto Complete = [&](int s, int v, double w)
    {
        ++st.complement_calls;
        int rem = U ^ s;
        double other = pc[rem] <= H ? D(rem, v) : fp::kInf;
        int lo = std::max(1, pc[rem] - H);
        auto TryComplement = [&](int x)
        {
            ++st.complement_enum;
            if ((x & rem) != x)
                return;
            int y = rem ^ x;
            if (x > y || pc[y] > H)
                return;
            double dx = D(x, v), dy = D(y, v);
            if (dx == fp::kInf || dy == fp::kInf)
                return;
            other = std::min(other, dx + dy);
        };
        long long bucket_cnt = 0;
        for (int sz = lo; sz <= H; ++sz)
            bucket_cnt += Bucket(v, sz).size();
        if (CountRange(pc[rem], lo, H) < bucket_cnt)
            EnumSubsets(rem, lo, H, [&](int x)
            {
                TryComplement(x);
            });
        else
            for (int sz = lo; sz <= H; ++sz)
                for (int x : Bucket(v, sz))
                    TryComplement(x);
        if (other < fp::kInf)
            ++st.complement_hits, best = std::min(best, w + other);
    };

    for (int s : order)
    {
        int k = pc[s];
        if (k > H)
            break;
        ++st.masks_processed;
        int rem = U ^ s;
        std::vector<double> lb(N, -1);
        auto Lb = [&](int v)
        {
            if (lb[v] < 0)
                ++st.lb_calls, lb[v] = LowerBound(v, rem);
            return lb[v];
        };
        std::vector<double> hv(N, -2);
        std::vector<long long> hv_ver(N, -1);
        auto Hval = [&](int v)
        {
            if (hv_ver[v] != exact_version)
                hv_ver[v] = exact_version, hv[v] = OnlineH(s, v);
            return hv[v];
        };
        Heap q;
        double blocked_min = fp::kInf;
        st.active_seed += active[s].size();
        st.active_by_size[k] += active[s].size();
        st.max_active = std::max(st.max_active, static_cast<int>(active[s].size()));
        for (int v : active[s])
        {
            if (k == 2)
                PairOwnMerge(s, v);
            double d = D(s, v);
            if (d + Far(v, rem) <= best && d + Lb(v) <= best && d + Hval(v) <= best)
                q.push({d, v}), ++st.pq_push;
            else
                blocked_min = std::min(blocked_min, d);
        }
        while (!q.empty())
        {
            auto [key, u] = q.top();
            q.pop(), ++st.pq_pop;
            double d = D(s, u);
            if (key != d)
                continue;
            if (d + Far(u, rem) > best)
            {
                blocked_min = std::min(blocked_min, d);
                continue;
            }
            if (d + Lb(u) > best)
            {
                blocked_min = std::min(blocked_min, d);
                continue;
            }
            if (d + Hval(u) > best)
            {
                blocked_min = std::min(blocked_min, d);
                continue;
            }
            if (k == 1 || (k == 2 && d <= blocked_min + 1e-9))
                MarkExact(s, u);
            Complete(s, u, d);
            ++st.total_inqueue, ++st.inqueue_by_size[k];
            Modify(s, u, d);
            for (auto e : graph.adj[u])
            {
                ++st.relax_try;
                double nd = d + e.w;
                if (nd >= D(s, e.to))
                    continue;
                if (nd >= best)
                {
                    ++st.prune_ge_best;
                    blocked_min = std::min(blocked_min, nd);
                    continue;
                }
                if (nd + Far(e.to, rem) > best)
                {
                    ++st.prune_far;
                    blocked_min = std::min(blocked_min, nd);
                    continue;
                }
                int nc = C(s, u) | color[e.to];
                if (k == 2)
                {
                    double own = PairOwnValue(s, e.to);
                    if (own < nd)
                        nd = own, nc = s | color[e.to];
                }
                if (nd >= D(s, e.to))
                    continue;
                SetDp(s, e.to, nd, nc);
                ++st.relax_ok, ++st.pq_push;
                q.push({nd, e.to});
            }
        }
    }

    st.dp_ms = std::chrono::duration<double, std::milli>(Clock::now() - dp_start).count();
    res.best_weight = best;
    res.feasible = true;
    return res;
}
}
