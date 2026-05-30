/**
 * @file        shape_sphere.cpp
 * @brief       球形刚体形状实现文件，定义球体形状。
 * @details     实现 ShapeSphere 类的各项成员函数。
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

#include "shape_sphere.h"

#include "constants.h"
#include "shape_box.h"
#include "world_scene_manager.h"
#include "collide_judge/standard_collide_judge_group.h"

namespace egret
{
    void ShapeSphere::setRadius(const double radius)
    {
        m_radius = radius;
    }

    ShapeType ShapeSphere::typeId() const
    {
        return ShapeType::Sphere;
    }

    double ShapeSphere::getVolume() const
    {
        return 4.0 / 3.0 * quickPower<double, int, 3>(m_radius) * PI;
    }

    Eigen::Vector3d ShapeSphere::getCenterOfMass() const
    {
        return {0, 0, 0};
    }

    Eigen::Matrix3d ShapeSphere::getInertiaTensor(double mass) const
    {
        const double inertia{0.4 * mass * quickSquare(m_radius)};
        return Eigen::Matrix3d::Identity() * inertia;
    }

    // bool ShapeSphere::collideWith(const ShapeBase* other, const Transform& thisTrans, const Transform& otherTrans,
    //     ContactManifold& manifold) const
    // {
    //     const auto* shapeSphere = dynamic_cast<const ShapeSphere*>(other);
    //     if (shapeSphere != nullptr) {
    //         return collideSphereSphere(*this, thisTrans, *shapeSphere, otherTrans, manifold);
    //     }
    //     const auto* shapeBox = dynamic_cast<const ShapeBox*>(other);
    //     if (shapeBox != nullptr) {
    //         return collideBoxSphere(*shapeBox, otherTrans, *this, thisTrans, manifold);
    //     }
    //     return false;
    // }

    AABB ShapeSphere::getAABB(const Transform &transform) const
    {
        const Eigen::Matrix3d linear = transform.getLocalToWorldMatrix().topLeftCorner<3, 3>();
        const Eigen::Vector3d center = transform.getTranslation();
        const Eigen::Vector3d extent = linear.rowwise().norm() * m_radius;
        return AABB{
            .min = center - extent,
            .max = center + extent,
        };
    }

    ShapeLoadInfo ShapeSphere::getLoadInfo() const
    {
        ShapeLoadInfo info{};
        info.typeId = static_cast<std::uint32_t>(typeId());
        info.numberParams["radius"] = std::vector<double>{m_radius};
        return info;
    }

    Eigen::Vector3d ShapeSphere::support(const Eigen::Vector3d &direction, const Transform &transform) const
    {
        // 将方向转换到局部坐标系
        Eigen::Vector3d localDir = transform.getRotation().conjugate() * direction;

        // 归一化方向
        const double len = localDir.norm();
        if (len < 1e-12)
        {
            localDir = Eigen::Vector3d(1, 0, 0);
        }
        else
        {
            localDir /= len;
        }

        // 球体的支撑点：中心 + 方向 * 半径
        Eigen::Vector3d localSupport = localDir * m_radius;

        // 变换回世界坐标系
        return transform.localToWorld(localSupport);
    }
} // egret