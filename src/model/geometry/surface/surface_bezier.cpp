/**
 * @file        surface_bezier.cpp
 * @brief       双三次 Bezier 曲面实现文件。
 * @details     提供 SurfaceBezier 类的所有方法实现，包括数值求最近点、法线等。
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

#include "surface_bezier.h"
#include <limits>

namespace egret
{
    TessellatedMesh SurfaceBezier::tessellate(
        const TessellationConfig& config) const
    {
        TessellatedMesh mesh;

        const int segs = std::max(2, config.subdivisions);
        const double invSegs = 1.0 / static_cast<double>(segs);
        const int vertsPerSide = segs + 1;

        // 预分配
        mesh.vertices.reserve(vertsPerSide * vertsPerSide);
        mesh.normals.reserve(vertsPerSide * vertsPerSide);

        // 生成顶点和法线
        for (int j = 0; j < vertsPerSide; ++j) {
            const double v = static_cast<double>(j) * invSegs;
            for (int i = 0; i < vertsPerSide; ++i) {
                const double u = static_cast<double>(i) * invSegs;

                // 曲面位置
                const Eigen::Vector3d pos = evaluate(u, v);

                // 法线 = ∂S/∂u × ∂S/∂v
                const Eigen::Vector3d su = deriveU(u, v);
                const Eigen::Vector3d sv = deriveV(u, v);
                Eigen::Vector3d n = su.cross(sv);
                const double nLen = n.norm();
                if (nLen > 1e-20) {
                    n /= nLen;
                } else {
                    n = Eigen::Vector3d::UnitX();
                }

                mesh.vertices.push_back(pos);
                mesh.normals.push_back(n);
            }
        }

        // 生成索引
        mesh.indices.reserve(segs * segs * 6);
        for (int j = 0; j < segs; ++j) {
            for (int i = 0; i < segs; ++i) {
                const auto idx00 = static_cast<std::uint32_t>(j * vertsPerSide + i);
                const auto idx10 = static_cast<std::uint32_t>(j * vertsPerSide + i + 1);
                const auto idx01 = static_cast<std::uint32_t>((j + 1) * vertsPerSide + i);
                const auto idx11 = static_cast<std::uint32_t>((j + 1) * vertsPerSide + i + 1);

                // 三角形 1
                mesh.indices.push_back({idx00, idx01, idx10});

                // 三角形 2
                mesh.indices.push_back({idx10, idx01, idx11});
                // mesh.indices.push_back(idx00);
                // mesh.indices.push_back(idx01);
                // mesh.indices.push_back(idx10);
                //
                // mesh.indices.push_back(idx10);
                // mesh.indices.push_back(idx01);
                // mesh.indices.push_back(idx11);
            }
        }

        return mesh;
    }

    SurfaceBezier::SurfaceBezier(
        const std::array<std::array<Eigen::Vector3d, 4>, 4>& controlPoints)
        : m_controlPoints(controlPoints)
    {
        precomputeCoefficients();
    }

    std::unique_ptr<SurfaceBase> SurfaceBezier::clone() const
    {
        return std::make_unique<SurfaceBezier>(m_controlPoints);
    }

    SurfaceType SurfaceBezier::getType() const
    {
        return SurfaceType::Bezier;
    }

    double SurfaceBezier::signedDistance(const Eigen::Vector3d& worldPoint) const
    {
        const Eigen::Vector3d cp = closestPoint(worldPoint);
        // 使用 closestPoint 缓存的 (u,v) 算法线，避免 findParameters 歧义
        const Eigen::Vector3d Su = deriveU(m_lastU, m_lastV);
        const Eigen::Vector3d Sv = deriveV(m_lastU, m_lastV);
        Eigen::Vector3d n = Su.cross(Sv);
        const double nLen = n.norm();
        if (nLen > 1e-20) n /= nLen;
        const Eigen::Vector3d diff = worldPoint - cp;
        return diff.dot(n);
    }

    Eigen::Vector3d SurfaceBezier::closestPoint(const Eigen::Vector3d& worldPoint) const
    {
        constexpr std::size_t kGridSamples = 5;
        constexpr std::size_t kEdgeSamples = 8;
        constexpr std::size_t kMaxIter = 32;
        constexpr double kTol = 1e-8;

        double bestDist2 = std::numeric_limits<double>::max();
        Eigen::Vector3d bestPoint = evaluate(0.0, 0.0);

        // 边界采样：对远离曲面的点，最近点一定在参数域边界上
        for (std::size_t s = 0; s <= kEdgeSamples; ++s) {
            const double t = static_cast<double>(s) / kEdgeSamples;
            // 四条边界：u=0, u=1, v=0, v=1
            const std::pair<double, double> edges[4] = {{0.0, t}, {1.0, t}, {t, 0.0}, {t, 1.0}};
            for (const auto& edge : edges) {
                const Eigen::Vector3d p = evaluate(edge.first, edge.second);
                const double d2 = (p - worldPoint).squaredNorm();
                if (d2 < bestDist2) {
                    bestDist2 = d2;
                    bestPoint = p;
                    m_lastU = edge.first;
                    m_lastV = edge.second;
                }
            }
        }

        for (std::size_t i = 0; i < kGridSamples; ++i) {
            for (std::size_t j = 0; j < kGridSamples; ++j) {
                double u = static_cast<double>(i) / (kGridSamples - 1);
                double v = static_cast<double>(j) / (kGridSamples - 1);

                for (std::size_t iter = 0; iter < kMaxIter; ++iter) {
                    const Eigen::Vector3d Su = deriveU(u, v);
                    const Eigen::Vector3d Sv = deriveV(u, v);
                    const Eigen::Vector3d S = evaluate(u, v);
                    const Eigen::Vector3d diff = S - worldPoint;

                    const double a = Su.squaredNorm();
                    const double b = Su.dot(Sv);
                    const double c = Sv.squaredNorm();
                    const double det = a * c - b * b;

                    if (std::abs(det) < 1e-20) {
                        break;
                    }

                    const double f1 = Su.dot(diff);
                    const double f2 = Sv.dot(diff);

                    const double du = -(c * f1 - b * f2) / det;
                    const double dv = -(-b * f1 + a * f2) / det;

                    double step = 1.0;
                    for (int ls = 0; ls < 8; ++ls) {
                        const double uNew = std::clamp(u + step * du, 0.0, 1.0);
                        const double vNew = std::clamp(v + step * dv, 0.0, 1.0);
                        const Eigen::Vector3d newS = evaluate(uNew, vNew);
                        const double newDist2 = (newS - worldPoint).squaredNorm();
                        if (newDist2 < (S - worldPoint).squaredNorm() + 1e-12) {
                            break;
                        }
                        step *= 0.5;
                    }

                    u = std::clamp(u + step * du, 0.0, 1.0);
                    v = std::clamp(v + step * dv, 0.0, 1.0);

                    if (std::abs(step * du) < kTol && std::abs(step * dv) < kTol) {
                        break;
                    }
                }

                const Eigen::Vector3d candidate = evaluate(u, v);
                const double dist2 = (candidate - worldPoint).squaredNorm();
                if (dist2 < bestDist2) {
                    bestDist2 = dist2;
                    bestPoint = candidate;
                    m_lastU = u;
                    m_lastV = v;
                }
            }
        }

        return bestPoint;
    }

    Eigen::Vector3d SurfaceBezier::normal(const Eigen::Vector3d& worldPoint) const
    {
        const Eigen::Vector3d cp = closestPoint(worldPoint);
        const auto [u, v] = findParameters(cp);
        const Eigen::Vector3d Su = deriveU(u, v);
        const Eigen::Vector3d Sv = deriveV(u, v);
        const Eigen::Vector3d n = Su.cross(Sv);
        const double nNorm = n.norm();
        if (nNorm < 1e-20) {
            return Eigen::Vector3d::UnitX();
        }
        return n / nNorm;
    }

    bool SurfaceBezier::collide(const ShapeBase* shape,
                                const Transform& shapeTransform,
                                ContactManifold& manifold) const
    {
        if (shape == nullptr) {
            return false;
        }

        const Eigen::Vector3d comWorld =
            shapeTransform.transformPointToWorld(shape->getCenterOfMass());

        // 计算一次最近点，然后复用其缓存的 (u,v) 算法线
        // 避免 normal() 重复调用 closestPoint 以及 findParameters 可能得到不同 (u,v)
        const Eigen::Vector3d cp = closestPoint(comWorld);
        const Eigen::Vector3d Su = deriveU(m_lastU, m_lastV);
        const Eigen::Vector3d Sv = deriveV(m_lastU, m_lastV);
        Eigen::Vector3d n = Su.cross(Sv);
        const double nLen = n.norm();
        if (nLen > 1e-20) n /= nLen;

        // 确保法线指向曲面"外部"（朝向物体质心方向）
        // Bezier 曲面的参数法线 (Su × Sv) 方向由控制点顺序决定，
        // 不保证指向外部，需要根据质心位置矫正。
        if ((comWorld - cp).dot(n) < 0) {
            n = -n;
        }

        const Eigen::Vector3d deepestWorld = shape->support(-n, shapeTransform);
        // 使用 cp 处的切平面计算有符号距离。避免调用 signedDistance()
        // 因其内部使用参数法线方向可能与 collide 计算的 n 不一致。
        const double sd = (deepestWorld - cp).dot(n);

        if (sd >= 0.0) {
            return false;
        }

        ContactPoint contact;
        contact.position = cp;
        contact.normal = n;
        contact.penetration = -sd;
        manifold.push_back(contact);
        return true;
    }

    // ==================== 私有辅助方法 ====================

    Eigen::Vector3d SurfaceBezier::evaluate(double u, double v) const
    {
        Eigen::Vector3d result = Eigen::Vector3d::Zero();
        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t j = 0; j < 4; ++j) {
                result += m_coeffs[i][j] * std::pow(u, static_cast<int>(i)) *
                          std::pow(v, static_cast<int>(j));
            }
        }
        return result;
    }

    Eigen::Vector3d SurfaceBezier::deriveU(double u, double v) const
    {
        Eigen::Vector3d result = Eigen::Vector3d::Zero();
        for (std::size_t i = 1; i < 4; ++i) {
            for (std::size_t j = 0; j < 4; ++j) {
                result += m_coeffs[i][j] * static_cast<double>(i) *
                          std::pow(u, static_cast<int>(i - 1)) *
                          std::pow(v, static_cast<int>(j));
            }
        }
        return result;
    }

    Eigen::Vector3d SurfaceBezier::deriveV(double u, double v) const
    {
        Eigen::Vector3d result = Eigen::Vector3d::Zero();
        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t j = 1; j < 4; ++j) {
                result += m_coeffs[i][j] * std::pow(u, static_cast<int>(i)) *
                          static_cast<double>(j) *
                          std::pow(v, static_cast<int>(j - 1));
            }
        }
        return result;
    }

    std::pair<double, double> SurfaceBezier::findParameters(
        const Eigen::Vector3d& surfacePoint) const
    {
        constexpr std::size_t kGrid = 10;
        double bestU = 0.0, bestV = 0.0;
        double bestDist = std::numeric_limits<double>::max();

        for (std::size_t i = 0; i <= kGrid; ++i) {
            for (std::size_t j = 0; j <= kGrid; ++j) {
                const double u = static_cast<double>(i) / kGrid;
                const double v = static_cast<double>(j) / kGrid;
                const double d = (evaluate(u, v) - surfacePoint).squaredNorm();
                if (d < bestDist) {
                    bestDist = d;
                    bestU = u;
                    bestV = v;
                }
            }
        }

        // 牛顿法细化
        constexpr std::size_t kMaxIter = 16;
        constexpr double kTol = 1e-10;
        double u = bestU, v = bestV;

        for (std::size_t iter = 0; iter < kMaxIter; ++iter) {
            const Eigen::Vector3d Su = deriveU(u, v);
            const Eigen::Vector3d Sv = deriveV(u, v);
            const Eigen::Vector3d S = evaluate(u, v);
            const Eigen::Vector3d diff = S - surfacePoint;

            const double a = Su.squaredNorm();
            const double b = Su.dot(Sv);
            const double c = Sv.squaredNorm();
            const double det = a * c - b * b;

            if (std::abs(det) < 1e-20) break;

            const double f1 = Su.dot(diff);
            const double f2 = Sv.dot(diff);

            const double du = -(c * f1 - b * f2) / det;
            const double dv = -(-b * f1 + a * f2) / det;

            u = std::clamp(u + du, 0.0, 1.0);
            v = std::clamp(v + dv, 0.0, 1.0);

            if (std::abs(du) < kTol && std::abs(dv) < kTol) break;
        }

        return {u, v};
    }

    void SurfaceBezier::precomputeCoefficients()
    {
        constexpr double bernsteinCoeffs[4][4] = {
            { 1.0, -3.0,  3.0, -1.0},
            { 0.0,  3.0, -6.0,  3.0},
            { 0.0,  0.0,  3.0, -3.0},
            { 0.0,  0.0,  0.0,  1.0}
        };

        for (auto& row : m_coeffs) {
            for (auto& col : row) {
                col.setZero();
            }
        }

        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t j = 0; j < 4; ++j) {
                for (std::size_t pi = 0; pi < 4; ++pi) {
                    for (std::size_t pj = 0; pj < 4; ++pj) {
                        m_coeffs[pi][pj] += bernsteinCoeffs[i][pi] *
                                            bernsteinCoeffs[j][pj] *
                                            m_controlPoints[i][j];
                    }
                }
            }
        }
    }
} // egret
