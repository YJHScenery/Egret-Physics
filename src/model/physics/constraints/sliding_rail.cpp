/**
 * @file        sliding_rail.cpp
 * @brief       滑轨约束器实现文件。
 * @details     实现 SlidingRail 类的各项约束求解方法：
 *              - applyConstraintForces（主要）：分解力 → 注入法向反作用力
 *              - applyPositionConstraint（微调）：偏离轨道时轻柔回拉
 *              - applyVelocityConstraint（微调）：法向速度超限时衰减
 *              - applyAngularVelocityConstraint（微调）：朝向偏差超限时修正
 *              - findClosestPointOnCurve：采样 + 三分搜索精化
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-02
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Constraints
 */

#include "sliding_rail.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "physical_entity.h"

namespace egret
{
    //  构造与析构 

    SlidingRail::SlidingRail(std::uint64_t id,
                             std::unique_ptr<CurveBase> curve)
        : PhysicsAbstract(id)
          , ConstraintsBase(id)
          , m_curve(std::move(curve))
    {
    }

    ConstraintType SlidingRail::getType() const
    {
        return ConstraintType::SlidingRail;
    }

    //  辅助方法 

    Eigen::Vector3d SlidingRail::getUnitTangent(const double t) const
    {
        if (m_curve == nullptr) {
            return Eigen::Vector3d::Zero();
        }

        const Eigen::Vector3d deriv = m_curve->derivative(t);
        const double norm = deriv.norm();
        if (norm < 1e-12) {
            return Eigen::Vector3d::Zero();
        }
        return deriv / norm;
    }

    std::pair<double, Eigen::Vector3d> SlidingRail::findClosestPointOnCurve(
        const Eigen::Vector3d& point,
        const std::optional<double> tInitialGuess) const
    {
        if (m_curve == nullptr) {
            return {0.0, Eigen::Vector3d::Zero()};
        }

        const double tStart = m_curve->getStartT();
        const double tEnd = m_curve->getEndT();
        const double tRange = tEnd - tStart;

        //  阶段 1：均匀采样，找到最近点 

        double tBest = tStart;
        double distSq = (m_curve->evaluate(tStart) - point).squaredNorm();

        // 如果提供了初始猜测，先检查初始猜测
        if (tInitialGuess.has_value()) {
            const double tg = std::clamp(tInitialGuess.value(), tStart, tEnd);
            const double dSq = (m_curve->evaluate(tg) - point).squaredNorm();
            if (dSq < distSq) {
                distSq = dSq;
                tBest = tg;
            }
        }

        // 均匀采样
        for (std::size_t i = 1; i <= k_sampleCount; ++i) {
            const double t = tStart + (static_cast<double>(i) / static_cast<double>(k_sampleCount)) * tRange;
            const double dSq = (m_curve->evaluate(t) - point).squaredNorm();
            if (dSq < distSq) {
                distSq = dSq;
                tBest = t;
            }
        }

        //  阶段 2：三分搜索精化 

        double refineRadius = tRange * k_refineRadius;
        double tLow = std::max(tStart, tBest - refineRadius);
        double tHigh = std::min(tEnd, tBest + refineRadius);

        for (int iter = 0; iter < k_refineIterations; ++iter) {
            const double tMid1 = tLow + (tHigh - tLow) / 3.0;
            const double tMid2 = tHigh - (tHigh - tLow) / 3.0;

            const double dSq1 = (m_curve->evaluate(tMid1) - point).squaredNorm();
            const double dSq2 = (m_curve->evaluate(tMid2) - point).squaredNorm();

            if (dSq1 < dSq2) {
                tHigh = tMid2;
            }
            else {
                tLow = tMid1;
            }
        }

        const double tFinal = (tLow + tHigh) / 2.0;
        return {tFinal, m_curve->evaluate(tFinal)};
    }

    //  约束力注入（主要约束手段） 

    void SlidingRail::applyReactionForce(PhysicalEntity* entity,
                                         const double t,
                                         const Eigen::Vector3d& closest) const
    {
        if (entity == nullptr || entity->isFixed()) {
            return;
        }

        // 1. 计算合力
        Eigen::Vector3d totalForce = Eigen::Vector3d::Zero();
        for (const auto& f : entity->getForcesCR()) {
            totalForce += f.force;
        }

        // 若无外力，无需施加反力
        if (totalForce.squaredNorm() < 1e-20) {
            return;
        }

        // 2. 分解为切向和法向分量
        const Eigen::Vector3d tangent = getUnitTangent(t);
        if (tangent.squaredNorm() < 1e-12) {
            // 切向未定义（退化曲线点），无法分解，跳过
            return;
        }

        const double fParallelMag = totalForce.dot(tangent);
        const Eigen::Vector3d fParallel = fParallelMag * tangent;
        const Eigen::Vector3d fNormal = totalForce - fParallel;

        // 3. 若法向力可忽略，无需施加反力
        if (fNormal.squaredNorm() < 1e-20) {
            return;
        }

        // 4. 注入法向反作用力（核心约束）
        const Eigen::Vector3d reactionForce = -fNormal;
        entity->upsertForce(Force{reactionForce, m_id});

        // 5. 若作用点（曲线最近点）偏离质心，注入等效力矩
        const Eigen::Vector3d com = entity->getPosition();
        const Eigen::Vector3d r = closest - com;
        if (r.squaredNorm() > 1e-12) {
            const Eigen::Vector3d reactionTorque = r.cross(reactionForce);
            entity->upsertTorque(Torque{reactionTorque, m_id});
        }
    }

    void SlidingRail::applyConstraintForces()
    {
        if (!m_enabled || m_curve == nullptr) {
            return;
        }

        for (auto* entity : m_registeredEntities) {
            if (entity == nullptr || entity->isFixed()) {
                continue;
            }

            // 获取/初始化实体在曲线上的参数 t
            auto it = m_entityParamMap.find(entity->getId());
            double t;
            Eigen::Vector3d closest;

            if (it == m_entityParamMap.end()) {
                // 首次出现：投影到曲线并记录参数
                auto [tNew, pt] = findClosestPointOnCurve(entity->getPosition());
                t = tNew;
                closest = pt;
                m_entityParamMap[entity->getId()] = t;
            }
            else {
                t = it->second;
                closest = m_curve->evaluate(t);

                // 由于实体可能在轨道上移动，重新查找最近点以更新 t
                auto [tNew, pt] = findClosestPointOnCurve(entity->getPosition(), t);
                t = tNew;
                closest = pt;
                // 更新参数映射
                m_entityParamMap[entity->getId()] = t;
            }

            applyReactionForce(entity, t, closest);
        }
    }

    //  位置微调 

    void SlidingRail::applyPositionConstraint([[maybe_unused]] const double dt)
    {
        if (!m_enabled || m_curve == nullptr) {
            return;
        }

        for (auto* entity : m_registeredEntities) {
            if (entity == nullptr || entity->isFixed()) {
                continue;
            }

            const std::uint64_t entityId = entity->getId();
            const Eigen::Vector3d pos = entity->getPosition();

            // 查找最近曲线点
            auto it = m_entityParamMap.find(entityId);
            std::optional<double> guess = (it != m_entityParamMap.end())
                                              ? std::optional(it->second)
                                              : std::nullopt;

            const auto [t, closest] = findClosestPointOnCurve(pos, guess);

            // 更新参数映射
            m_entityParamMap[entityId] = t;

            // 计算偏移距离
            const Eigen::Vector3d offset = closest - pos;
            const double dist = offset.norm();

            if (dist < m_positionTolerance) {
                continue;
            }

            // 确定拉回系数
            const double factor = (it == m_entityParamMap.end())
                                      ? m_firstSnapFactor // 首次出现，快速拉入
                                      : m_pullBackFactor; // 常规微调

            // 轻柔回拉
            constexpr double k_minMove = 1e-8;
            const double moveDist = dist * factor;
            if (moveDist > k_minMove) {
                const Eigen::Vector3d offsetWithFactor {offset * factor};
                const Eigen::Vector3d newPos = pos + offsetWithFactor;

                LOG_DEBUG(std::format("offset: {}, {}, {}", offsetWithFactor.x(), offsetWithFactor.y(), offsetWithFactor.z()));

                entity->setPosition(newPos);
            }
        }
    }

    //  速度微调
    void SlidingRail::applyVelocityConstraint([[maybe_unused]] const double dt)
    {

        if (!m_enabled || m_curve == nullptr) {
            return;
        }

        for (auto* entity : m_registeredEntities) {
            if (entity == nullptr || entity->isFixed()) {
                continue;
            }

            auto it = m_entityParamMap.find(entity->getId());
            if (it == m_entityParamMap.end()) {
                continue;
            }

            const double t = it->second;
            const Eigen::Vector3d tangent = getUnitTangent(t);
            if (tangent.squaredNorm() < 1e-12) {
                continue;
            }

            const Eigen::Vector3d v = entity->getVelocity();

            // 分解速度
            const double vParallelMag = v.dot(tangent);
            const Eigen::Vector3d vParallel = vParallelMag * tangent;
            const Eigen::Vector3d vNormal = v - vParallel;

            const double vNormalMag = vNormal.norm();
            if (vNormalMag < m_velocityTolerance) {
                continue;
            }

            // 衰减法向速度分量
            const Eigen::Vector3d vNew = vParallel + vNormal * (1.0 - m_normalDampingFactor);

            LOG_DEBUG(std::format("New Speed - Old Speed = {}" , (vNew - entity->getSpeedCR()).norm()));

            entity->setSpeed(vNew);
        }
    }

    //  角速度微调
    void SlidingRail::applyAngularVelocityConstraint([[maybe_unused]] const double dt)
    {
        if (!m_enabled || m_curve == nullptr || dt <= 0.0) {
            return;
        }

        for (auto* entity : m_registeredEntities) {
            if (entity == nullptr || entity->isFixed()) {
                continue;
            }

            auto it = m_entityParamMap.find(entity->getId());
            if (it == m_entityParamMap.end()) {
                continue;
            }

            const double t = it->second;
            const Eigen::Vector3d tangent = getUnitTangent(t);
            if (tangent.squaredNorm() < 1e-12) {
                continue;
            }

            // 获取实体当前旋转矩阵的第一列作为"前方"参考方向
            const Eigen::Matrix3d rotMat = entity->getTransformCR().getRotationMatrix();
            const Eigen::Vector3d forward = rotMat.col(0);

            // 计算前方方向与曲线切向的偏差
            const Eigen::Vector3d cross = forward.cross(tangent);
            const double sinAngle = cross.norm();

            if (sinAngle <= m_angularTolerance) {
                continue;
            }

            // 轻柔角速度修正
            constexpr double k_angularCorrectionFactor = 0.1;
            const Eigen::Vector3d axis = cross.normalized();
            const double angle = std::asin(std::clamp(sinAngle, -1.0, 1.0));
            const Eigen::Vector3d deltaOmega = axis * angle * k_angularCorrectionFactor / dt;

            entity->setAngular(entity->getAngularCR() + deltaOmega);
        }
    }

    //  约束误差 

    double SlidingRail::computeConstraintError() const
    {
        if (m_curve == nullptr) {
            return 0.0;
        }

        double maxDist = 0.0;

        for (auto* entity : m_registeredEntities) {
            if (entity == nullptr) {
                continue;
            }

            auto it = m_entityParamMap.find(entity->getId());
            std::optional<double> guess = (it != m_entityParamMap.end())
                                              ? std::optional(it->second)
                                              : std::nullopt;

            // 注意：findClosestPointOnCurve 不是 const 方法因为它调用了 m_curve，
            // 但 m_curve 的 evaluate/derivative 是 const 方法，所以这里实际上是 const 安全的。
            const double tStart = m_curve->getStartT();
            const double tEnd = m_curve->getEndT();

            // 简化版距离计算（不修改映射，仅计算距离）
            const Eigen::Vector3d pos = entity->getPosition();

            // 采样查找最近距离
            const double tRange = tEnd - tStart;
            double bestDistSq = std::numeric_limits<double>::max();

            if (guess.has_value()) {
                const double tg = std::clamp(guess.value(), tStart, tEnd);
                bestDistSq = (m_curve->evaluate(tg) - pos).squaredNorm();
            }

            for (std::size_t i = 0; i <= k_sampleCount; ++i) {
                const double ts = tStart + (static_cast<double>(i) / static_cast<double>(k_sampleCount)) * tRange;
                const double dSq = (m_curve->evaluate(ts) - pos).squaredNorm();
                if (dSq < bestDistSq) {
                    bestDistSq = dSq;
                }
            }

            const double dist = std::sqrt(bestDistSq);
            if (dist > maxDist) {
                maxDist = dist;
            }
        }

        return maxDist;
    }

    //  拷贝与克隆 

    SlidingRail::SlidingRail(const SlidingRail& other)
        : PhysicsAbstract(other)
          , ConstraintsBase(other)
          , m_curve(other.m_curve ? other.m_curve->clone() : nullptr)
          , m_entityParamMap(other.m_entityParamMap)
          , m_positionTolerance(other.m_positionTolerance)
          , m_velocityTolerance(other.m_velocityTolerance)
          , m_angularTolerance(other.m_angularTolerance)
          , m_pullBackFactor(other.m_pullBackFactor)
          , m_firstSnapFactor(other.m_firstSnapFactor)
          , m_normalDampingFactor(other.m_normalDampingFactor)
          , m_frictionCoeff(other.m_frictionCoeff)
    {
    }

    SlidingRail& SlidingRail::operator=(const SlidingRail& other)
    {
        if (this == &other) {
            return *this;
        }

        PhysicsAbstract::operator=(other);
        ConstraintsBase::operator=(other);
        m_curve = other.m_curve ? other.m_curve->clone() : nullptr;
        m_entityParamMap = other.m_entityParamMap;
        m_positionTolerance = other.m_positionTolerance;
        m_velocityTolerance = other.m_velocityTolerance;
        m_angularTolerance = other.m_angularTolerance;
        m_pullBackFactor = other.m_pullBackFactor;
        m_firstSnapFactor = other.m_firstSnapFactor;
        m_normalDampingFactor = other.m_normalDampingFactor;
        m_frictionCoeff = other.m_frictionCoeff;

        return *this;
    }

    std::unique_ptr<PhysicsAbstract> SlidingRail::clone(const std::uint64_t id) const
    {
        auto rail = std::make_unique<SlidingRail>(*this);
        rail->setId(id);
        return rail;
    }
} // egret
