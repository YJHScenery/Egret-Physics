/**
 * @file        shape_box.cpp
 * @brief       盒形刚体形状实现文件，定义长方体形状。
 * @details     实现 ShapeBox 类的各项成员函数。
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

#include "shape_box.h"

#include "shape_sphere.h"
#include "world_scene_manager.h"
#include "collide_judge/standard_collide_judge_group.h"
#include <ranges>

namespace egret
{
    ShapeBox::ShapeBox(Eigen::Vector3d size) : m_size(std::move(size))
    {
    }

    ShapeType ShapeBox::typeId() const
    {
        return ShapeType::Box;
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

    AABB ShapeBox::getAABB(const Transform &transform) const
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
        info.typeId = (std::uint32_t)typeId();
        info.numberParams["size"] = std::vector<double>{m_size.data(), m_size.data() + m_size.size()};
        return info;
    }

    Eigen::Vector3d ShapeBox::support(const Eigen::Vector3d &direction, const Transform &transform) const
    {
        // 将方向转换到局部坐标系
        Eigen::Vector3d localDir = transform.getRotation().conjugate() * direction;

        // 局部坐标系下的支撑点：在每个轴上取符号对应的半长
        const Eigen::Vector3d localSupport(
            (localDir.x() > 0 ? 1 : -1) * (m_size.x() / 2.0),
            (localDir.y() > 0 ? 1 : -1) * (m_size.y() / 2.0),
            (localDir.z() > 0 ? 1 : -1) * (m_size.z() / 2.0));

        // 变换回世界坐标系
        return transform.localToWorld(localSupport);
    }

    void ShapeBox::getLocalAxes(const Transform &trans, Eigen::Vector3d &axisX, Eigen::Vector3d &axisY,
                                Eigen::Vector3d &axisZ)
    {
        axisX = trans.getRotation() * Eigen::Vector3d::UnitX();
        axisY = trans.getRotation() * Eigen::Vector3d::UnitY();
        axisZ = trans.getRotation() * Eigen::Vector3d::UnitZ();
    }

} // egret
