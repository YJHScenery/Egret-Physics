/**
 * @file        sliding_rail.cpp
 * @brief       滑轨约束器实现文件。
 * @details     实现 SlidingRail 类的各项约束求解方法：
 *              - applyConstraintForces（主要）：分解力 → 注入法向反作用力
 *              - applyPositionConstraint：将质心投影到轨道
 *              - applyVelocityConstraint：移除法向速度并限制端点
 *              - applyFrictionForces：依据反力计算切向摩擦
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

        double tFinal = (tLow + tHigh) / 2.0;
        const double refinedDist = (m_curve->evaluate(tFinal) - point).squaredNorm();
        if ((m_curve->evaluate(tStart) - point).squaredNorm() <= refinedDist) tFinal = tStart;
        if ((m_curve->evaluate(tEnd) - point).squaredNorm() < (m_curve->evaluate(tFinal) - point).squaredNorm()) tFinal = tEnd;
        return {tFinal, m_curve->evaluate(tFinal)};
    }

    //  约束力注入（主要约束手段） 

    void SlidingRail::applyReactionForce(PhysicalEntity* entity,
                                         const double t,
        [[maybe_unused]] const Eigen::Vector3d& closest) const
    {
        if (entity == nullptr || entity->isFixed()) {
            return;
        }

        // 1. 计算合力
        Eigen::Vector3d totalForce = Eigen::Vector3d::Zero();
        for (const auto& f : entity->getForcesCR()) {
            if (f.id != m_id) totalForce += f.force;
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

        // 曲率产生向心加速度，采用切向量的有限差分计算 dT/ds。
        const double tStart = m_curve->getStartT();
        const double tEnd = m_curve->getEndT();
        const double step = std::max((tEnd - tStart) * 1e-5, 1e-9);
        const double left = std::max(tStart, t - step);
        const double right = std::min(tEnd, t + step);
        const double derivativeNorm = m_curve->derivative(t).norm();
        Eigen::Vector3d curvature = Eigen::Vector3d::Zero();
        if (right > left && derivativeNorm > 1e-12) {
            curvature = (getUnitTangent(right) - getUnitTangent(left)) / ((right - left) * derivativeNorm);
            curvature -= tangent * curvature.dot(tangent);
        }
        const double speed = entity->getVelocity().dot(tangent);
        const Eigen::Vector3d reactionForce = entity->getMass() * speed * speed * curvature - fNormal;
        entity->upsertForce(Force{reactionForce, m_id});

        // 此约束限定质心轨迹，不为数值位置误差引入虚假的力矩。
    }

    void SlidingRail::applyConstraintForces()
    {
        if (!m_enabled || m_curve == nullptr) {
            return;
        }

        for (auto* entity : m_registeredEntities) {
            if (entity == nullptr || !entity->isActive() || entity->isFixed()) {
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

    void SlidingRail::applyFrictionForces(const double dt)
    {
        if (!m_enabled || !m_curve || !std::isfinite(dt) || dt <= 0.0) return;
        for (auto* entity : m_registeredEntities) {
            if (!entity || !entity->isActive() || entity->isFixed() || entity->getMass() <= 0.0) continue;
            const auto it = m_entityParamMap.find(entity->getId());
            if (it == m_entityParamMap.end()) continue;
            const Eigen::Vector3d tangent = getUnitTangent(it->second);
            if (tangent.squaredNorm() < 1e-12) continue;
            Eigen::Vector3d external = Eigen::Vector3d::Zero();
            Eigen::Vector3d reaction = Eigen::Vector3d::Zero();
            for (const auto& force : entity->getForcesCR()) {
                if (force.id != m_id) external += force.force;
                else reaction += force.force;
            }
            reaction -= reaction.dot(tangent) * tangent;
            const double normalLoad = reaction.norm();
            const double predictedVelocity = entity->getVelocity().dot(tangent) + external.dot(tangent) * dt / entity->getMass();
            const double desiredForce = -predictedVelocity * entity->getMass() / dt;
            const double staticLimit = std::sqrt(m_frictionCoeff) * std::sqrt(entity->getStaticFriction()) * normalLoad;
            const double dynamicLimit = std::sqrt(m_dynamicFrictionCoeff) * std::sqrt(entity->getDynamicFriction()) * normalLoad;
            const double friction = std::abs(desiredForce) <= staticLimit ? desiredForce
                : std::copysign(std::min(dynamicLimit, std::abs(desiredForce)), desiredForce);
            entity->upsertForce(Force{reaction + friction * tangent, m_id});
        }
    }

    //  位置微调 

    void SlidingRail::applyPositionConstraint([[maybe_unused]] const double dt)
    {
        if (!m_enabled || m_curve == nullptr) {
            return;
        }

        for (auto* entity : m_registeredEntities) {
            if (entity == nullptr || !entity->isActive() || entity->isFixed()) {
                continue;
            }

            const std::uint64_t entityId = entity->getId();
            const Eigen::Vector3d pos = entity->getPosition();

            // 查找最近曲线点
            auto it = m_entityParamMap.find(entityId);
            const bool firstProjection = it == m_entityParamMap.end();
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
            const double factor = firstProjection
                                      ? m_firstSnapFactor // 首次出现，快速拉入
                                      : m_pullBackFactor; // 常规微调

            // 轻柔回拉
            constexpr double k_minMove = 1e-8;
            const double moveDist = dist * factor;
            if (moveDist > k_minMove) {
                const Eigen::Vector3d offsetWithFactor {offset * factor};
                const Eigen::Vector3d newPos = pos + offsetWithFactor;

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
            if (entity == nullptr || !entity->isActive() || entity->isFixed()) {
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
            double vParallelMag = v.dot(tangent);
            const double epsilon = 1e-8 * std::max(1.0, m_curve->getEndT() - m_curve->getStartT());
            if ((t <= m_curve->getStartT() + epsilon && vParallelMag < 0.0) ||
                (t >= m_curve->getEndT() - epsilon && vParallelMag > 0.0)) vParallelMag = 0.0;
            const Eigen::Vector3d vParallel = vParallelMag * tangent;
            const Eigen::Vector3d vNormal = v - vParallel;

            const double vNormalMag = vNormal.norm();
            if (vNormalMag < m_velocityTolerance && (v - vParallel).norm() < m_velocityTolerance) {
                continue;
            }

            // 衰减法向速度分量
            const Eigen::Vector3d vNew = vParallel + vNormal * (1.0 - m_normalDampingFactor);

            entity->setSpeed(vNew);
        }
    }

    //  角速度微调
    void SlidingRail::applyAngularVelocityConstraint([[maybe_unused]] const double dt)
    {
        // 轨道只约束质心位置，物体仍保有旋转自由度。
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

            const Eigen::Vector3d pos = entity->getPosition();
            const Eigen::Vector3d closest = findClosestPointOnCurve(pos, guess).second;
            const double dist = (closest - pos).norm();
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
          , m_dynamicFrictionCoeff(other.m_dynamicFrictionCoeff)
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
        m_dynamicFrictionCoeff = other.m_dynamicFrictionCoeff;

        return *this;
    }

    std::unique_ptr<PhysicsAbstract> SlidingRail::clone(const std::uint64_t id) const
    {
        auto rail = std::make_unique<SlidingRail>(*this);
        rail->setId(id);
        return rail;
    }
} // egret
