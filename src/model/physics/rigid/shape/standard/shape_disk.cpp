//
// Created by jehor on 2026/4/24.
//

#include "shape_disk.h"

#include "world_scene_manager.h"

namespace egret
{
    ShapeDisk::ShapeDisk(const double radius): m_radius(radius)
    {
    }

    const std::string& ShapeDisk::typeId() const
    {
        static std::string typeId{TYPE_ID_STANDARD_DISK};
        return typeId;
    }

    Eigen::Vector3d ShapeDisk::getCenterOfMass() const
    {
        return {0.0, 0.0, 0.0};
    }

    Eigen::Matrix3d ShapeDisk::getInertiaTensor(double mass) const
    {
        Eigen::Matrix3d inertiaTensor{};
        inertiaTensor.setIdentity();
        inertiaTensor(3, 3) = 2.0;
        inertiaTensor *= 0.25 * mass * quickSquare(m_radius);
        return inertiaTensor;
    }

    AABB ShapeDisk::getAABB(const Transform& transform) const
    {
        const Eigen::Matrix3d rotationMat{transform.getRotationMatrix()};
        Eigen::Vector3d min, max;
        for (int i = 0; i < 3; i++) {
            const double a{transform.getScale().x() * rotationMat(i, 0)};
            const double b{transform.getScale().y() * rotationMat(i, 1)};
            const double halfLen{m_radius * sqrt(quickSquare(a) + quickSquare(b))};
            min[i] = transform.getTranslation()[i] - halfLen;
            max[i] = transform.getTranslation()[i] + halfLen;
        }
        return AABB{min, max};
    }

    SceneRenderItem ShapeDisk::getBasicRenderInfo(const Eigen::Vector3d& position) const
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

    void ShapeDisk::setRadius(const double radius)
    {
        m_radius = radius;
    }
} // egret