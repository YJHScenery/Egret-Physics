//
// Created by jehor on 2026/4/24.
//

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

    const std::string& ShapeSphere::typeId() const
    {
        static std::string typeId {TYPE_ID_STANDARD_SPHERE};
        return typeId;
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
        const double inertia {0.4 * mass * quickSquare(m_radius)};
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

    AABB ShapeSphere::getAABB(const Transform& transform) const
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
        info.typeId = typeId();
        info.parameters["radius"] = m_radius;
        return info;
    }

    SceneRenderItem ShapeSphere::getBasicRenderInfo(const Eigen::Vector3d& position) const
    {
        SceneRenderItem item{};
        const double diameter = m_radius * 2.0;
        item.kind = this->typeId();
        item.width = diameter;
        item.height = diameter;
        item.x = position.x() - m_radius;
        item.y = position.y() - m_radius;
        return item;
    }
} // egret