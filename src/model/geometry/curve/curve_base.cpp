/**
 * @file        curve_base.cpp
 * @brief       空间曲线抽象基类实现。
 * @details     实现 CurveBase 的默认 sample() 方法，
 *              在参数范围内均匀采样 evaluate() 生成线条顶点序列。
 *
 * @ingroup     Physics
 */

#include <algorithm>

#include "curve_base.h"

namespace egret
{
    CurvePoints CurveBase::sample(const SampleConfig& config) const
    {
        CurvePoints result;

        const double tStart = getStartT();
        const double tEnd   = getEndT();
        const double tRange = tEnd - tStart;

        // 至少 2 个顶点（1 条线段）
        const int numPoints = std::max(2, config.segments + 1);
        result.vertices.reserve(static_cast<std::size_t>(numPoints));

        const double invSegments = 1.0 / static_cast<double>(
            std::max(1, config.segments));

        for (int i = 0; i < numPoints; ++i) {
            const double t = tStart + static_cast<double>(i) * invSegments * tRange;
            result.vertices.push_back(evaluate(t));
        }

        return result;
    }
} // egret
