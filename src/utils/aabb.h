//
// Created by jehor on 2026/6/9.
//

#ifndef EGRET_PHYSICS_AABB_H
#define EGRET_PHYSICS_AABB_H
#include <Eigen/Dense>

namespace egret
{
    /**
     * @brief       AABB 包围盒结构体，存储轴对齐包围盒的最小和最大点。
     * @details     AABB 是 AABB 包围盒结构体，存储轴对齐包围盒的最小和最大点。
     *              用于物理引擎的广相位碰撞检测和空间划分。
     *              AABB 是最简单的包围盒类型，计算效率高。
     *
     * @invariant   min <= max，最小点坐标小于最大点坐标
     * @see         BroadPhaseStrategy, ShapeBase
     */
    struct AABB
    {
        Eigen::Vector3d min{};
        Eigen::Vector3d max{};

        void expand(const AABB& other);

        // 相交检测函数
        bool intersects(const AABB& other) const;

        // 可选：更严格的相交检测（排除边界相接的情况）
        bool intersects_strict(const AABB& other) const;

        // 可选：带容差的相交检测
        bool intersects_with_tolerance(const AABB& other, double epsilon = 1e-6) const;
    };
} // egret

#endif //EGRET_PHYSICS_AABB_H
