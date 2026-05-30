/**
 * @file        shape_box.h
 * @brief       盒形刚体形状头文件，定义长方体形状。
 * @details     定义 ShapeBox 类，继承自 ShapeBase，表示长方体形状，
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

#ifndef EGRET_PHYSICS_SHAPE_CUBE_H
#define EGRET_PHYSICS_SHAPE_CUBE_H
#include "shape_base.h"

namespace egret
{
    /**
     * @brief       长方体形状类，表示盒形刚体的碰撞形状。
     * @details     ShapeBox 继承自 ShapeBase，表示一个以原点为中心的长方体形状，
     *              用于物理碰撞检测和转动惯量计算。
     *              长方体由三个尺寸参数（长、宽、高）定义，支持不同比例的盒子。
     *              长方体的转动惯量在各轴上不同，适合模拟方形物体。
     *
     * @invariant   m_size 各分量 > 0，尺寸必须为正值
     * @invariant   typeId() 返回 ShapeType::Box
     * @invariant   质心位于原点 (0, 0, 0)
     * @remark      长方体形状不包含位置信息，位置由 Transform 管理
     * @see         ShapeBase, Transform, AABB
     */
    class ShapeBox : public ShapeBase
    {
    public:
        explicit ShapeBox(Eigen::Vector3d size);

        [[nodiscard]] ShapeType getType() const override;

        ShapeBox(double length, double width, double height);

        void setSize(Eigen::Vector3d size);

        void setSize(double length, double width, double height);

        [[nodiscard]] Eigen::Vector3d getHalfSize() const;

        [[nodiscard]] Eigen::Vector3d getSize() const;

        [[nodiscard]] double getLength() const;

        [[nodiscard]] double getWidth() const;

        [[nodiscard]] double getHeight() const;

        [[nodiscard]] double getVolume() const override;

        [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override;

        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

        // [[nodiscard]] bool collideWith(const ShapeBase* other,
        //                                const Transform& thisTrans, const Transform& otherTrans,
        //                                ContactManifold& manifold) const override;

        [[nodiscard]] AABB getAABB(const Transform &transform) const override;

        [[nodiscard]] ShapeLoadInfo getLoadInfo() const override;

        [[nodiscard]] Eigen::Vector3d support(const Eigen::Vector3d &direction, const Transform &transform) const override;

        static void getLocalAxes(const Transform &trans,
                                 Eigen::Vector3d &axisX,
                                 Eigen::Vector3d &axisY,
                                 Eigen::Vector3d &axisZ);
        //
        // static bool collideBoxSphere(const ShapeBox& box, const Transform& boxTrans,
        //                              const ShapeSphere& sphere, const Transform& sphereTrans,
        //                              ContactManifold& manifold);
        //
        // static bool collideBoxBox(const ShapeBox& boxA, const Transform& transA,
        //                           const ShapeBox& boxB, const Transform& transB,
        //                           ContactManifold& manifold);

    private:
        Eigen::Vector3d m_size{};
    };
} // egret

#endif // EGRET_PHYSICS_SHAPE_CUBE_H
