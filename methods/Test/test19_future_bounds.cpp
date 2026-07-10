#include "test19_future_bounds.h"

#include <algorithm>

#include "../../float_compare.h"

namespace gst::methods::test19
{
namespace
{
int FirstBit(int mask)
{
    int bit = 0;
    while (!((mask >> bit) & 1))
        ++bit;
    return bit;
}

int PopCount(int mask)
{
    int count = 0;
    while (mask)
    {
        mask &= mask - 1;
        ++count;
    }
    return count;
}
}  // namespace

void MetricTspLowerBound::Build(const std::vector<std::vector<double>>& group_metric, int full_mask)
{
    g_ = static_cast<int>(group_metric.size());
    const int subset_count = 1 << g_;
    path_.assign(static_cast<size_t>(subset_count) * g_ * g_, fp::kInf);

    for (int start = 0; start < g_; ++start)
    {
        path_[PathIndex(1 << start, start, start)] = 0.0;
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (!((mask >> start) & 1))
                continue;
            for (int last = 0; last < g_; ++last)
            {
                if (!((mask >> last) & 1))
                    continue;
                const double cur = path_[PathIndex(mask, start, last)];
                if (cur >= fp::kInf / 4)
                    continue;
                const int rem = full_mask ^ mask;
                for (int t = rem; t; t &= t - 1)
                {
                    const int bit = t & -t;
                    const int next = FirstBit(bit);
                    double& dst = path_[PathIndex(mask | bit, start, next)];
                    dst = std::min(dst, cur + group_metric[last][next]);
                }
            }
        }
    }

    endpoint_pairs_.clear();
    endpoint_pairs_.resize(subset_count);
    for (int mask = 1; mask < subset_count; ++mask)
    {
        const int count = PopCount(mask);
        if (count <= 1)
            continue;
        auto& pairs = endpoint_pairs_[mask];
        pairs.reserve(count * (count - 1) / 2);
        for (int ta = mask; ta; ta &= ta - 1)
        {
            const int a = FirstBit(ta & -ta);
            for (int tb = mask & ~((1 << (a + 1)) - 1); tb; tb &= tb - 1)
            {
                const int b = FirstBit(tb & -tb);
                const double w = std::min(path_[PathIndex(mask, a, b)], path_[PathIndex(mask, b, a)]);
                if (w < fp::kInf / 4)
                    pairs.push_back({a, b, w});
            }
        }
    }
}

double MetricTspLowerBound::TourHalf(int rem_mask,
                                     int root,
                                     const std::vector<std::vector<double>>& group_dist) const
{
    if (!rem_mask)
        return 0.0;
    if (!(rem_mask & (rem_mask - 1)))
        return group_dist[FirstBit(rem_mask)][root];

    double tour = fp::kInf;
    for (const auto& endpoint : endpoint_pairs_[rem_mask])
    {
        tour = std::min(tour,
                        group_dist[endpoint.a][root] + endpoint.path + group_dist[endpoint.b][root]);
    }
    return tour * 0.5;
}

size_t MetricTspLowerBound::PathIndex(int mask, int start, int last) const
{
    return (static_cast<size_t>(mask) * g_ + start) * g_ + last;
}
}  // namespace gst::methods::test19
