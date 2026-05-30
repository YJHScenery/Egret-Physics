/**
 * @file        shape_cylinder.h
 * @brief       圆柱形刚体形状头文件，定义圆柱体形状。
 * @details     定义 ShapeCylinder 类，继承自 ShapeBase，表示圆柱体形状，
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

#ifndef EGRET_PHYSICS_SHAPE_CYLINDER_H
#define EGRET_PHYSICS_SHAPE_CYLINDER_H
#include "shape_base.h"

namespace egret
{

    /**
     * @brief       圆柱体形状类，表示圆柱形刚体的碰撞形状。
     * @details     ShapeCylinder 继承自 ShapeBase，表示一个以原点为中心、
     *              沿Z轴方向的圆柱体形状，用于物理碰撞检测和转动惯量计算。
     *              圆柱体由半径和高度两个参数定义。
     *              圆柱体的转动惯量在轴向和径向不同，适合模拟圆柱形物体。
     *
     * @invariant   m_radius > 0，半径必须为正值
     * @invariant   m_height > 0，高度必须为正值
     * @invariant   typeId() 返回 ShapeType::Cylinder
     * @invariant   质心位于原点 (0, 0, 0)
     * @remark      圆柱体形状不包含位置信息，位置由 Transform 管理
     * @see         ShapeBase, ShapeCylindricalShell, Transform
     */
    class ShapeCylinder : public ShapeBase
    {
    public:
        // 构造函数
        ShapeCylinder(double radius, double height);

        [[nodiscard]] ShapeType getType() const override;

        // 获取几何属性
        [[nodiscard]] double getRadius() const { return m_radius; }
        [[nodiscard]] double getHeight() const { return m_height; }
        [[nodiscard]] double getHalfHeight() const { return m_height * 0.5; }

        [[nodiscard]] double getVolume() const override;
        [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override;
        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;
        [[nodiscard]] AABB getAABB(const Transform &transform) const override;
        [[nodiscard]] ShapeLoadInfo getLoadInfo() const override;
        [[nodiscard]] Eigen::Vector3d support(const Eigen::Vector3d &direction, const Transform &transform) const override;
        // [[nodiscard]] bool collideWith(const ShapeBase* other,
        //                                const Transform& thisTrans, const Transform& otherTrans,
        //                                ContactManifold& manifold) const override;

        void setRadius(double radius);

        void setHeight(double height);

    protected:
        double m_radius{};
        double m_height{};
    };

} // egret

#endif // EGRET_PHYSICS_SHAPE_CYLINDER_H
