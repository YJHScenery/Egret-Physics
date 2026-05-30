/**
 * @file        shape_disk.cpp
 * @brief       圆盘形刚体形状实现文件，定义圆盘形状。
 * @details     实现 ShapeDisk 类的各项成员函数。
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

#include "shape_disk.h"

#include "world_scene_manager.h"

namespace egret
{
    ShapeDisk::ShapeDisk(const double radius) : m_radius(radius)
    {
    }

    ShapeType ShapeDisk::getType() const
    {
        return ShapeType::Disk;
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

    AABB ShapeDisk::getAABB(const Transform &transform) const
    {
        const Eigen::Matrix3d rotationMat{transform.getRotationMatrix()};
        Eigen::Vector3d min, max;
        for (int i = 0; i < 3; i++)
        {
            const double a{transform.getScale().x() * rotationMat(i, 0)};
            const double b{transform.getScale().y() * rotationMat(i, 1)};
            const double halfLen{m_radius * sqrt(quickSquare(a) + quickSquare(b))};
            min[i] = transform.getTranslation()[i] - halfLen;
            max[i] = transform.getTranslation()[i] + halfLen;
        }
        return AABB{min, max};
    }

    ShapeLoadInfo ShapeDisk::getLoadInfo() const
    {
        ShapeLoadInfo info{};
        info.typeId = static_cast<std::uint32_t>(getType());
        info.numberParams["radius"] = std::vector<double>{m_radius};
        return info;
    }

    void ShapeDisk::setRadius(const double radius)
    {
        m_radius = radius;
    }

    Eigen::Vector3d ShapeDisk::support(const Eigen::Vector3d &direction, const Transform &transform) const
    {
        // 将方向转换到局部坐标系（圆盘在局部坐标系中位于xy平面）
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

        // 圆盘的支撑点：在xy平面上沿投影方向取最大半径点
        // 忽略z分量，因为圆盘是2D的（厚度为0）
        Eigen::Vector2d xyDir(localDir.x(), localDir.y());
        double xyLen = xyDir.norm();

        Eigen::Vector3d localSupport;
        if (xyLen > 1e-12)
        {
            xyDir /= xyLen;
            localSupport = Eigen::Vector3d(xyDir.x() * m_radius, xyDir.y() * m_radius, 0);
        }
        else
        {
            // 方向垂直于圆盘平面，任意边缘点都可作为支撑点
            localSupport = Eigen::Vector3d(m_radius, 0, 0);
        }

        // 变换回世界坐标系
        return transform.localToWorld(localSupport);
    }
} // egret