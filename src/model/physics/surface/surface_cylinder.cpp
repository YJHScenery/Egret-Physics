/**
 * @file        surface_cylinder.cpp
 * @brief       标准圆柱面实现文件。
 * @details     提供 SurfaceCylinder 类的所有方法实现。
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

#include "surface_cylinder.h"

namespace egret
{
    SurfaceCylinder::SurfaceCylinder(double radius,
                                     double length,
                                     double thetaMax,
                                     const Transform& transform)
        : m_radius(std::abs(radius))
        , m_length(std::abs(length))
        , m_thetaMax(std::clamp(thetaMax, 1e-6, 2.0 * egret::PI))
        , m_transform(transform)
        , m_isFullCylinder(std::abs(thetaMax - 2.0 * egret::PI) < 1e-6)
    {
    }

    std::unique_ptr<SurfaceBase> SurfaceCylinder::clone() const
    {
        return std::make_unique<SurfaceCylinder>(m_radius, m_length, m_thetaMax, m_transform);
    }

    SurfaceType SurfaceCylinder::getType() const
    {
        return SurfaceType::Cylinder;
    }

    double SurfaceCylinder::signedDistance(const Eigen::Vector3d& worldPoint) const
    {
        const Eigen::Vector3d local = m_transform.worldToLocal(worldPoint);
        return signedDistanceLocal(local);
    }

    Eigen::Vector3d SurfaceCylinder::closestPoint(const Eigen::Vector3d& worldPoint) const
    {
        const Eigen::Vector3d local = m_transform.worldToLocal(worldPoint);
        const Eigen::Vector3d closestLocal = closestPointLocal(local);
        return m_transform.localToWorld(closestLocal);
    }

    Eigen::Vector3d SurfaceCylinder::normal(const Eigen::Vector3d& worldPoint) const
    {
        const Eigen::Vector3d local = m_transform.worldToLocal(worldPoint);
        const auto [closestLocal, feature] = closestPointWithFeatureLocal(local);
        const Eigen::Vector3d nLocal = normalLocal(closestLocal, feature);
        return m_transform.localToWorld(nLocal).normalized();
    }

    bool SurfaceCylinder::collide(const ShapeBase* shape,
                                  const Transform& shapeTransform,
                                  ContactManifold& manifold) const
    {
        if (shape == nullptr) {
            return false;
        }

        // 将形状变换到圆柱的局部空间
        const Transform localShapeTransform = localizeTransform(shapeTransform);
        const Eigen::Vector3d comLocal = localShapeTransform.transformPointToWorld(
            shape->getCenterOfMass());

        // 在局部空间中，使用支撑函数检测碰撞
        const auto [closestLocal, feature] = closestPointWithFeatureLocal(comLocal);
        const Eigen::Vector3d nLocal = normalLocal(closestLocal, feature);

        // 找到形状在 -nLocal 方向上的最深点
        const Eigen::Vector3d nWorld = m_transform.localToWorld(nLocal).normalized();
        const Eigen::Vector3d deepestWorld = shape->support(-nWorld, shapeTransform);
        const double sd = signedDistance(deepestWorld);

        if (sd >= 0.0) {
            return false;
        }

        const Eigen::Vector3d cpWorld = m_transform.localToWorld(closestLocal);

        ContactPoint contact;
        contact.position = cpWorld;
        contact.normal = nWorld;
        contact.penetration = -sd;
        manifold.push_back(contact);
        return true;
    }

    // ==================== 私有辅助方法 ====================

    double SurfaceCylinder::signedDistanceLocal(const Eigen::Vector3d& local) const
    {
        const double r = std::sqrt(local.x() * local.x() + local.y() * local.y());
        const double theta = std::atan2(local.y(), local.x());
        const double thetaNorm = theta < 0.0 ? theta + 2.0 * egret::PI : theta;
        const double z = local.z();

        double dLateral = std::abs(r - m_radius);
        double dBottom = z;
        double dTop = m_length - z;
        double minDist = std::min(dLateral, std::min(dBottom, dTop));

        if (!m_isFullCylinder) {
            if (thetaNorm < 0.0 || thetaNorm > m_thetaMax) {
                const double dThetaStart = thetaNorm;
                const double dThetaEnd = std::abs(thetaNorm - m_thetaMax);
                const double dStart = r * dThetaStart;
                const double dEnd = r * dThetaEnd;
                minDist = std::min(minDist, std::min(dStart, dEnd));
            }
        }

        bool inside = true;
        if (r > m_radius) inside = false;
        if (z < 0.0 || z > m_length) inside = false;
        if (!m_isFullCylinder && (thetaNorm < 0.0 || thetaNorm > m_thetaMax)) inside = false;

        return inside ? -minDist : minDist;
    }

    Eigen::Vector3d SurfaceCylinder::closestPointLocal(const Eigen::Vector3d& local) const
    {
        auto [closest, _] = closestPointWithFeatureLocal(local);
        return closest;
    }

    std::pair<Eigen::Vector3d, SurfaceCylinder::CylinderFeature>
    SurfaceCylinder::closestPointWithFeatureLocal(const Eigen::Vector3d& local) const
    {
        const double r = std::sqrt(local.x() * local.x() + local.y() * local.y());
        const double theta = std::atan2(local.y(), local.x());
        const double thetaNorm = theta < 0.0 ? theta + 2.0 * egret::PI : theta;
        const double z = local.z();

        // 候选：侧面最近点
        const Eigen::Vector3d lateralPoint(
            m_radius * std::cos(theta),
            m_radius * std::sin(theta),
            std::clamp(z, 0.0, m_length)
        );
        double bestDist = (local - lateralPoint).squaredNorm();
        Eigen::Vector3d bestPoint = lateralPoint;
        CylinderFeature bestFeature = CylinderFeature::Lateral;

        // 候选：底部圆盘
        {
            const Eigen::Vector3d bottomPoint(
                std::min(r, m_radius) * std::cos(theta),
                std::min(r, m_radius) * std::sin(theta),
                0.0
            );
            const double d = (local - bottomPoint).squaredNorm();
            if (d < bestDist) {
                bestDist = d;
                bestPoint = bottomPoint;
                bestFeature = CylinderFeature::BottomCap;
            }

            // 候选：顶部圆盘
            const Eigen::Vector3d topPoint(
                std::min(r, m_radius) * std::cos(theta),
                std::min(r, m_radius) * std::sin(theta),
                m_length
            );
            const double dTop = (local - topPoint).squaredNorm();
            if (dTop < bestDist) {
                bestDist = dTop;
                bestPoint = topPoint;
                bestFeature = CylinderFeature::TopCap;
            }
        }

        // 对于非完整圆柱，考虑起始端面和终止端面
        if (!m_isFullCylinder) {
            auto addEndCap = [&](double thetaCap) {
                const double capX = std::cos(thetaCap);
                const double capY = std::sin(thetaCap);
                const double projR = local.x() * capX + local.y() * capY;
                const double clampedR = std::clamp(projR, 0.0, m_radius);
                const Eigen::Vector3d capPoint(
                    clampedR * capX,
                    clampedR * capY,
                    std::clamp(z, 0.0, m_length)
                );
                const double d = (local - capPoint).squaredNorm();
                return std::make_pair(capPoint, d);
            };

            auto [startPoint, dStart] = addEndCap(0.0);
            if (dStart < bestDist) {
                bestDist = dStart;
                bestPoint = startPoint;
                bestFeature = CylinderFeature::StartCap;
            }

            auto [endPoint, dEnd] = addEndCap(m_thetaMax);
            if (dEnd < bestDist) {
                bestDist = dEnd;
                bestPoint = endPoint;
                bestFeature = CylinderFeature::EndCap;
            }
        }

        return {bestPoint, bestFeature};
    }

    Eigen::Vector3d SurfaceCylinder::normalLocal(const Eigen::Vector3d& pointLocal,
                                                  CylinderFeature feature) const
    {
        switch (feature) {
        case CylinderFeature::Lateral: {
            const double rProj = std::sqrt(pointLocal.x() * pointLocal.x() +
                                           pointLocal.y() * pointLocal.y());
            if (rProj < 1e-12) {
                return Eigen::Vector3d::UnitX();
            }
            return Eigen::Vector3d(pointLocal.x() / rProj, pointLocal.y() / rProj, 0.0);
        }
        case CylinderFeature::BottomCap:
            return -Eigen::Vector3d::UnitZ();
        case CylinderFeature::TopCap:
            return Eigen::Vector3d::UnitZ();
        case CylinderFeature::StartCap:
            return Eigen::Vector3d::UnitY();
        case CylinderFeature::EndCap:
            return Eigen::Vector3d(-std::sin(m_thetaMax), std::cos(m_thetaMax), 0.0);
        }
        return Eigen::Vector3d::UnitX();
    }

    Transform SurfaceCylinder::localizeTransform(const Transform& worldTransform) const
    {
        const Eigen::Matrix4d localMat = m_transform.getWorldToLocalMatrix() *
                                         worldTransform.getLocalToWorldMatrix();

        Eigen::Matrix3d rotMat = localMat.block<3, 3>(0, 0);
        Eigen::Vector3d trans = localMat.block<3, 1>(0, 3);
        Eigen::Quaterniond rot(rotMat);

        return Transform(trans, rot);
    }
} // egret
