#include "test14.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <utility>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

namespace gst::methods::test14
{
namespace
{
struct State
{
    std::vector<int> v, cv;
    std::vector<double> d, cd;
    std::vector<unsigned char> exact;
    bool ready = false;
};
}

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
        throw std::runtime_error("Test14 supports group count <= 22.");
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
    for (int s = 1; s < S; ++s)
    {
        pc[s] = pc[s >> 1] + (s & 1);
        if (pc[s] <= H)
            order.push_back(s), st.total_by_size[pc[s]] += n;
    }
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return pc[a] != pc[b] ? pc[a] < pc[b] : a < b; });

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
    st.group_dist_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - gd_start).count();

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
                        seen[e.to] = stamp, d[e.to] = du + e.w,
                        q.push({d[e.to], e.to});
            }
            if (!ok)
                return fp::kInf;
        }
        return ans;
    };

    const auto greedy_start = Clock::now();
    st.greedy_upper = Greedy(best_root);
    st.greedy_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - greedy_start).count();
    best = std::min(best, st.greedy_upper);
    st.preprocess_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - solve_start).count();

    const auto dp_start = Clock::now();
    std::vector<State> state(S);
    for (int a = 0; a < g; ++a)
    {
        auto& z = state[1 << a];
        z.v = query.groups[a];
        std::sort(z.v.begin(), z.v.end());
        z.v.erase(std::unique(z.v.begin(), z.v.end()), z.v.end());
        z.d.assign(z.v.size(), 0);
        z.exact.assign(z.v.size(), 1);
        z.cv = z.v;
        z.cd = z.d;
    }

    auto Available = [&](int s) { return state[s].ready || pc[s] == 1; };
    auto Join = [&](const std::vector<int>& av, const std::vector<double>& ad,
                    const std::vector<int>& bv, const std::vector<double>& bd,
                    auto&& use, long long& scans, long long& hits)
    {
        scans += av.size() + bv.size();
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
    };

    std::vector<double> dist(N, fp::kInf), h(N), comp(N), lb(N), zero(N);
    std::vector<int> touched, status(N), confirmed_stamp(N);
    int stamp = 0;

    for (int s : order)
    {
        const int k = pc[s], rem = U ^ s;
        ++stamp, ++st.masks_processed;
        touched.clear();

        auto Set = [&](int v, double w)
        {
            if (w >= dist[v])
                return;
            if (dist[v] == fp::kInf)
                touched.push_back(v);
            dist[v] = w;
        };

        if (k == 1)
            for (size_t i = 0; i < state[s].v.size(); ++i)
                Set(state[s].v[i], 0);
        else
        {
            const auto begin = Clock::now();
            const long long hits_before = st.pull_hits;
            for (int a = (s - 1) & s; a; a = (a - 1) & s)
            {
                int b = s ^ a;
                if (!b || a > b || !Available(a) || !Available(b))
                    continue;
                ++st.pull_pairs;
                const auto& x = state[a];
                const auto& y = state[b];
                long long raw_hits = 0;
                Join(x.v, x.d, y.v, y.d,
                     [&](int v, double da, double db, size_t ia, size_t ib)
                     {
                         if (!x.exact[ia] && !y.exact[ib])
                             return;
                         ++st.pull_hits;
                         double cand = da + db;
                         if (cand >= best)
                             ++st.prune_ge_best;
                         else if (cand + Far(v, rem) >= best)
                             ++st.prune_far;
                         else
                             Set(v, cand);
                     },
                     st.pull_scan, raw_hits);
            }
            st.pull_ms +=
                std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
            st.merge_by_size[k] += st.pull_hits - hits_before;
        }

        std::sort(touched.begin(), touched.end());
        st.active_seed += touched.size();
        st.active_by_size[k] += touched.size();
        st.max_active = std::max(st.max_active, static_cast<int>(touched.size()));

        std::fill(h.begin(), h.end(), -1);
        {
            const auto begin = Clock::now();
            for (int t = rem; t; t = (t - 1) & rem)
            {
                if (pc[t] > k || !Available(t))
                    continue;
                ++st.h_pairs;
                const auto& z = state[t];
                Join(touched, zero, z.cv, z.cd,
                     [&](int v, double, double w, size_t, size_t)
                     { h[v] = std::max(h[v], w); },
                     st.h_scan, st.h_hits);
            }
            st.h_ms +=
                std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        }

        std::fill(comp.begin(), comp.end(), fp::kInf);
        {
            const auto begin = Clock::now();
            for (int x = rem;; x = (x - 1) & rem)
            {
                int y = rem ^ x;
                if (x <= y && pc[x] <= H && pc[y] <= H &&
                    (!x || Available(x)) && (!y || Available(y)))
                {
                    ++st.complement_pairs;
                    if (!x || !y)
                    {
                        int z = x | y;
                        const auto& q = state[z];
                        st.complement_scan += q.v.size();
                        for (size_t i = 0; i < q.v.size(); ++i)
                            if (q.d[i] < comp[q.v[i]])
                                comp[q.v[i]] = q.d[i], ++st.complement_hits;
                    }
                    else
                    {
                        const auto& a = state[x];
                        const auto& b = state[y];
                        Join(a.v, a.d, b.v, b.d,
                             [&](int v, double da, double db, size_t, size_t)
                             {
                                 if (da + db < comp[v])
                                     comp[v] = da + db;
                             },
                             st.complement_scan, st.complement_hits);
                    }
                }
                if (!x)
                    break;
            }
            st.complement_ms +=
                std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        }

        std::fill(lb.begin(), lb.end(), -1);
        auto Lb = [&](int v)
        {
            if (lb[v] < 0)
                ++st.lb_calls, lb[v] = LowerBound(v, rem);
            return lb[v];
        };

        for (int v : touched)
            confirmed_stamp[v] = 0;
        if (k == 1)
            for (int v : state[s].cv)
                confirmed_stamp[v] = stamp;
        if (k == 1)
            st.total_valid += state[s].cv.size(),
                st.valid_by_size[k] += state[s].cv.size();

        const auto search_begin = Clock::now();
        Heap q;
        double mx = 0;
        int targets = 0;
        for (int v : touched)
            if (dist[v] + h[v] <= best && dist[v] + Lb(v) <= best)
                q.push({dist[v] + Lb(v), v}), ++st.pq_push, ++targets,
                    status[v] = stamp, mx = std::max(mx, dist[v]);
        for (int v : touched)
            if (status[v] != stamp && dist[v] < mx)
                q.push({dist[v] + Lb(v), v}), ++st.pq_push;

        while (!q.empty() && targets)
        {
            auto [key, u] = q.top();
            q.pop(), ++st.pq_pop;
            double d = dist[u];
            if (key != d + Lb(u) || d + Lb(u) > best)
                continue;
            if (comp[u] < fp::kInf)
                best = std::min(best, d + comp[u]);
            ++st.total_inqueue, ++st.inqueue_by_size[k];
            if (status[u] == stamp)
            {
                --targets;
                if (confirmed_stamp[u] != stamp)
                    confirmed_stamp[u] = stamp, ++st.total_valid,
                    ++st.valid_by_size[k];
            }
            for (auto e : graph.adj[u])
            {
                ++st.relax_try;
                double nd = d + e.w;
                if (nd >= dist[e.to] || nd + Lb(e.to) > best)
                    continue;
                if (dist[e.to] == fp::kInf)
                    touched.push_back(e.to);
                if (status[u] != status[e.to])
                {
                    status[e.to] = status[u];
                    if (status[e.to] == stamp)
                        ++targets;
                }
                dist[e.to] = nd;
                ++st.relax_ok, ++st.pq_push;
                q.push({nd + Lb(e.to), e.to});
            }
        }
        st.search_ms +=
            std::chrono::duration<double, std::milli>(Clock::now() - search_begin).count();

        std::sort(touched.begin(), touched.end());
        auto& z = state[s];
        z.v.clear(), z.d.clear(), z.cv.clear(), z.cd.clear(), z.exact.clear();
        z.v.reserve(touched.size()), z.d.reserve(touched.size());
        z.exact.reserve(touched.size());
        for (int v : touched)
        {
            z.v.push_back(v), z.d.push_back(dist[v]);
            z.exact.push_back(confirmed_stamp[v] == stamp);
            if (confirmed_stamp[v] == stamp)
                z.cv.push_back(v), z.cd.push_back(dist[v]);
            dist[v] = fp::kInf;
        }
        z.ready = true;
        st.finite_states += z.v.size();
        st.confirmed_states += z.cv.size();
    }

    st.dp_ms = std::chrono::duration<double, std::milli>(Clock::now() - dp_start).count();
    res.best_weight = best;
    res.feasible = true;
    return res;
}

}  // namespace gst::methods::test14
