#ifndef GST_METHODS_TEST_TEST19_FUTURE_BOUNDS_H
#define GST_METHODS_TEST_TEST19_FUTURE_BOUNDS_H

#include <cstddef>
#include <vector>

namespace gst::methods::test19
{
class MetricTspLowerBound
{
public:
    void Build(const std::vector<std::vector<double>>& group_metric, int full_mask);

    double TourHalf(int rem_mask, int root, const std::vector<std::vector<double>>& group_dist) const;

private:
    struct EndpointPair
    {
        int a = 0;
        int b = 0;
        double path = 0.0;
    };

    size_t PathIndex(int mask, int start, int last) const;

    int g_ = 0;
    std::vector<double> path_;
    std::vector<std::vector<EndpointPair>> endpoint_pairs_;
};
}  // namespace gst::methods::test19

#endif
