#include "test13.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <utility>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

namespace gst::methods::test13
{
SolveResult SolveOneQuery(const Graph& graph, const Query& query)
{
    using P = std::pair<double, int>;
    using Heap = std::priority_queue<P, std::vector<P>, std::greater<P>>;

    SolveResult res;
    auto& st = res.stats;
    const int n = graph.n, g = static_cast<int>(query.groups.size());
    st.n = n, st.m = graph.m, st.g = g;
    if (!g)
        return {0.0, true, {}};
    if (g > 22)
        throw std::runtime_error("Test13 supports group count <= 22.");
    if (!IsQueryFeasible(graph, query))
        return res;

    const auto solve_start = std::chrono::steady_clock::now();
    const int H = g / 2, S = 1 << g, U = S - 1, N = n + 1;
    std::vector<int> pc(S);
    std::vector<int> order;
    st.valid_by_size.assign(H + 1, 0);
    st.total_by_size.assign(H + 1, 0);
    st.active_by_size.assign(H + 1, 0);
    st.inqueue_by_size.assign(H + 1, 0);
    st.merge_by_size.assign(H + 1, 0);
    for (int s = 1; s < S; ++s)
    {
        pc[s] = pc[s >> 1] + (s & 1);
        if (pc[s] <= H)
            st.total_by_size[pc[s]] += n;
        order.push_back(s);
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) { return pc[a] < pc[b]; });

    std::vector<int> color(n + 1);
    for (int a = 0; a < g; ++a)
    {
        st.total_group_vertices += static_cast<int>(query.groups[a].size());
        st.max_group_size = std::max(st.max_group_size, static_cast<int>(query.groups[a].size()));
        for (int v : query.groups[a])
            color[v] |= 1 << a;
    }

    auto gd_start = std::chrono::steady_clock::now();
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
    st.group_dist_ms = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - gd_start)
                           .count();

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
            int b = bit0[s], pre = s ^ (1 << b);
            int old = far0[p0 + pre];
            far0[p0 + s] = old == 255 || gd[b][v] > gd[old][v] ? b : old;
        }
        size_t p1 = static_cast<size_t>(v) * S1;
        for (int s = 1; s < S1; ++s)
        {
            int b = G0 + bit1[s], pre = s ^ (1 << bit1[s]);
            int old = far1[p1 + pre];
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

    auto greedy_start = std::chrono::steady_clock::now();
    st.greedy_upper = Greedy(best_root);
    st.greedy_ms = std::chrono::duration<double, std::milli>(
                       std::chrono::steady_clock::now() - greedy_start)
                       .count();
    best = std::min(best, st.greedy_upper);
    st.preprocess_ms = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - solve_start)
                           .count();

    auto dp_start = std::chrono::steady_clock::now();
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
    std::vector<uint64_t> confirmed((static_cast<size_t>(rows) * N + 63) >> 6);
    std::vector<std::vector<int>> active(S);
    std::vector<std::vector<int>> finite_bucket(static_cast<size_t>(N) * (H + 1));
    std::vector<std::vector<int>> confirmed_bucket(static_cast<size_t>(N) * (H + 1));
    auto FiniteBucket = [&](int v, int k) -> std::vector<int>&
    {
        return finite_bucket[static_cast<size_t>(v) * (H + 1) + k];
    };
    auto ConfirmedBucket = [&](int v, int k) -> std::vector<int>&
    {
        return confirmed_bucket[static_cast<size_t>(v) * (H + 1) + k];
    };
    auto Id = [&](int s, int v)
    {
        return static_cast<size_t>(row[s]) * N + v;
    };
    auto D = [&](int s, int v) -> double&
    {
        return dp[Id(s, v)];
    };
    auto MarkConfirmed = [&](int s, int v)
    {
        size_t id = Id(s, v);
        if (confirmed[id >> 6] >> (id & 63) & 1)
            return;
        confirmed[id >> 6] |= 1ULL << (id & 63);
        ConfirmedBucket(v, pc[s]).push_back(s);
    };
    for (int v = 1; v <= n; ++v)
        D(0, v) = 0;

    auto SetDp = [&](int s, int v, double w)
    {
        if (D(s, v) == fp::kInf)
        {
            active[s].push_back(v);
            FiniteBucket(v, pc[s]).push_back(s);
        }
        D(s, v) = w;
    };

    auto Modify = [&](int s, int v, double w)
    {
        SetDp(s, v, w);
        MarkConfirmed(s, v);
        ++st.total_valid, ++st.valid_by_size[pc[s]];

        int lim = H - pc[s];
        for (int sz = 1; sz <= lim; ++sz)
            for (int t : FiniteBucket(v, sz))
            {
                ++st.merge_enum;
                if (t & s)
                    continue;
                double cand = w + D(t, v);
                if (cand >= best)
                {
                    ++st.prune_ge_best;
                    continue;
                }
                int nxt = s | t;
                if (cand + Far(v, U ^ nxt) >= best)
                {
                    ++st.prune_far;
                    continue;
                }
                ++st.merge_live_checks, ++st.merge_by_size[pc[s]];
                if (cand < D(nxt, v))
                    SetDp(nxt, v, cand);
            }
    };

    for (int a = 0; a < g; ++a)
        for (int v : query.groups[a])
            Modify(1 << a, v, 0);

    auto OnlineH = [&](int mask, int v)
    {
        ++st.h_calls;
        double ans = -1;
        int lim = pc[mask];
        for (int sz = 1; sz <= lim; ++sz)
            for (int t : ConfirmedBucket(v, sz))
            {
                ++st.h_checks;
                if (t & mask)
                continue;
            ++st.h_hits;
            if (D(t, v) > ans)
                    ans = D(t, v);
            }
        return ans;
    };

    auto Complete = [&](int mask, int v, double w)
    {
        ++st.complement_calls;
        int rem = U ^ mask;
        double other = fp::kInf;
        if (pc[rem] <= H && D(rem, v) < other)
        {
            ++st.complement_enum;
            other = D(rem, v);
        }
        int low = std::max(1, pc[rem] - H);
        for (int sz = low; sz <= H; ++sz)
            for (int x : FiniteBucket(v, sz))
            {
                ++st.complement_enum;
                if ((x & rem) != x)
                    continue;
                int y = rem ^ x;
                if (x > y || pc[y] > H)
                    continue;
                double cand = D(x, v) + D(y, v);
                if (cand < other)
                    other = cand;
            }
        if (other < fp::kInf)
        {
            ++st.complement_hits;
            best = std::min(best, w + other);
        }
    };

    std::vector<int> status(N);
    int stamp = 0;
    for (int s : order)
    {
        int k = pc[s];
        if (k > H)
            break;
        ++stamp, ++st.masks_processed;
        int rem = U ^ s;
        std::vector<double> lb(N, -1);
        auto Lb = [&](int v)
        {
            if (lb[v] < 0)
                ++st.lb_calls, lb[v] = LowerBound(v, rem);
            return lb[v];
        };

        Heap q;
        double mx = 0;
        int targets = 0;
        st.active_seed += active[s].size();
        st.active_by_size[k] += active[s].size();
        st.max_active = std::max(st.max_active, static_cast<int>(active[s].size()));
        for (int v : active[s])
        {
            double d = D(s, v);
            double hv = OnlineH(s, v);
            if (d + hv <= best && d + Lb(v) <= best)
                q.push({d + Lb(v), v}), ++st.pq_push, ++targets,
                    status[v] = stamp, mx = std::max(mx, d);
        }
        for (int v : active[s])
            if (status[v] != stamp && D(s, v) < mx)
                q.push({D(s, v) + Lb(v), v}), ++st.pq_push;

        while (!q.empty() && targets)
        {
            auto [key, u] = q.top();
            q.pop(), ++st.pq_pop;
            double d = D(s, u);
            if (key != d + Lb(u) || d + Lb(u) > best)
                continue;
            Complete(s, u, d);
            ++st.total_inqueue, ++st.inqueue_by_size[k];
            if (status[u] == stamp)
                --targets, Modify(s, u, d);
            for (auto e : graph.adj[u])
            {
                ++st.relax_try;
                double nd = d + e.w;
                if (nd >= D(s, e.to) || nd + Lb(e.to) > best)
                    continue;
                if (status[u] != status[e.to])
                {
                    status[e.to] = status[u];
                    if (status[e.to] == stamp)
                        ++targets;
                }
                SetDp(s, e.to, nd);
                ++st.relax_ok, ++st.pq_push;
                q.push({nd + Lb(e.to), e.to});
            }
        }
    }

    st.dp_ms = std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - dp_start)
                   .count();
    res.best_weight = best;
    res.feasible = true;
    return res;
}

}  // namespace gst::methods::test13
