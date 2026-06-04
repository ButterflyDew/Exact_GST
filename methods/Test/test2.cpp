#include "test2.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <stdexcept>
#include <utility>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

namespace gst::methods::test2
{

namespace
{

struct HeapNode
{
    double d = fp::kInf;
    int v = -1;
    int mask = 0;
};

class BinaryHeap
{
public:
    bool Empty() const
    {
        return a_.empty();
    }

    const HeapNode& Top() const
    {
        return a_[0];
    }

    void DecreaseOrPush(int mask, int v, double d)
    {
        const std::uint64_t key = Key(mask, v);
        auto it = pos_.find(key);
        if (it == pos_.end())
        {
            int idx = static_cast<int>(a_.size());
            a_.push_back({d, v, mask});
            pos_[key] = idx;
            SiftUp(idx);
            return;
        }
        int idx = it->second;
        if (fp::Lt(d, a_[idx].d))
        {
            a_[idx].d = d;
            SiftUp(idx);
        }
    }

    HeapNode Pop()
    {
        HeapNode ret = a_[0];
        const std::uint64_t ret_key = Key(ret.mask, ret.v);
        pos_.erase(ret_key);
        if (a_.size() == 1)
        {
            a_.pop_back();
            return ret;
        }
        a_[0] = a_.back();
        a_.pop_back();
        pos_[Key(a_[0].mask, a_[0].v)] = 0;
        SiftDown(0);
        return ret;
    }

private:
    static std::uint64_t Key(int mask, int v)
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(mask)) << 32) |
               static_cast<std::uint32_t>(v);
    }

    static bool Less(const HeapNode& x, const HeapNode& y)
    {
        if (!fp::Eq(x.d, y.d))
        {
            return fp::Lt(x.d, y.d);
        }
        if (x.mask != y.mask)
        {
            return x.mask < y.mask;
        }
        return x.v < y.v;
    }

    void SiftUp(int i)
    {
        while (i > 0)
        {
            int p = (i - 1) >> 1;
            if (!Less(a_[i], a_[p]))
            {
                break;
            }
            pos_[Key(a_[i].mask, a_[i].v)] = p;
            pos_[Key(a_[p].mask, a_[p].v)] = i;
            std::swap(a_[i], a_[p]);
            i = p;
        }
    }

    void SiftDown(int i)
    {
        int n = static_cast<int>(a_.size());
        while (true)
        {
            int l = (i << 1) + 1;
            int r = l + 1;
            int k = i;
            if (l < n && Less(a_[l], a_[k]))
            {
                k = l;
            }
            if (r < n && Less(a_[r], a_[k]))
            {
                k = r;
            }
            if (k == i)
            {
                break;
            }
            pos_[Key(a_[i].mask, a_[i].v)] = k;
            pos_[Key(a_[k].mask, a_[k].v)] = i;
            std::swap(a_[i], a_[k]);
            i = k;
        }
    }

    std::vector<HeapNode> a_;
    std::unordered_map<std::uint64_t, int> pos_;
};

long long Comb(int n, int k)
{
    if (k < 0 || k > n)
    {
        return 0;
    }
    k = std::min(k, n - k);
    long long ans = 1;
    for (int i = 1; i <= k; ++i)
    {
        ans = ans * (n - k + i) / i;
    }
    return ans;
}

}  // namespace

SolveResult SolveOneQuery(const Graph& graph, const Query& query, dpbf::OutputMode /*output_mode*/)
{
    const int g = static_cast<int>(query.groups.size());
    const int n = graph.n;
    SolveResult result;
    if (g == 0)
    {
        result.best_weight = 0.0;
        result.feasible = true;
        return result;
    }
    if (g >= 23)
    {
        throw std::runtime_error("Test2 currently supports group count <= 22.");
    }

    const int full = (1 << g) - 1;
    const int h = g / 2;
    std::vector<std::vector<double>> dp(1 << g, std::vector<double>(n + 1, fp::kInf));
    std::vector<std::vector<double>> gval(1 << g, std::vector<double>(n + 1, -1.0));
    std::vector<int> bit(1 << g, 0);
    for (int mask = 1; mask <= full; ++mask)
    {
        bit[mask] = bit[mask >> 1] + (mask & 1);
    }

    std::vector<BinaryHeap> heaps(h + 1);
    for (int gi = 0; gi < g; ++gi)
    {
        const int m = (1 << gi);
        for (int v : query.groups[gi])
        {
            if (v < 1 || v > n)
            {
                throw std::runtime_error("Query vertex id out of range.");
            }
            dp[m][v] = 0.0;
            gval[m][v] = 0.0;
            if (h >= 1)
            {
                heaps[1].DecreaseOrPush(m, v, 0.0);
            }
        }
    }

    if (!IsQueryFeasible(graph, query))
    {
        result.best_weight = -1.0;
        result.feasible = false;
        result.stats.valid_by_size.assign(g + 1, 0);
        result.stats.total_by_size.assign(g + 1, 0);
        return result;
    }

    double best = fp::kInf;
    std::vector<std::vector<double>> f(n + 1, std::vector<double>(1 << g, fp::kInf));
    for (int v = 1; v <= n; ++v)
    {
        f[v][0] = 0.0;
    }
    std::vector<char> root_alive(n + 1, 1);
    std::vector<long long> valid_by_size(g + 1, 0);
    std::vector<long long> total_by_size(g + 1, 0);
    for (int k = 1; k <= h; ++k)
    {
        total_by_size[k] = Comb(g, k) * n;
    }

    for (int x = 1; x <= h; ++x)
    {
        std::vector<std::unordered_map<int, double>> changed_items(n + 1);

        while (!heaps[x].Empty())
        {
            const HeapNode top = heaps[x].Top();
            if (!fp::Lt(top.d, best))
            {
                break;
            }
            HeapNode cur = heaps[x].Pop();
            if (!root_alive[cur.v])
            {
                continue;
            }
            if (bit[cur.mask] != x)
            {
                continue;
            }
            if (fp::Cmp(cur.d, dp[cur.mask][cur.v]) != 0)
            {
                continue;
            }
            if (x <= h && fp::Lt(gval[cur.mask][cur.v], cur.d))
            {
                gval[cur.mask][cur.v] = cur.d;
                changed_items[cur.v][cur.mask] = cur.d;
            }

            // 第一类：同根并集转移。
            const int rem = full ^ cur.mask;
            for (int add = rem; add > 0; add = (add - 1) & rem)
            {
                const int t = cur.mask | add;
                const int ts = bit[t];
                if (ts > h && t != full)
                {
                    continue;
                }
                const double rhs = dp[add][cur.v];
                if (!fp::Lt(rhs, fp::kInf / 4))
                {
                    continue;
                }
                const double cand = cur.d + rhs;
                if (ts <= h && fp::Lt(gval[t][cur.v], cur.d))
                {
                    gval[t][cur.v] = cur.d;
                    changed_items[cur.v][t] = cur.d;
                }
                if (fp::Lt(cand, dp[t][cur.v]))
                {
                    dp[t][cur.v] = cand;
                    if (t == full && fp::Lt(cand, best))
                    {
                        best = cand;
                    }
                    if (ts <= h)
                    {
                        heaps[ts].DecreaseOrPush(t, cur.v, cand);
                    }
                }
            }

            // 第二类：图边扩展。
            for (const auto& e : graph.adj[cur.v])
            {
                const double cand = cur.d + e.w;
                if (fp::Lt(cand, dp[cur.mask][e.to]))
                {
                    dp[cur.mask][e.to] = cand;
                    heaps[x].DecreaseOrPush(cur.mask, e.to, cand);
                }
            }
        }

        // 把本轮新增 g 物品并入全局 f（01 背包）。
        for (int v = 1; v <= n; ++v)
        {
            if (!root_alive[v] || changed_items[v].empty())
            {
                continue;
            }
            for (const auto& it : changed_items[v])
            {
                const int t = it.first;
                const double w = it.second;
                for (int p = full; p >= 0; --p)
                {
                    if ((p & t) != t)
                    {
                        continue;
                    }
                    const double prev = f[v][p ^ t];
                    if (!fp::Lt(prev, fp::kInf / 4))
                    {
                        continue;
                    }
                    const double cand = prev + w;
                    if (fp::Lt(cand, f[v][p]))
                    {
                        f[v][p] = cand;
                    }
                }
            }
            if (fp::Lt(f[v][full], best))
            {
                best = f[v][full];
            }
        }

        // 筛有效状态，并按根剪枝。
        std::vector<long long> cur_valid(g + 1, 0);
        for (int v = 1; v <= n; ++v)
        {
            if (!root_alive[v])
            {
                continue;
            }
            bool has_valid = false;
            for (int s = 1; s <= full; ++s)
            {
                const int k = bit[s];
                if (k < 1 || k > h)
                {
                    continue;
                }
                const double gv = gval[s][v];
                if (gv < 0.0)
                {
                    continue;
                }
                const double rhs = f[v][full ^ s];
                if (!fp::Lt(rhs, fp::kInf / 4))
                {
                    continue;
                }
                if (fp::Lt(gv + rhs, best))
                {
                    ++cur_valid[k];
                    has_valid = true;
                }
            }
            if (!has_valid)
            {
                root_alive[v] = 0;
            }
        }
        valid_by_size.swap(cur_valid);

        if (x == h)
        {
            break;
        }
    }

    result.stats.valid_by_size = std::move(valid_by_size);
    result.stats.total_by_size = std::move(total_by_size);
    for (int k = 1; k <= h; ++k)
    {
        result.stats.total_valid += result.stats.valid_by_size[k];
    }

    if (!fp::Lt(best, fp::kInf / 4))
    {
        result.best_weight = -1.0;
        result.feasible = false;
        return result;
    }
    result.best_weight = best;
    result.feasible = true;
    return result;
}

}  // namespace gst::methods::test2
