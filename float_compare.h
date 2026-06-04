#ifndef GST_FLOAT_COMPARE_H
#define GST_FLOAT_COMPARE_H

#include <cmath>

namespace gst::fp
{

constexpr double kEps = 1e-9;
constexpr double kInf = 1e100;

inline int Cmp(double a, double b, double eps = kEps)
{
    if (std::fabs(a - b) <= eps)
    {
        return 0;
    }
    return (a < b) ? -1 : 1;
}

// 全局统一的实数比较接口，避免不同模块各自写 eps 判断。
inline bool Eq(double a, double b, double eps = kEps)
{
    return Cmp(a, b, eps) == 0;
}

inline bool Lt(double a, double b, double eps = kEps)
{
    return Cmp(a, b, eps) < 0;
}

inline bool Le(double a, double b, double eps = kEps)
{
    return Cmp(a, b, eps) <= 0;
}

}  // namespace gst::fp

#endif  // GST_FLOAT_COMPARE_H
