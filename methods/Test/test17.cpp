#include "test17.h"

#include <algorithm>
#include <chrono>
#include <queue>
#include <stdexcept>
#include <utility>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

namespace gst::methods::test17
{
namespace
{
struct State
{
    std::vector<int> v, cover;
    std::vector<double> d;
    bool ready = false;
};

int FirstBit(int s)
{
    int a = 0;
    while (!(s >> a & 1))
        ++a;
    return a;
}

int NextBit(int s, int a)
{
    do
        ++a;
    while (!(s >> a & 1));
    return a;
}

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
        throw std::runtime_error("Test16 supports group count <= 22.");
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
                        seen[e.to] = stamp, d[e.to] = du + e.w, q.push({d[e.to], e.to});
            }
            if (!ok)
                return fp::kInf;
        }
        return ans;
    };
    const auto greedy_start = Clock::now();
    st.greedy_upper = Greedy(best_root);
    st.greedy_ms = std::chrono::duration<double, std::milli>(Clock::now() - greedy_start).count();
    best = std::min(best, st.greedy_upper);
    st.preprocess_ms = std::chrono::duration<double, std::milli>(Clock::now() - solve_start).count();

    const auto dp_start = Clock::now();
    std::vector<State> state(S);
    for (int a = 0; a < g; ++a)
    {
        int s = 1 << a;
        auto& z = state[s];
        z.v.resize(n), z.d.resize(n), z.cover.resize(n);
        for (int v = 1; v <= n; ++v)
        {
            int i = v - 1;
            z.v[i] = v;
            z.d[i] = gd[a][v];
            z.cover[i] = s | color[v];
        }
        z.ready = true;
        st.finite_states += n, st.confirmed_states += n;
    }

    auto Available = [&](int s) { return s && pc[s] <= H && state[s].ready; };
    auto Lookup = [&](int s, int v) -> double
    {
        if (!s)
            return 0.0;
        if (pc[s] == 1)
            return gd[FirstBit(s)][v];
        if (!Available(s))
            return fp::kInf;
        const auto& z = state[s];
        auto it = std::lower_bound(z.v.begin(), z.v.end(), v);
        if (it == z.v.end() || *it != v)
            return fp::kInf;
        return z.d[it - z.v.begin()];
    };
    std::vector<double> dist(N, fp::kInf), lb(N);
    std::vector<int> cov(N), touched, kept, exact_mark(N);
    std::vector<std::pair<int, int>> complete_pairs;
    int stamp = 0;

    for (int s : order)
    {
        const int k = pc[s], rem = U ^ s;
        if (k == 1)
            continue;
        ++stamp, ++st.masks_processed;
        touched.clear();
        kept.clear();

        auto Set = [&](int v, double w, int c)
        {
            if (w >= dist[v])
                return;
            if (dist[v] == fp::kInf)
                touched.push_back(v);
            dist[v] = w, cov[v] = c;
        };

        const auto pull_begin = Clock::now();
        const long long hits_before = st.pull_hits;
        if (k == 2)
        {
            int a = FirstBit(s), b = NextBit(s, a);
            ++st.pull_pairs;
            st.pull_scan += n;
            for (int v = 1; v <= n; ++v)
                Set(v, gd[a][v] + gd[b][v], s | color[v]), ++st.pull_hits;
        }
        else
        {
            for (int a = (s - 1) & s; a; a = (a - 1) & s)
            {
                int b = s ^ a;
                if (!b || a > b || !Available(a) || !Available(b))
                    continue;
                ++st.pull_pairs;
                const auto& x = state[a];
                const auto& y = state[b];
                JoinRows(x.v, x.d, y.v, y.d,
                         [&](int v, double da, double db, size_t ia, size_t ib)
                         {
                             double cand = da + db;
                             if (cand >= best)
                                 ++st.prune_ge_best;
                             else if (cand + Far(v, U ^ s) >= best)
                                 ++st.prune_far;
                             else
                                 Set(v, cand, x.cover[ia] | y.cover[ib]);
                         },
                         st.pull_scan, st.pull_hits);
            }
        }
        st.pull_ms += std::chrono::duration<double, std::milli>(Clock::now() - pull_begin).count();
        st.merge_by_size[k] += st.pull_hits - hits_before;

        st.active_seed += touched.size();
        st.active_by_size[k] += touched.size();
        st.max_active = std::max(st.max_active, static_cast<int>(touched.size()));

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
                best = std::min(best, d + other);
            st.complement_ms += std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        };

        std::fill(lb.begin(), lb.end(), -1);
        auto Lb = [&](int v)
        {
            if (lb[v] < 0)
                ++st.lb_calls, lb[v] = LowerBound(v, rem);
            return lb[v];
        };
        auto MarkExact = [&](int v)
        {
            if (exact_mark[v] != stamp)
                exact_mark[v] = stamp, ++st.total_valid, ++st.valid_by_size[k];
        };
        auto Certify = [&](int v, double d)
        {
            ++st.certify_try;
            if (d <= LowerBound(v, s) + 1e-7)
                ++st.certify_success, ++st.certify_by_lb, MarkExact(v);
        };

        const auto search_begin = Clock::now();
        Heap q;
        double blocked_min = fp::kInf;
        for (int v : touched)
        {
            double d = dist[v];
            if (d + Far(v, rem) <= best && d + Lb(v) <= best)
                q.push({d, v}), ++st.pq_push;
            else
                blocked_min = std::min(blocked_min, d);
        }

        while (!q.empty())
        {
            auto [key, u] = q.top();
            q.pop(), ++st.pq_pop;
            double d = dist[u];
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
            if (k == 2 && d <= blocked_min + 1e-9)
                ++st.certify_success, ++st.certify_by_pop, MarkExact(u);
            Certify(u, d);
            Complete(u, d);
            kept.push_back(u);
            ++st.total_inqueue, ++st.inqueue_by_size[k];
            for (auto e : graph.adj[u])
            {
                ++st.relax_try;
                double nd = d + e.w;
                if (nd >= dist[e.to])
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
        auto& z = state[s];
        z.v.clear(), z.d.clear(), z.cover.clear();
        z.v.reserve(kept.size()), z.d.reserve(kept.size()), z.cover.reserve(kept.size());
        long long confirmed = 0;
        for (int v : kept)
        {
            z.v.push_back(v), z.d.push_back(dist[v]), z.cover.push_back(cov[v]);
            confirmed += exact_mark[v] == stamp;
        }
        for (int v : touched)
        {
            dist[v] = fp::kInf, exact_mark[v] = 0;
        }
        z.ready = true;
        st.finite_states += z.v.size();
        st.confirmed_states += confirmed;
    }

    st.dp_ms = std::chrono::duration<double, std::milli>(Clock::now() - dp_start).count();
    res.best_weight = best;
    res.feasible = true;
    return res;
}

}  // namespace gst::methods::test17
