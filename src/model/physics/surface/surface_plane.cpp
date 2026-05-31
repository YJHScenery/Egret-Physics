/**
 * @file        surface_plane.cpp
 * @brief       无限大平面曲面实现文件。
 * @details     提供 SurfacePlane 类的所有方法实现。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-01
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 */

#include "surface_plane.h"

namespace egret
{
    SurfacePlane::SurfacePlane(const Eigen::Vector3d& anchor,
                               const Eigen::Vector3d& normal)
        : m_anchor(anchor)
        , m_normal(normal.normalized())
    {
    }

    std::unique_ptr<SurfaceBase> SurfacePlane::clone() const
    {
        return std::make_unique<SurfacePlane>(m_anchor, m_normal);
    }

    SurfaceType SurfacePlane::getType() const
    {
        return SurfaceType::Plane;
    }

    double SurfacePlane::signedDistance(const Eigen::Vector3d& worldPoint) const
    {
        return (worldPoint - m_anchor).dot(m_normal);
    }

    Eigen::Vector3d SurfacePlane::closestPoint(const Eigen::Vector3d& worldPoint) const
    {
        return worldPoint - signedDistance(worldPoint) * m_normal;
    }

    Eigen::Vector3d SurfacePlane::normal(const Eigen::Vector3d& /*worldPoint*/) const
    {
        return m_normal;
    }

    bool SurfacePlane::collide(const ShapeBase* shape,
                               const Transform& shapeTransform,
                               ContactManifold& manifold) const
    {
        if (shape == nullptr) {
            return false;
        }

        // 在 -normal 方向找到形状上最深入平面的点
        const Eigen::Vector3d deepestPoint = shape->support(-m_normal, shapeTransform);
        const double sd = (deepestPoint - m_anchor).dot(m_normal);

        if (sd >= 0.0) {
            return false;
        }

        // 接触点 = 平面上距离 deepestPoint 最近的点
        const Eigen::Vector3d contactPos = deepestPoint - sd * m_normal;

        ContactPoint contact;
        contact.position = contactPos;
        contact.normal = m_normal;
        contact.penetration = -sd;
        manifold.push_back(contact);
        return true;
    }
} // egret
