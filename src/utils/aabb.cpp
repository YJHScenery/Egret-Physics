//
// Created by jehor on 2026/6/9.
//

#include "aabb.h"

namespace egret
{
    void AABB::expand(const AABB& other)
    {
        min = min.cwiseMin(other.min);
        max = max.cwiseMax(other.max);
    }

    // 相交检测（包含边界相接的情况）
    bool AABB::intersects(const AABB& other) const
    {
        return (min.x() <= other.max.x() && max.x() >= other.min.x()) &&
            (min.y() <= other.max.y() && max.y() >= other.min.y()) &&
            (min.z() <= other.max.z() && max.z() >= other.min.z());
    }

    // 严格相交检测（排除边界只相接的情况，必须内部真正重叠）
    bool AABB::intersects_strict(const AABB& other) const
    {
        return (min.x() < other.max.x() && max.x() > other.min.x()) &&
            (min.y() < other.max.y() && max.y() > other.min.y()) &&
            (min.z() < other.max.z() && max.z() > other.min.z());
    }

    // 带容差的相交检测
    bool AABB::intersects_with_tolerance(const AABB& other, double epsilon) const
    {
        return (min.x() <= other.max.x() + epsilon && max.x() + epsilon >= other.min.x()) &&
            (min.y() <= other.max.y() + epsilon && max.y() + epsilon >= other.min.y()) &&
            (min.z() <= other.max.z() + epsilon && max.z() + epsilon >= other.min.z());
    }
} // egret
