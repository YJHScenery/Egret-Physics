/**
 * @file        shape_cylindrical_shell.h
 * @brief       圆柱壳形刚体形状头文件，定义圆柱壳形状。
 * @details     定义 ShapeCylindricalShell 类，继承自 ShapeCylinder，表示圆柱壳形状，
 *              用于物理碰撞检测和渲染。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-30
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_SHAPE_CYLINDRICAL_SHELL_H
#define EGRET_PHYSICS_SHAPE_CYLINDRICAL_SHELL_H
#include "shape_base.h"
#include "shape_cylinder.h"

namespace egret
{

    /**
     * @brief       圆柱壳形状类，表示圆柱壳形刚体的碰撞形状。
     * @details     ShapeCylindricalShell 继承自 ShapeCylinder，表示一个以原点为中心、
     *              沿Z轴方向的圆柱壳形状，用于物理碰撞检测和转动惯量计算。
     *              圆柱壳是圆柱体的空心版本，质量分布在表面而非内部。
     *              圆柱壳的转动惯量与实心圆柱不同。
     *
     * @invariant   m_radius > 0，半径必须为正值
     * @invariant   m_height > 0，高度必须为正值
     * @invariant   typeId() 返回 ShapeType::CylindricalShell
     * @invariant   质心位于原点 (0, 0, 0)
     * @remark      圆柱壳是空心形状，适合模拟管状物体如管道、空心圆柱
     * @see         ShapeCylinder, ShapeBase, Transform
     */
    class ShapeCylindricalShell : public ShapeCylinder
    {
    public:
        ShapeCylindricalShell(double radius, double mass);

        [[nodiscard]] ShapeType typeId() const override;

        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

        [[nodiscard]] ShapeLoadInfo getLoadInfo() const override;
    };

} // egret

#endif // EGRET_PHYSICS_SHAPE_CYLINDRICAL_SHELL_H
