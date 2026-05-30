/**
 * @file        shape_ring.h
 * @brief       圆环形刚体形状头文件，定义圆环形状。
 * @details     定义 ShapeRing 类，继承自 ShapeDisk，表示圆环形状，
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
#ifndef EGRET_PHYSICS_SHAPE_RING_H
#define EGRET_PHYSICS_SHAPE_RING_H
#include "shape_base.h"
#include "shape_disk.h"

namespace egret
{

    /**
     * @brief       圆环形状类，表示圆环形刚体的碰撞形状。
     * @details     ShapeRing 继承自 ShapeDisk，表示一个以原点为中心、
     *              位于XY平面的圆环形状，用于物理碰撞检测和转动惯量计算。
     *              圆环由半径参数定义，是圆盘的空心版本。
     *              圆环的体积为零，转动惯量在Z轴方向为零。
     *
     * @invariant   m_radius > 0，半径必须为正值
     * @invariant   typeId() 返回 ShapeType::Ring
     * @invariant   getVolume() 返回 0（二维形状）
     * @invariant   质心位于原点 (0, 0, 0)
     * @remark      圆环是二维空心形状，适合模拟环形物体如轮胎、环状结构
     * @see         ShapeDisk, ShapeBase, Transform
     */
    class ShapeRing : public ShapeDisk
    {

    public:
        explicit ShapeRing(double radius);

        [[nodiscard]] ShapeType typeId() const override;

        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

        [[nodiscard]] ShapeLoadInfo getLoadInfo() const override;
    };

} // egret

#endif // EGRET_PHYSICS_SHAPE_RING_H
