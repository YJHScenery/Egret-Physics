/**
 * @file        shape_disk.h
 * @brief       圆盘形刚体形状头文件，定义圆盘形状。
 * @details     定义 ShapeDisk 类，继承自 ShapeBase，表示圆盘形状，
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

#ifndef EGRET_PHYSICS_SHAPE_DISK_H
#define EGRET_PHYSICS_SHAPE_DISK_H
#include "shape_base.h"

namespace egret
{

    /**
     * @brief       圆盘形状类，表示圆盘形刚体的碰撞形状。
     * @details     ShapeDisk 继承自 ShapeBase，表示一个以原点为中心、
     *              位于XY平面的圆盘形状，用于物理碰撞检测和转动惯量计算。
     *              圆盘由半径参数定义，厚度为零（二维形状）。
     *              圆盘的体积为零，转动惯量在Z轴方向为零。
     * 
     * @invariant   m_radius > 0，半径必须为正值
     * @invariant   typeId() 返回 ShapeType::Disk
     * @invariant   getVolume() 返回 0（二维形状）
     * @invariant   质心位于原点 (0, 0, 0)
     * @remark      圆盘是二维形状，适合模拟薄片物体
     * @see         ShapeBase, ShapeRing, Transform
     */
    class ShapeDisk : public ShapeBase
    {
    public:
        explicit ShapeDisk(double radius);

        [[nodiscard]] ShapeType getType() const override;

        [[nodiscard]] double getRadius() const { return m_radius; }

        [[nodiscard]] double getVolume() const override { return 0; }

        [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override;

        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

        [[nodiscard]] AABB getAABB(const Transform &transform) const override;

        [[nodiscard]] ShapeLoadInfo getLoadInfo() const override;

        void setRadius(double radius);

        [[nodiscard]] Eigen::Vector3d support(const Eigen::Vector3d &direction, const Transform &transform) const override;

    private:
        double m_radius{};
    };

} // egret

#endif // EGRET_PHYSICS_SHAPE_DISK_H
