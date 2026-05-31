/**
 * @file        surface_sphere.cpp
 * @brief       标准（椭）球面实现文件。
 * @details     提供 SurfaceSphere 的 collide 实现，包括对球体形状的解析碰撞检测。
 *
 * @ingroup     Physics
 */

#include "surface_sphere.h"
// 仅当对球体形状做解析优化时需要
#include "shape_sphere.h"
#include <numbers>

namespace egret
{
    TessellatedMesh SurfaceSphere::tessellate(
        const TessellationConfig& config) const
    {
        TessellatedMesh mesh;

        // 经纬分段数
        const int lonSegs = std::max(3, config.subdivisions);       // 经线段数（绕 Z 轴）
        const int latSegs = std::max(2, config.subdivisions / 2);  // 纬线段数（从北极到南极）

        const double dTheta = 2.0 * std::numbers::pi / static_cast<double>(lonSegs);
        const double dPhi = std::numbers::pi / static_cast<double>(latSegs);

        const double a = m_radii.x();
        const double b = m_radii.y();
        const double c = m_radii.z();

        // 预分配顶点：极点 + 中间环
        const int totalVerts = lonSegs * (latSegs - 1) + 2;
        mesh.vertices.reserve(totalVerts);
        mesh.normals.reserve(totalVerts);

        // 辅助 lambda：生成局部坐标顶点和法线
        auto addVertex = [&](double theta, double phi) {
            const double sinPhi = std::sin(phi);
            const double cosPhi = std::cos(phi);
            const double sinTheta = std::sin(theta);
            const double cosTheta = std::cos(theta);

            // 局部坐标位置
            const Eigen::Vector3d localPos(
                a * cosTheta * sinPhi,
                b * sinTheta * sinPhi,
                c * cosPhi
            );

            // 局部法线（椭球梯度方向）
            Eigen::Vector3d localNormal(
                cosTheta * sinPhi / a,
                sinTheta * sinPhi / b,
                cosPhi / c
            );
            const double nLen = localNormal.norm();
            if (nLen > 1e-12) {
                localNormal /= nLen;
            }

            // 变换到世界空间
            mesh.vertices.push_back(m_transform.localToWorld(localPos));
            mesh.normals.push_back(
                m_transform.localToWorld(localNormal).normalized());
        };

        // 北极点 (phi = 0)
        addVertex(0.0, 0.0);

        // 中间环 (phi = dPhi 到 pi - dPhi)
        for (int j = 1; j < latSegs; ++j) {
            const double phi = static_cast<double>(j) * dPhi;
            for (int i = 0; i < lonSegs; ++i) {
                const double theta = static_cast<double>(i) * dTheta;
                addVertex(theta, phi);
            }
        }

        // 南极点 (phi = pi)
        addVertex(0.0, std::numbers::pi);

        // 生成索引
        const auto idx = [&](int ring, int col) -> std::uint32_t {
            // ring=0 → 北极（索引 0）
            // ring=1..latSegs-1 → 中间环（索引 1 + (ring-1)*lonSegs + col）
            // ring=latSegs → 南极（索引 totalVerts-1）
            if (ring == 0) return 0;
            if (ring == latSegs) return static_cast<std::uint32_t>(totalVerts - 1);
            return static_cast<std::uint32_t>(1 + (ring - 1) * lonSegs + col);
        };

        const std::uint32_t southPole = static_cast<std::uint32_t>(totalVerts - 1);

        // 顶部帽（北极 → 第一环）
        for (int i = 0; i < lonSegs; ++i) {
            const int nextI = (i + 1) % lonSegs;
            mesh.indices.push_back(idx(0, 0));       // 北极
            mesh.indices.push_back(idx(1, nextI));   // 第一环 i+1
            mesh.indices.push_back(idx(1, i));       // 第一环 i
        }

        // 中间带（环 j → 环 j+1）
        for (int j = 1; j < latSegs - 1; ++j) {
            for (int i = 0; i < lonSegs; ++i) {
                const int nextI = (i + 1) % lonSegs;
                // 三角形 1
                mesh.indices.push_back(idx(j, i));
                mesh.indices.push_back(idx(j + 1, i));
                mesh.indices.push_back(idx(j, nextI));
                // 三角形 2
                mesh.indices.push_back(idx(j, nextI));
                mesh.indices.push_back(idx(j + 1, i));
                mesh.indices.push_back(idx(j + 1, nextI));
            }
        }

        // 底部帽（最后一环 → 南极）
        for (int i = 0; i < lonSegs; ++i) {
            const int nextI = (i + 1) % lonSegs;
            mesh.indices.push_back(idx(latSegs - 1, i));
            mesh.indices.push_back(idx(latSegs - 1, nextI));
            mesh.indices.push_back(southPole);
        }

        return mesh;
    }

    bool SurfaceSphere::collide(const ShapeBase* shape,
                                const Transform& shapeTransform,
                                ContactManifold& manifold) const
    {
        if (shape == nullptr) {
            return false;
        }

        // 对球形形状使用解析碰撞检测
        if (shape->getType() == ShapeType::Sphere) {
            const auto* sphere = static_cast<const ShapeSphere*>(shape);
            const double sphereRadius = sphere->getRadius();

            // 球心在世界空间中的位置
            const Eigen::Vector3d sphereCenter = shapeTransform.getTranslation();

            // 球心到椭球面的最近点
            const Eigen::Vector3d cp = closestPoint(sphereCenter);
            const Eigen::Vector3d n = normal(sphereCenter);

            // 球心到椭球面的有符号距离
            const double sd = signedDistance(sphereCenter);

            // 如果球心到曲面的距离小于球半径，则发生碰撞
            // 注意：sd > 0 表示球心在外部，sd < 0 表示球心在内部
            // 碰撞条件：|sd| < sphereRadius (球心在曲面附近的球半径范围内)
            // 更精确：球表面与曲面相交
            if (sd > sphereRadius) {
                // 球完全在曲面外部，没有碰撞
                return false;
            }

            // 计算穿透深度
            const double penetrationDepth = sphereRadius - sd;

            ContactPoint contact;
            contact.position = cp;
            contact.normal = n;
            contact.penetration = penetrationDepth;
            manifold.push_back(contact);
            return true;
        }

        // 对其他形状，回退到基类默认实现
        return SurfaceBase::collide(shape, shapeTransform, manifold);
    }
} // egret
