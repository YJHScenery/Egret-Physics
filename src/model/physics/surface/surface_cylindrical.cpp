/**
 * @file        surface_cylindrical.cpp
 * @brief       柱面曲面实现文件。
 * @details     提供 SurfaceCylindrical 类的所有方法实现，
 *              包括最近点搜索、法线计算、碰撞检测和网格化。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-31
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 */

#include "surface_cylindrical.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace egret
{
    // ==================== 公开接口 ====================

    SurfaceCylindrical::SurfaceCylindrical(
        std::unique_ptr<CurveBase> curve,
        const Eigen::Vector3d& direction)
        : m_curve(std::move(curve))
        , m_direction(direction)
        , m_dirLenSq(direction.squaredNorm())
    {
        if (!m_curve) {
            throw std::invalid_argument(
                "SurfaceCylindrical: 基曲线不能为空");
        }
        if (m_dirLenSq < 1e-30) {
            throw std::invalid_argument(
                "SurfaceCylindrical: 方向矢量不能为零向量");
        }
    }

    std::unique_ptr<SurfaceBase> SurfaceCylindrical::clone() const
    {
        return std::make_unique<SurfaceCylindrical>(
            m_curve->clone(), m_direction);
    }

    SurfaceType SurfaceCylindrical::getType() const
    {
        return SurfaceType::Cylindrical;
    }

    double SurfaceCylindrical::signedDistance(
        const Eigen::Vector3d& worldPoint) const
    {
        // 使用缓存的 (t, s) 计算法线，避免重复搜索
        const auto [t, s] = findClosestParameters(worldPoint);

        // 计算法线方向
        const Eigen::Vector3d rawN = evaluateRawNormal(t);
        const double nLen = rawN.norm();
        if (nLen < 1e-20) {
            return std::numeric_limits<double>::max();
        }
        const Eigen::Vector3d n = rawN / nLen;

        // 有符号距离 = (查询点 - 曲面上最近点) · 法线
        const Eigen::Vector3d surfacePoint = evaluate(t, s);
        return (worldPoint - surfacePoint).dot(n);
    }

    Eigen::Vector3d SurfaceCylindrical::closestPoint(
        const Eigen::Vector3d& worldPoint) const
    {
        const auto [t, s] = findClosestParameters(worldPoint);
        return evaluate(t, s);
    }

    Eigen::Vector3d SurfaceCylindrical::normal(
        const Eigen::Vector3d& worldPoint) const
    {
        // 使用最近点处的参数 t 计算法线
        // 法线方向由曲线切向和方向矢量的叉积决定
        const auto [t, _] = findClosestParameters(worldPoint);

        const Eigen::Vector3d rawN = evaluateRawNormal(t);
        const double nLen = rawN.norm();
        if (nLen < 1e-20) {
            return Eigen::Vector3d::UnitZ();
        }
        return rawN / nLen;
    }

    bool SurfaceCylindrical::collide(
        const ShapeBase* shape,
        const Transform& shapeTransform,
        ContactManifold& manifold) const
    {
        if (shape == nullptr) {
            return false;
        }

        const Eigen::Vector3d comWorld =
            shapeTransform.transformPointToWorld(shape->getCenterOfMass());

        const auto [t, s] = findClosestParameters(comWorld);

        // 计算法线
        const Eigen::Vector3d rawN = evaluateRawNormal(t);
        const double nLen = rawN.norm();
        if (nLen < 1e-20) {
            return false;
        }
        Eigen::Vector3d n = rawN / nLen;

        // 确保法线指向质心方向（指向"外部"）
        const Eigen::Vector3d cp = evaluate(t, s);
        if ((comWorld - cp).dot(n) < 0.0) {
            n = -n;
        }

        const Eigen::Vector3d deepestWorld = shape->support(-n, shapeTransform);
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

    TessellatedMesh SurfaceCylindrical::tessellate(
        const TessellationConfig& config) const
    {
        TessellatedMesh mesh;

        const int tSegs = std::max(4, config.subdivisions);
        const int sSegs = std::max(2, config.subdivisions / 4);
        const int tVerts = tSegs + 1;
        const int sVerts = sSegs + 1;

        const double tStart = m_curve->getStartT();
        const double tEnd   = m_curve->getEndT();
        const double tInv   = 1.0 / static_cast<double>(tSegs);

        mesh.vertices.reserve(tVerts * sVerts);
        mesh.normals.reserve(tVerts * sVerts);

        // 生成顶点和法线
        for (int j = 0; j < sVerts; ++j) {
            const double s = static_cast<double>(j) / static_cast<double>(sSegs);
            for (int i = 0; i < tVerts; ++i) {
                const double t = tStart + static_cast<double>(i) * (tEnd - tStart) * tInv;
                const Eigen::Vector3d pos = evaluate(t, s);

                // 法线 = C'(t) × n（归一化）
                const Eigen::Vector3d rawN = evaluateRawNormal(t);
                Eigen::Vector3d n = rawN;
                const double nLen = n.norm();
                if (nLen > 1e-20) {
                    n /= nLen;
                } else {
                    n = Eigen::Vector3d::UnitZ();
                }

                mesh.vertices.push_back(pos);
                mesh.normals.push_back(n);
            }
        }

        // 生成三角形索引
        mesh.indices.reserve(tSegs * sSegs * 6);
        for (int j = 0; j < sSegs; ++j) {
            for (int i = 0; i < tSegs; ++i) {
                const auto idx00 = static_cast<std::uint32_t>(j * tVerts + i);
                const auto idx10 = static_cast<std::uint32_t>(j * tVerts + i + 1);
                const auto idx01 = static_cast<std::uint32_t>((j + 1) * tVerts + i);
                const auto idx11 = static_cast<std::uint32_t>((j + 1) * tVerts + i + 1);

                // 三角形 1
                mesh.indices.push_back(idx00);
                mesh.indices.push_back(idx01);
                mesh.indices.push_back(idx10);
                // 三角形 2
                mesh.indices.push_back(idx10);
                mesh.indices.push_back(idx01);
                mesh.indices.push_back(idx11);
            }
        }

        return mesh;
    }

    // ==================== 私有方法 ====================

    Eigen::Vector3d SurfaceCylindrical::evaluate(
        const double t, const double s) const
    {
        return m_curve->evaluate(t) + s * m_direction;
    }

    Eigen::Vector3d SurfaceCylindrical::evaluateRawNormal(
        const double t) const
    {
        // N(t) = C'(t) × n
        return m_curve->derivative(t).cross(m_direction);
    }

    std::pair<double, double> SurfaceCylindrical::findClosestParameters(
        const Eigen::Vector3d& worldPoint) const
    {
        const double tStart = m_curve->getStartT();
        const double tEnd   = m_curve->getEndT();
        const double tRange = tEnd - tStart;

        constexpr std::size_t kTSamples = 64;
        constexpr std::size_t kSSamples = 8;

        // === 阶段 1：粗采样搜索 ===
        double bestDist2 = std::numeric_limits<double>::max();
        double bestT = tStart;
        double bestS = 0.0;

        for (std::size_t i = 0; i < kTSamples; ++i) {
            const double t = tStart + static_cast<double>(i) / (kTSamples - 1) * tRange;

            // 最优 s：s* = clamp(n·(Q - C(t)) / |n|², 0, 1)
            const Eigen::Vector3d curvePoint = m_curve->evaluate(t);
            const Eigen::Vector3d diff = worldPoint - curvePoint;
            const double sOpt = std::clamp(
                diff.dot(m_direction) / m_dirLenSq, 0.0, 1.0);

            // 同时采样 s 方向
            for (std::size_t j = 0; j < kSSamples; ++j) {
                const double s = sOpt; // 先聚焦在最优 s 附近
                const Eigen::Vector3d sp = curvePoint + s * m_direction;
                const double d2 = (sp - worldPoint).squaredNorm();
                if (d2 < bestDist2) {
                    bestDist2 = d2;
                    bestT = t;
                    bestS = s;
                }
            }
        }

        // 也试试 s=0 和 s=1 边界
        for (std::size_t i = 0; i < kTSamples; ++i) {
            const double t = tStart + static_cast<double>(i) / (kTSamples - 1) * tRange;
            const Eigen::Vector3d curvePoint = m_curve->evaluate(t);

            for (const double s : {0.0, 1.0}) {
                const Eigen::Vector3d sp = curvePoint + s * m_direction;
                const double d2 = (sp - worldPoint).squaredNorm();
                if (d2 < bestDist2) {
                    bestDist2 = d2;
                    bestT = t;
                    bestS = s;
                }
            }
        }

        // === 阶段 2：迭代细化 ===
        // 使用坐标下降法：交替优化 t（黄金分割）和 s（解析解）
        constexpr std::size_t kRefineIters = 12;
        double t = bestT;
        double s = bestS;

        for (std::size_t iter = 0; iter < kRefineIters; ++iter) {
            // 1. 固定 t，解析求解最优 s
            {
                const Eigen::Vector3d curvePoint = m_curve->evaluate(t);
                const Eigen::Vector3d diff = worldPoint - curvePoint;
                s = std::clamp(diff.dot(m_direction) / m_dirLenSq, 0.0, 1.0);
            }

            // 2. 固定 s，在 t 附近做黄金分割搜索
            {
                // 搜索区间 [t_left, t_right]
                const double searchRadius = tRange * std::max(1e-4, std::pow(0.5, static_cast<double>(iter) / 4.0));
                double tLeft  = std::max(tStart, t - searchRadius);
                double tRight = std::min(tEnd,   t + searchRadius);

                // 黄金分割比例
                constexpr double kPhi = 0.6180339887498949;

                for (std::size_t gs = 0; gs < 8; ++gs) {
                    const double t1 = tRight - kPhi * (tRight - tLeft);
                    const double t2 = tLeft  + kPhi * (tRight - tLeft);

                    const double f1 = (evaluate(t1, s) - worldPoint).squaredNorm();
                    const double f2 = (evaluate(t2, s) - worldPoint).squaredNorm();

                    if (f1 < f2) {
                        tRight = t2;
                    } else {
                        tLeft = t1;
                    }
                }

                t = 0.5 * (tLeft + tRight);
            }
        }

        m_lastT = t;
        m_lastS = s;
        return {t, s};
    }
} // egret
