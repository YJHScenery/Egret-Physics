/**
 * @file        shape_rod.h
 * @brief       杆形刚体形状头文件，定义杆状形状。
 * @details     定义 ShapeRod 类，继承自 ShapeBase，表示杆状形状，
 *              用于物理碰撞检测和渲染。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-26
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_SHAPE_ROD_H
#define EGRET_PHYSICS_SHAPE_ROD_H
#include <string>

#include "shape_base.h"

namespace egret
{
    /**
     * @brief       杆状形状类，表示杆形刚体的碰撞形状。
     * @details     ShapeRod 继承自 ShapeBase，表示一个以原点为中心、
     *              沿Z轴方向的细杆形状，用于物理碰撞检测和转动惯量计算。
     *              杆由长度参数定义，半径为零（一维形状）。
     *              杆的体积为零，转动惯量在轴向为零。
     *
     * @invariant   m_length > 0，长度必须为正值
     * @invariant   typeId() 返回 ShapeType::Rod
     * @invariant   getVolume() 返回 0（一维形状）
     * @invariant   质心位于原点 (0, 0, 0)
     * @remark      杆是一维形状，适合模拟细长物体如棍棒、杆件
     * @see         ShapeBase, Transform
     */
    class ShapeRod : public ShapeBase
    {
    public:
        explicit ShapeRod(double length);
        [[nodiscard]] ShapeType typeId() const override;

        [[nodiscard]] double getLength() const { return m_length; }

        [[nodiscard]] double getVolume() const override { return 0; };

        [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override;

        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

        [[nodiscard]] AABB getAABB(const Transform &transform) const override;

        [[nodiscard]] ShapeLoadInfo getLoadInfo() const override;

        void setLength(double length);

        [[nodiscard]] std::pair<Eigen::Vector3d, Eigen::Vector3d> getStartEnd(const Transform &transform) const;

        [[nodiscard]] Eigen::Vector3d support(const Eigen::Vector3d &direction, const Transform &transform) const override;

    private:
        double m_length{};
    };
}

#endif // EGRET_PHYSICS_SHAPE_ROD_H
