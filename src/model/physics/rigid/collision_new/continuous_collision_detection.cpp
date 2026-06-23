/**
 * @file        continuous_collision_detection.cpp
 * @brief       CCD（连续碰撞检测）算法实现
 * @details     实现 TOI 二分搜索与碰撞信息提取，底层统一使用 GJK + EPA。
 *              算法遵循文档 Phase 2 Chapter-3 的阶段一至阶段四流程。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-23
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 */

#include "continuous_collision_detection.h"
#include "ccd_integrator_adapter.h"
#include "gilbert_johnson_keerthi.h"
#include "expanding_polytope_algorithm.h"
#include <algorithm>

namespace egret
{

    //  构造


    ContinuousCollisionDetection::ContinuousCollisionDetection(
        PhysicalEntity* entityA, PhysicalEntity* entityB)
        : m_entityA(entityA)
        , m_entityB(entityB)
    {
    }


    //  内部辅助 — 将实体 Transform 设置为指定时刻的预测值


    namespace
    {
        /**
         * @brief 将两个实体的 Transform 设置为 t 时刻的预测值（严格半隐式欧拉）
         */
        void setEntityTransformsAt(
            PhysicalEntity* entityA,
            PhysicalEntity* entityB,
            const Transform& initA,
            const Transform& initB,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            const Eigen::Vector3d& linearAccelA,
            const Eigen::Vector3d& angularAccelA,
            const Eigen::Vector3d& linearAccelB,
            const Eigen::Vector3d& angularAccelB,
            double t)
        {
            const Transform predA = CcdIntegratorAdapter::predictTransform(
                initA, linearVelA, angularVelA,
                linearAccelA, angularAccelA, t);
            const Transform predB = CcdIntegratorAdapter::predictTransform(
                initB, linearVelB, angularVelB,
                linearAccelB, angularAccelB, t);

            entityA->setPosition(predA.getTranslation());
            entityA->setRotation(predA.getRotation());
            entityA->setScale(predA.getScale());
            entityB->setPosition(predB.getTranslation());
            entityB->setRotation(predB.getRotation());
            entityB->setScale(predB.getScale());
        }

        /**
         * @brief 将实体 Transform 恢复为指定值
         */
        void restoreEntityTransforms(
            PhysicalEntity* entityA,
            PhysicalEntity* entityB,
            const Transform& initA,
            const Transform& initB)
        {
            entityA->setPosition(initA.getTranslation());
            entityA->setRotation(initA.getRotation());
            entityA->setScale(initA.getScale());
            entityB->setPosition(initB.getTranslation());
            entityB->setRotation(initB.getRotation());
            entityB->setScale(initB.getScale());
        }
    }


    //  gjkCollisionCheckAt — 在时刻 t 做 GJK 碰撞检测


    bool ContinuousCollisionDetection::gjkCollisionCheckAt(
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        const Eigen::Vector3d& linearAccelA,
        const Eigen::Vector3d& angularAccelA,
        const Eigen::Vector3d& linearAccelB,
        const Eigen::Vector3d& angularAccelB,
        const double t)
    {
        setEntityTransformsAt(m_entityA, m_entityB,
                              m_initialTransformA, m_initialTransformB,
                              linearVelA, angularVelA,
                              linearVelB, angularVelB,
                              linearAccelA, angularAccelA,
                              linearAccelB, angularAccelB, t);

        GilbertJohnsonKeerthi gjk(m_entityA, m_entityB);
        gjk.initialize();
        m_lastGjkResult = gjk.run();

        return m_lastGjkResult.isCollided;
    }


    //  gjkDistanceAt — 在时刻 t 做 GJK 距离查询
    double ContinuousCollisionDetection::gjkDistanceAt(
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        const Eigen::Vector3d& linearAccelA,
        const Eigen::Vector3d& angularAccelA,
        const Eigen::Vector3d& linearAccelB,
        const Eigen::Vector3d& angularAccelB,
        const double t)
    {
        setEntityTransformsAt(m_entityA, m_entityB,
                              m_initialTransformA, m_initialTransformB,
                              linearVelA, angularVelA,
                              linearVelB, angularVelB,
                              linearAccelA, angularAccelA,
                              linearAccelB, angularAccelB, t);

        GilbertJohnsonKeerthi gjk(m_entityA, m_entityB);
        gjk.initialize();
        m_lastGjkResult = gjk.run();

        if (m_lastGjkResult.isCollided) {
            // 已穿透——返回负值（具体数值不重要，二分搜索只需符号）
            // 实际穿透深度需要用 EPA 获取，这里返回负的哨兵值
            return -1.0;
        }

        return m_lastGjkResult.distance;
    }


    //  computeToi — 主 CCD 入口
    std::optional<CcdResult> ContinuousCollisionDetection::computeToi(
        const Eigen::Vector3d& linearVelocityA,
        const Eigen::Vector3d& angularVelocityA,
        const Eigen::Vector3d& linearVelocityB,
        const Eigen::Vector3d& angularVelocityB,
        const double dt)
    {
        if (m_entityA == nullptr || m_entityB == nullptr) {
            return std::nullopt;
        }

        // ── 保存初始 Transform ──
        m_initialTransformA = m_entityA->getTransformCR();
        m_initialTransformB = m_entityB->getTransformCR();

        // ── 计算加速度（严格半隐式欧拉所需）──
        const double massA = m_entityA->getMass();
        const double massB = m_entityB->getMass();

        Eigen::Vector3d linearAccelA{Eigen::Vector3d::Zero()};
        Eigen::Vector3d linearAccelB{Eigen::Vector3d::Zero()};
        if (massA > 0.0) {
            linearAccelA = m_entityA->getJoinForce() / massA;
        }
        if (massB > 0.0) {
            linearAccelB = m_entityB->getJoinForce() / massB;
        }

        // 逆惯量矩阵（世界坐标系）→ 角加速度 α = I⁻¹·τ
        Eigen::Vector3d angularAccelA = Eigen::Vector3d::Zero();
        Eigen::Vector3d angularAccelB = Eigen::Vector3d::Zero();

        if (massA > 0.0 && m_entityA->hasRotation()) {
            const auto shapeA = m_entityA->getShape();
            if (shapeA) {
                const Eigen::Matrix3d I_local = shapeA->getInertiaTensor(massA);
                const Eigen::Matrix3d R = m_initialTransformA.getRotationMatrix();
                const Eigen::Matrix3d I_world = R * I_local * R.transpose();
                angularAccelA = I_world.inverse() * m_entityA->getTorque();
            }
        }

        if (massB > 0.0 && m_entityB->hasRotation()) {
            const auto shapeB = m_entityB->getShape();
            if (shapeB) {
                const Eigen::Matrix3d I_local = shapeB->getInertiaTensor(massB);
                const Eigen::Matrix3d R = m_initialTransformB.getRotationMatrix();
                const Eigen::Matrix3d I_world = R * I_local * R.transpose();
                angularAccelB = I_world.inverse() * m_entityB->getTorque();
            }
        }

        //  Phase 1: 预测 t=Δt 时刻的碰撞检测

        const bool collidedAtDt = gjkCollisionCheckAt(
            linearVelocityA, angularVelocityA,
            linearVelocityB, angularVelocityB,
            linearAccelA, angularAccelA,
            linearAccelB, angularAccelB, dt);

        if (!collidedAtDt) {
            // 整个时间步内未发生碰撞——无需 CCD
            restoreEntityTransforms(m_entityA, m_entityB,
                                    m_initialTransformA, m_initialTransformB);
            return std::nullopt;
        }

        // ── 获取 t=Δt 时刻的 EPA 结果，用于 Phase 4 回退 ──
        ExpandingPolytopeAlgorithm epaDt(m_lastGjkResult, m_entityA, m_entityB);
        const EPAResult epaResultDt = epaDt.run();

        //  Phase 2: 提取 t=0 时刻的初始间隙 d₀

        const double d0 = gjkDistanceAt(
            linearVelocityA, angularVelocityA,
            linearVelocityB, angularVelocityB,
            linearAccelA, angularAccelA,
            linearAccelB, angularAccelB, 0.0);

        if (d0 <= 0.0) {
            // 初始状态已穿透——TOI = 0
            // 在 t=0 处运行 EPA 获取接触信息
            // （此时实体 Transform 已在 t=0 状态，由 gjkDistanceAt 设置）
            ExpandingPolytopeAlgorithm epa0(m_lastGjkResult, m_entityA, m_entityB);
            const EPAResult epaResult0 = epa0.run();

            restoreEntityTransforms(m_entityA, m_entityB,
                                    m_initialTransformA, m_initialTransformB);

            return CcdResult{
                .toi = 0.0,
                .normal = epaResult0.normal,
                .contactPosition = epaResult0.contactPoint,
                .penetration = epaResult0.penetrationDepth
            };
        }

        //  Phase 3: 二分搜索求解 TOI τ_c
        //  已知：Φ(0) = d₀ > 0（分离），Φ(Δt) < 0（穿透）
        //  根据介值定理，存在唯一的 τ_c ∈ (0, Δt) 使 Φ(τ_c) = 0

        double tLow = 0.0;
        double tHigh = dt;

        for (int iter = 0; iter < kMaxToiIterations; ++iter) {
            const double tMid = (tLow + tHigh) * 0.5;

            const bool collidedAtMid = gjkCollisionCheckAt(
                linearVelocityA, angularVelocityA,
                linearVelocityB, angularVelocityB,
                linearAccelA, angularAccelA,
                linearAccelB, angularAccelB, tMid);

            if (!collidedAtMid) {
                // 仍分离 → 接触时刻在 [tMid, tHigh]
                tLow = tMid;
            } else {
                // 已穿透 → 接触时刻在 [tLow, tMid]
                tHigh = tMid;
                // m_lastGjkResult 已被 gjkCollisionCheckAt 更新，
                // 保留最近一次穿透的 GJK 结果供后续 EPA 使用
            }

            // 区间足够小时提前终止
            if (tHigh - tLow < kToiEpsilon) {
                break;
            }
        }

        const double toi = (tLow + tHigh) * 0.5;

        //  Phase 4: 在 τ_c 时刻提取接触信息（EPA）

        // 在 TOI 时刻再做一次完整的 GJK → EPA
        setEntityTransformsAt(m_entityA, m_entityB,
                              m_initialTransformA, m_initialTransformB,
                              linearVelocityA, angularVelocityA,
                              linearVelocityB, angularVelocityB,
                              linearAccelA, angularAccelA,
                              linearAccelB, angularAccelB, toi);

        GilbertJohnsonKeerthi gjkToi(m_entityA, m_entityB);
        gjkToi.initialize();
        const GJKResult3D gjkResultToi = gjkToi.run();

        // 期望在 TOI 处恰好接触（穿透深度 ≈ 0）
        // 若 GJK 判定为碰撞，则用 EPA 提取精确信息
        CcdResult result;
        result.toi = toi;

        if (gjkResultToi.isCollided) {
            ExpandingPolytopeAlgorithm epaToi(gjkResultToi, m_entityA, m_entityB);
            const EPAResult epaResultToi = epaToi.run();

            result.normal = epaResultToi.normal;
            result.contactPosition = epaResultToi.contactPoint;
            result.penetration = epaResultToi.penetrationDepth;
        } else {
            // 回退：GJK 在 TOI 处未检测到碰撞（二分边界稍偏早）
            // 使用 GJK 最近点对推算接触法线与接触点：
            //   normal = normalize(a₀ - b₀)（从 B 指向 A）
            //   contactPosition = (a₀ + b₀) / 2
            const auto& [a0, b0] = gjkResultToi.minimumVerticesPair;
            const Eigen::Vector3d diff = a0 - b0;
            const double diffNorm = diff.norm();

            if (diffNorm > 1e-10) {
                result.normal = diff / diffNorm;
            } else {
                // 最近点对几乎重合，回退使用 t=Δt 时刻 EPA 法线
                result.normal = epaResultDt.normal;
            }
            result.contactPosition = (a0 + b0) * 0.5;
            result.penetration = 0.0;
        }

        //  恢复实体的原始 Transform

        restoreEntityTransforms(m_entityA, m_entityB,
                                m_initialTransformA, m_initialTransformB);

        return result;
    }

} // namespace egret
