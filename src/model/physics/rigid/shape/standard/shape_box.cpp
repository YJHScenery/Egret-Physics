//
// Created by jehor on 2026/4/24.
//

#include "shape_box.h"

#include "shape_sphere.h"
#include "world_scene_manager.h"
#include "collide_judge/standard_collide_judge_group.h"

namespace egret
{
    ShapeBox::ShapeBox(Eigen::Vector3d size) : m_size(std::move(size))
    {
    }

    const std::string& ShapeBox::typeId() const
    {
        static std::string typeId = TYPE_ID_STANDARD_BOX;
        return typeId;
    }

    ShapeBox::ShapeBox(const double length, const double width, const double height) : m_size(length, width, height)
    {
    }

    void ShapeBox::setSize(Eigen::Vector3d size)
    {
        m_size = std::move(size);
    }

    void ShapeBox::setSize(const double length, const double width, const double height)
    {
        m_size = Eigen::Vector3d(length, width, height);
    }

    Eigen::Vector3d ShapeBox::getHalfSize() const
    {
        return m_size * 0.5;
    }

    Eigen::Vector3d ShapeBox::getSize() const
    {
        return m_size;
    }

    double ShapeBox::getLength() const
    {
        return m_size.x();
    }

    double ShapeBox::getWidth() const
    {
        return m_size.y();
    }

    double ShapeBox::getHeight() const
    {
        return m_size.z();
    }

    double ShapeBox::getVolume() const
    {
        return m_size.x() * m_size.y() * m_size.z();
    }

    Eigen::Vector3d ShapeBox::getCenterOfMass() const
    {
        return {0, 0, 0};
    }

    Eigen::Matrix3d ShapeBox::getInertiaTensor(double mass) const
    {
        constexpr double coefficient{1.0 / 12.0};
        const double i_xx{coefficient * mass * (quickSquare(m_size.y()) + quickSquare(m_size.z()))};
        const double i_yy{coefficient * mass * (quickSquare(m_size.x()) + quickSquare(m_size.z()))};
        const double i_zz{coefficient * mass * (quickSquare(m_size.x()) + quickSquare(m_size.y()))};
        Eigen::Matrix3d inertiaTensor;
        inertiaTensor(0, 0) = i_xx;
        inertiaTensor(1, 1) = i_yy;
        inertiaTensor(2, 2) = i_zz;
        return inertiaTensor;
    }

    // bool ShapeBox::collideWith(const ShapeBase* other, const Transform& thisTrans, const Transform& otherTrans,
    //                            ContactManifold& manifold) const
    // {
    //     // 尝试转为球体
    //     const auto* sphere = dynamic_cast<const ShapeSphere*>(other);
    //     if (sphere != nullptr) {
    //         return collideBoxSphere(*this, thisTrans, *sphere, otherTrans, manifold);
    //     }
    //
    //     // 尝试转为长方体
    //     const auto* box = dynamic_cast<const ShapeBox*>(other);
    //     if (box != nullptr) {
    //         return collideBoxBox(*this, thisTrans, *box, otherTrans, manifold);
    //     }
    //
    //     // 其他形状暂不支持
    //     return false;
    // }

    AABB ShapeBox::getAABB(const Transform& transform) const
    {
        const Eigen::Matrix3d linear = transform.getLocalToWorldMatrix().topLeftCorner<3, 3>();
        const Eigen::Vector3d center = transform.getTranslation();
        const Eigen::Vector3d extent = linear.cwiseAbs() * getHalfSize();

        AABB result{};
        result.min = center - extent;
        result.max = center + extent;
        return result;
    }

    ShapeLoadInfo ShapeBox::getLoadInfo() const
    {
        ShapeLoadInfo info{};
        info.typeId = typeId();
        info.parameters["size"] = m_size;
        return info;
    }

    SceneRenderItem ShapeBox::getBasicRenderInfo(const Eigen::Vector3d& position) const
    {
        SceneRenderItem item{};
        const Eigen::Vector3d size {this->getSize()};
        item.kind = this->typeId();
        item.width = size.x();
        item.height = size.y();
        item.x = position.x() - size.x() * 0.5;
        item.y = position.y() - size.y() * 0.5;
        return item;
    }

    void ShapeBox::getLocalAxes(const Transform& trans, Eigen::Vector3d& axisX, Eigen::Vector3d& axisY,
                                Eigen::Vector3d& axisZ)
    {
        axisX = trans.getRotation() * Eigen::Vector3d::UnitX();
        axisY = trans.getRotation() * Eigen::Vector3d::UnitY();
        axisZ = trans.getRotation() * Eigen::Vector3d::UnitZ();
    }

} // egret
