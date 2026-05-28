//
// Created by jehor on 2026/4/24.
//

#include "shape_rod.h"

#include "world_scene_manager.h"

namespace egret
{
    ShapeRod::ShapeRod(const double length): m_length(length)
    {
    }

    ShapeID ShapeRod::typeId() const
    {
        return ShapeID::Rod;
    }

    Eigen::Vector3d ShapeRod::getCenterOfMass() const
    {
        return {0.0, 0.0, 0.0};
    }

    Eigen::Matrix3d ShapeRod::getInertiaTensor(double mass) const
    {
        Eigen::Matrix3d inertiaTensor;
        inertiaTensor.setIdentity();
        inertiaTensor(2, 2) = 0;
        inertiaTensor *= 1.0 / 12.0 * mass * quickSquare(m_length);
        return inertiaTensor;
    }

    AABB ShapeRod::getAABB(const Transform& transform) const
    {
        const double a = 0.5 * m_length * transform.getScale().z();  // 假设 m_length 是细棒长度
        const Eigen::Vector3d rotationZ = transform.getRotationMatrix().col(2);
        const Eigen::Vector3d halfExtent = a * rotationZ.cwiseAbs();  // 逐分量绝对值
        const Eigen::Vector3d center = transform.getTranslation();
        return {center - halfExtent, center + halfExtent};
    }

    SceneRenderItem ShapeRod::getBasicRenderInfo(const Eigen::Vector3d& position) const
    {
        SceneRenderItem item{};
        constexpr double displaySize = 16.0;
        item.kind = (std::uint32_t)this->typeId();
        item.width = displaySize;
        item.height = displaySize;
        item.x = position.x() - displaySize * 0.5;
        item.y = position.y() - displaySize * 0.5;
        return item;
    }

    void ShapeRod::setLength(const double length)
    {
        m_length = length;
    }

    std::pair<Eigen::Vector3d, Eigen::Vector3d> ShapeRod::getStartEnd(const Transform& transform) const
    {
        const Eigen::Vector3d position{transform.getTranslation()};
        const Eigen::Matrix3d rotationMatrix{transform.getRotationMatrix()};
        const Eigen::Vector3d factor{0, 0, -(m_length * transform.getScale().z()) / 2.0};

        return {position + rotationMatrix * factor, position + rotationMatrix * (-factor)};
    }

    Eigen::Vector3d ShapeRod::support(const Eigen::Vector3d& direction, const Transform& transform) const
    {
        // 将方向转换到局部坐标系（细棒在局部坐标系中沿z轴）
        Eigen::Vector3d localDir = transform.getRotation().conjugate() * direction;
        
        // 细棒的支撑点是两个端点之一
        // 计算方向在细棒轴（z轴）上的投影
        double halfLength = m_length * 0.5;
        double zProjection = localDir.z();
        
        // 取能最大化投影的端点
        Eigen::Vector3d localSupport(0, 0, zProjection >= 0 ? halfLength : -halfLength);
        
        // 变换回世界坐标系
        return transform.localToWorld(localSupport);
    }
}
