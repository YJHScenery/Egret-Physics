/**
 * @file        shape_sphere.h
 * @brief       球形刚体形状头文件，定义球体形状。
 * @details     定义 ShapeSphere 类，继承自 ShapeBase，表示球体形状，
 *              用于物理碰撞检测和渲染。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-24
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#ifndef EGRET_PHYSICS_SHAPE_SPHERE_H
#define EGRET_PHYSICS_SHAPE_SPHERE_H
#include "shape_base.h"

namespace egret
{
    /**
     * @brief       球体形状类，表示球形刚体的碰撞形状。
     * @details     ShapeSphere 继承自 ShapeBase，表示一个以原点为中心的球体形状，
     *              用于物理碰撞检测和转动惯量计算。
     *              球体是最简单的凸形状，碰撞检测效率高。
     *              球体的转动惯量在各轴上相等，适合模拟球形物体。
     *
     * @invariant   m_radius > 0，半径必须为正值
     * @invariant   typeId() 返回 ShapeType::Sphere
     * @invariant   质心位于原点 (0, 0, 0)
     * @remark      球体形状不包含位置信息，位置由 Transform 管理
     * @see         ShapeBase, ShapeSphericalShell, Transform
     */
    class ShapeSphere : public ShapeBase
    {
    public:
        explicit ShapeSphere(const double radius) : m_radius{radius} {}

        void setRadius(double radius);

        [[nodiscard]] ShapeType typeId() const override;

        [[nodiscard]] double getVolume() const override;

        [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override;

        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

        // [[nodiscard]] bool collideWith(const ShapeBase* other, const Transform& thisTrans, const Transform& otherTrans,
        //                                ContactManifold& manifold) const override;

        [[nodiscard]] AABB getAABB(const Transform &transform) const override;

        [[nodiscard]] double getRadius() const { return m_radius; }

        [[nodiscard]] ShapeLoadInfo getLoadInfo() const override;

        [[nodiscard]] Eigen::Vector3d support(const Eigen::Vector3d &direction, const Transform &transform) const override;

    protected:
        double m_radius{};
    };
} // egret

#endif // EGRET_PHYSICS_SHAPE_SPHERE_H
