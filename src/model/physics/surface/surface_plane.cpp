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
    TessellatedMesh SurfacePlane::tessellate(
        const TessellationConfig& config) const
    {
        TessellatedMesh mesh;

        const int segs = std::max(1, config.subdivisions);
        const double halfExtent = config.planeExtent * 0.5;
        const double invSegs = 1.0 / static_cast<double>(segs);

        // 在平面内构建正交基向量 (u, v)
        // 选择与法线不平行的参考向量
        Eigen::Vector3d ref = Eigen::Vector3d::UnitZ();
        if (std::abs(m_normal.dot(ref)) > 0.9) {
            ref = Eigen::Vector3d::UnitX();
        }
        const Eigen::Vector3d u = m_normal.cross(ref).normalized();
        const Eigen::Vector3d v = m_normal.cross(u).normalized();

        // 顶点总数：(segs + 1) ^ 2
        const int vertsPerSide = segs + 1;
        mesh.vertices.reserve(vertsPerSide * vertsPerSide);
        mesh.normals.reserve(vertsPerSide * vertsPerSide);

        for (int j = 0; j < vertsPerSide; ++j) {
            const double vy = -halfExtent + static_cast<double>(j) * config.planeExtent * invSegs;
            for (int i = 0; i < vertsPerSide; ++i) {
                const double vx = -halfExtent + static_cast<double>(i) * config.planeExtent * invSegs;
                // 顶点位置：锚点 + u * vx + v * vy
                mesh.vertices.push_back(m_anchor + u * vx + v * vy);
                // 法线：平面法线（恒定）
                mesh.normals.push_back(m_normal);
            }
        }

        // 生成索引（两个三角形组成一个网格单元）
        mesh.indices.reserve(segs * segs * 6);
        for (int j = 0; j < segs; ++j) {
            for (int i = 0; i < segs; ++i) {
                const std::uint32_t idx00 = static_cast<std::uint32_t>(j * vertsPerSide + i);
                const std::uint32_t idx10 = static_cast<std::uint32_t>(j * vertsPerSide + i + 1);
                const std::uint32_t idx01 = static_cast<std::uint32_t>((j + 1) * vertsPerSide + i);
                const std::uint32_t idx11 = static_cast<std::uint32_t>((j + 1) * vertsPerSide + i + 1);

                // 三角形 1: (idx00, idx01, idx10)
                mesh.indices.push_back(idx00);
                mesh.indices.push_back(idx01);
                mesh.indices.push_back(idx10);
                // 三角形 2: (idx10, idx01, idx11)
                mesh.indices.push_back(idx10);
                mesh.indices.push_back(idx01);
                mesh.indices.push_back(idx11);
            }
        }

        return mesh;
    }

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
