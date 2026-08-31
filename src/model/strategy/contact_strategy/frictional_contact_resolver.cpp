/**
 * @file        frictional_contact_resolver.cpp
 * @brief       含摩擦接触求解器实现文件
 * @details     实现 FrictionalContactResolverStrategy 的各项成员函数：
 *              热启动、速度求解（法向冲量 + Coulomb 摩擦）、位置求解（穿透修正 + 切向漂移抑制）。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-07
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Strategy
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#include "frictional_contact_resolver.h"
#include "physical_entity.h"
#include <algorithm>
#include <cmath>

namespace egret
{
    namespace
    {
        /**
         * @brief 根据法线构建两个正交切向方向。
         * @param normal 单位法向量（指向物体 A 到物体 B 的方向）。
         * @param[out] tangent1 第一切向（单位向量）。
         * @param[out] tangent2 第二切向（单位向量，tangent1 × normal）。
         */
        void buildTangents(const Eigen::Vector3d &normal,
                           Eigen::Vector3d &tangent1,
                           Eigen::Vector3d &tangent2)
        {
            // 选择一个与法线不平行的参考向量来构建切向
            const double absX = std::abs(normal.x());
            const double absY = std::abs(normal.y());
            const double absZ = std::abs(normal.z());

            Eigen::Vector3d ref;
            if (absX <= absY && absX <= absZ) {
                ref = Eigen::Vector3d::UnitX();
            } else if (absY <= absX && absY <= absZ) {
                ref = Eigen::Vector3d::UnitY();
            } else {
                ref = Eigen::Vector3d::UnitZ();
            }

            tangent1 = normal.cross(ref).normalized();
            tangent2 = normal.cross(tangent1); // 已是单位向量（normal ⟂ tangent1 且均单位）
        }

        /**
         * @brief 对单个约束执行一次速度求解（法向恢复 + Coulomb 摩擦）。
         * @param bodyA 实体 A 的句柄。
         * @param bodyB 实体 B 的句柄。
         * @param constraint 当前约束引用（累积冲量状态在调用间更新）。
         * @return 如果实际施加了冲量（法向或切向）则返回 true。
         *
         * @details 对单个约束执行一次完整的法向 + 切向冲量求解：
         *          - 法向：增量求解，累积裁剪至 ≥ 0，仅恢复系数项
         *          - 切向：增量求解，累积裁剪至 Coulomb 摩擦锥内
         *          - 仅施加增量冲量（避免重复计数）
         *          - 穿透修正由独立的 positionSolve() 阶段处理
         */
        bool solveSingleConstraint(SolverBodyHandle &bodyA,
                                   SolverBodyHandle &bodyB,
                                   SolverContactConstraint &constraint)
        {
            // ---- 1. 预处理与有效性检查 ----

            const Eigen::Vector3d rawNormal = constraint.contactNormal;
            if (!rawNormal.allFinite() || rawNormal.squaredNorm() < 1e-12) {
                return false;
            }
            const Eigen::Vector3d normal = rawNormal.normalized();

            const Eigen::Vector3d velocityA = bodyA.entity->getVelocity();
            const Eigen::Vector3d angularA  = bodyA.entity->getAngularVelocity();
            const Eigen::Vector3d velocityB = bodyB.entity->getVelocity();
            const Eigen::Vector3d angularB  = bodyB.entity->getAngularVelocity();

            if (!velocityA.allFinite() || !velocityB.allFinite() ||
                !angularA.allFinite()  || !angularB.allFinite()) {
                return false;
            }

            const Eigen::Vector3d positionA = bodyA.entity->getPosition();
            const Eigen::Vector3d positionB = bodyB.entity->getPosition();
            const Eigen::Vector3d contactPos = constraint.contactPosition;

            const Eigen::Vector3d rA = contactPos - positionA;
            const Eigen::Vector3d rB = contactPos - positionB;

            const Eigen::Vector3d contactVelA = velocityA + angularA.cross(rA);
            const Eigen::Vector3d contactVelB = velocityB + angularB.cross(rB);

            // ---- 2. 构建切向方向 ----

            Eigen::Vector3d tangent1, tangent2;
            buildTangents(normal, tangent1, tangent2);

            // ---- 3. 计算各方向的有效质量 ----

            const Eigen::Vector3d rA_cross_n  = rA.cross(normal);
            const Eigen::Vector3d rB_cross_n  = rB.cross(normal);
            const Eigen::Vector3d rA_cross_t1 = rA.cross(tangent1);
            const Eigen::Vector3d rB_cross_t1 = rB.cross(tangent1);
            const Eigen::Vector3d rA_cross_t2 = rA.cross(tangent2);
            const Eigen::Vector3d rB_cross_t2 = rB.cross(tangent2);

            // 法向有效质量
            double effectiveMassNormal = bodyA.inverseMass + bodyB.inverseMass;
            effectiveMassNormal += rA_cross_n.dot(bodyA.inverseInertiaTensor * rA_cross_n);
            effectiveMassNormal += rB_cross_n.dot(bodyB.inverseInertiaTensor * rB_cross_n);

            // 切向 1 有效质量
            double effectiveMassTangent1 = bodyA.inverseMass + bodyB.inverseMass;
            effectiveMassTangent1 += rA_cross_t1.dot(bodyA.inverseInertiaTensor * rA_cross_t1);
            effectiveMassTangent1 += rB_cross_t1.dot(bodyB.inverseInertiaTensor * rB_cross_t1);

            // 切向 2 有效质量
            double effectiveMassTangent2 = bodyA.inverseMass + bodyB.inverseMass;
            effectiveMassTangent2 += rA_cross_t2.dot(bodyA.inverseInertiaTensor * rA_cross_t2);
            effectiveMassTangent2 += rB_cross_t2.dot(bodyB.inverseInertiaTensor * rB_cross_t2);

            if (!std::isfinite(effectiveMassNormal) || effectiveMassNormal <= 0.0) {
                return false;
            }

            // ---- 4. 法向冲量求解（增量 + 累积 + 裁剪 ≥ 0） ----
            //
            // 使用标准 Sequential Impulses 公式：
            //   λ_inc = -(1+e)·v_rel_n / M_eff    （仅在 v_rel_n < 0 时）
            //   λ_new = max(0, λ_old + λ_inc)
            //
            // 注意：此处只处理速度层面的恢复系数。
            // 穿透修正由独立的 positionSolve() 阶段通过 Baumgarte 位置修正处理，
            // 不在速度求解中引入速度偏置，避免多轮迭代中重复累加导致回弹发散。

            const double relativeNormalVelocity = (contactVelB - contactVelA).dot(normal);
            double normalDelta = 0.0;

            if (relativeNormalVelocity < 0.0) {
                normalDelta = -(1.0 + constraint.restitution) * relativeNormalVelocity / effectiveMassNormal;
            }

            const double oldNormalAccum = constraint.accumulatedNormalImpulse;
            double newNormalAccum = oldNormalAccum + normalDelta;
            if (newNormalAccum < 0.0) {
                newNormalAccum = 0.0;
            }
            normalDelta = newNormalAccum - oldNormalAccum;

            // ---- 5. 切向摩擦冲量求解（增量 + Coulomb 摩擦锥裁剪） ----

            const double relativeTangentVelocity1 = (contactVelB - contactVelA).dot(tangent1);
            const double relativeTangentVelocity2 = (contactVelB - contactVelA).dot(tangent2);

            const double frictionCoeff = constraint.frictionCoefficient;

            // 切向 1
            double tangentDelta1 = 0.0;
            if (std::isfinite(effectiveMassTangent1) && effectiveMassTangent1 > 0.0) {
                tangentDelta1 = -relativeTangentVelocity1 / effectiveMassTangent1;
            }
            const double oldTangentAccum1 = constraint.accumulatedTangentImpulse1;
            double newTangentAccum1 = oldTangentAccum1 + tangentDelta1;
            const double maxFriction1 = frictionCoeff * newNormalAccum;
            newTangentAccum1 = std::clamp(newTangentAccum1, -maxFriction1, maxFriction1);
            tangentDelta1 = newTangentAccum1 - oldTangentAccum1;

            // 切向 2
            double tangentDelta2 = 0.0;
            if (std::isfinite(effectiveMassTangent2) && effectiveMassTangent2 > 0.0) {
                tangentDelta2 = -relativeTangentVelocity2 / effectiveMassTangent2;
            }
            const double oldTangentAccum2 = constraint.accumulatedTangentImpulse2;
            double newTangentAccum2 = oldTangentAccum2 + tangentDelta2;
            const double maxFriction2 = frictionCoeff * newNormalAccum;
            newTangentAccum2 = std::clamp(newTangentAccum2, -maxFriction2, maxFriction2);
            tangentDelta2 = newTangentAccum2 - oldTangentAccum2;

            // 若法向冲量因裁剪减小，需重新裁剪切向冲量
            if (newNormalAccum < oldNormalAccum + 1e-12) {
                const double clampedMax1 = frictionCoeff * newNormalAccum;
                newTangentAccum1 = std::clamp(newTangentAccum1, -clampedMax1, clampedMax1);
                tangentDelta1 = newTangentAccum1 - oldTangentAccum1;

                const double clampedMax2 = frictionCoeff * newNormalAccum;
                newTangentAccum2 = std::clamp(newTangentAccum2, -clampedMax2, clampedMax2);
                tangentDelta2 = newTangentAccum2 - oldTangentAccum2;
            }

            // ---- 6. 写入累积冲量 ----

            constraint.accumulatedNormalImpulse    = newNormalAccum;
            constraint.accumulatedTangentImpulse1  = newTangentAccum1;
            constraint.accumulatedTangentImpulse2  = newTangentAccum2;

            // ---- 7. 施加增量冲量到实体速度 ----

            // 总增量冲量向量
            const Eigen::Vector3d impulseDelta = normalDelta * normal
                                               + tangentDelta1 * tangent1
                                               + tangentDelta2 * tangent2;

            if (impulseDelta.squaredNorm() < 1e-20) {
                // 无量级冲量，视为未实际作用
                return false;
            }

            // 作用到物体 A（反向，因为法线指向 A→B）
            if (bodyA.inverseMass > 0.0) {
                const Eigen::Vector3d nextSpeedA = velocityA - impulseDelta * bodyA.inverseMass;
                if (nextSpeedA.allFinite()) {
                    bodyA.entity->setSpeed(nextSpeedA);
                }

                const Eigen::Vector3d angularImpulseA = rA.cross(-impulseDelta);
                const Eigen::Vector3d nextAngularA = angularA + bodyA.inverseInertiaTensor * angularImpulseA;
                if (nextAngularA.allFinite()) {
                    bodyA.entity->setAngular(nextAngularA);
                }
            }

            // 作用到物体 B（正向）
            if (bodyB.inverseMass > 0.0) {
                const Eigen::Vector3d nextSpeedB = velocityB + impulseDelta * bodyB.inverseMass;
                if (nextSpeedB.allFinite()) {
                    bodyB.entity->setSpeed(nextSpeedB);
                }

                const Eigen::Vector3d angularImpulseB = rB.cross(impulseDelta);
                const Eigen::Vector3d nextAngularB = angularB + bodyB.inverseInertiaTensor * angularImpulseB;
                if (nextAngularB.allFinite()) {
                    bodyB.entity->setAngular(nextAngularB);
                }
            }

            return true;
        }
    } // anonymous namespace

    //  公开接口

    void FrictionalContactResolverStrategy::resolveContacts(
        SolverSceneSnapshotBase &scene,
        const std::span<const SolverContactConstraint> constraints,
        [[maybe_unused]] const double dt,
        const SolverConfig &config,
        SolverStats &stats) const
    {
        auto bodies = scene.getBodies();

        // 使用 const_cast 获取可变引用，以便在迭代间更新累积冲量字段。
        // 这是物理引擎中的常见模式（参见 Box2D b2ContactSolver），
        // 因为接口层面约束是"只读输入"，但求解器需要在迭代间维护中间状态。
        auto mutableConstraints = std::span<SolverContactConstraint>(
            const_cast<SolverContactConstraint *>(constraints.data()),
            constraints.size());

        // ---- 阶段 1：热启动 ----
        // 在速度求解之前，用上一帧保存的累积冲量预施加。
        // 这显著加快收敛速度，尤其是在物体处于静止堆叠状态时。
        warmStart(bodies, constraints);

        // ---- 阶段 2：速度求解 ----
        velocitySolve(bodies, mutableConstraints, config, stats);

        // ---- 阶段 3：位置求解 ----
        positionSolve(bodies, mutableConstraints, config, stats);
    }

    //  热启动

    void FrictionalContactResolverStrategy::warmStart(
        std::span<SolverBodyHandle> bodies,
        const std::span<const SolverContactConstraint> constraints) const
    {
        for (const SolverContactConstraint &constraint : constraints) {
            if (constraint.bodyAIndex >= bodies.size() || constraint.bodyBIndex >= bodies.size()) {
                continue;
            }

            SolverBodyHandle &bodyA = bodies[constraint.bodyAIndex];
            SolverBodyHandle &bodyB = bodies[constraint.bodyBIndex];

            if (bodyA.entity == nullptr || bodyB.entity == nullptr) {
                continue;
            }

            // 验证累积冲量有效性
            if (!std::isfinite(constraint.accumulatedNormalImpulse) ||
                !std::isfinite(constraint.accumulatedTangentImpulse1) ||
                !std::isfinite(constraint.accumulatedTangentImpulse2)) {
                continue;
            }

            const double totalImpulse = std::abs(constraint.accumulatedNormalImpulse)
                                      + std::abs(constraint.accumulatedTangentImpulse1)
                                      + std::abs(constraint.accumulatedTangentImpulse2);
            if (totalImpulse < 1e-15) {
                continue;
            }

            const Eigen::Vector3d rawNormal = constraint.contactNormal;
            if (!rawNormal.allFinite() || rawNormal.squaredNorm() < 1e-12) {
                continue;
            }
            const Eigen::Vector3d normal = rawNormal.normalized();

            Eigen::Vector3d tangent1, tangent2;
            buildTangents(normal, tangent1, tangent2);

            const Eigen::Vector3d impulse = constraint.accumulatedNormalImpulse * normal
                                          + constraint.accumulatedTangentImpulse1 * tangent1
                                          + constraint.accumulatedTangentImpulse2 * tangent2;

            const Eigen::Vector3d positionA = bodyA.entity->getPosition();
            const Eigen::Vector3d positionB = bodyB.entity->getPosition();
            const Eigen::Vector3d contactPos = constraint.contactPosition;
            const Eigen::Vector3d rA = contactPos - positionA;
            const Eigen::Vector3d rB = contactPos - positionB;

            if (bodyA.inverseMass > 0.0) {
                const Eigen::Vector3d nextSpeedA = bodyA.entity->getVelocity() - impulse * bodyA.inverseMass;
                if (nextSpeedA.allFinite()) {
                    bodyA.entity->setSpeed(nextSpeedA);
                }

                const Eigen::Vector3d angularImpulseA = rA.cross(-impulse);
                const Eigen::Vector3d nextAngularA = bodyA.entity->getAngularVelocity()
                    + bodyA.inverseInertiaTensor * angularImpulseA;
                if (nextAngularA.allFinite()) {
                    bodyA.entity->setAngular(nextAngularA);
                }
            }

            if (bodyB.inverseMass > 0.0) {
                const Eigen::Vector3d nextSpeedB = bodyB.entity->getVelocity() + impulse * bodyB.inverseMass;
                if (nextSpeedB.allFinite()) {
                    bodyB.entity->setSpeed(nextSpeedB);
                }

                const Eigen::Vector3d angularImpulseB = rB.cross(impulse);
                const Eigen::Vector3d nextAngularB = bodyB.entity->getAngularVelocity()
                    + bodyB.inverseInertiaTensor * angularImpulseB;
                if (nextAngularB.allFinite()) {
                    bodyB.entity->setAngular(nextAngularB);
                }
            }
        }
    }

    //  速度求解

    void FrictionalContactResolverStrategy::velocitySolve(
        std::span<SolverBodyHandle> bodies,
        const std::span<const SolverContactConstraint> constraints,
        const SolverConfig &config,
        SolverStats &stats) const
    {
        // const_cast：需要在迭代间更新约束对象的累积冲量字段
        auto mutableConstraints = std::span<SolverContactConstraint>(
            const_cast<SolverContactConstraint *>(constraints.data()),
            constraints.size());

        for (std::uint32_t velIter = 0; velIter < config.velocityIterations; ++velIter) {
            for (SolverContactConstraint &constraint : mutableConstraints) {
                if (constraint.bodyAIndex >= bodies.size() || constraint.bodyBIndex >= bodies.size()) {
                    continue;
                }

                SolverBodyHandle &bodyA = bodies[constraint.bodyAIndex];
                SolverBodyHandle &bodyB = bodies[constraint.bodyBIndex];

                if (bodyA.entity == nullptr || bodyB.entity == nullptr) {
                    continue;
                }

                const bool applied = solveSingleConstraint(bodyA, bodyB, constraint);

                if (applied) {
                    ++stats.resolvedContactCount;
                }
            }
        }
    }

    //  位置求解

    void FrictionalContactResolverStrategy::positionSolve(
        std::span<SolverBodyHandle> bodies,
        const std::span<const SolverContactConstraint> constraints,
        const SolverConfig &config,
        SolverStats &stats) const
    {
        for (std::uint32_t posIter = 0; posIter < config.positionIterations; ++posIter) {
            for (const SolverContactConstraint &constraint : constraints) {
                if (constraint.bodyAIndex >= bodies.size() || constraint.bodyBIndex >= bodies.size()) {
                    continue;
                }

                SolverBodyHandle &bodyA = bodies[constraint.bodyAIndex];
                SolverBodyHandle &bodyB = bodies[constraint.bodyBIndex];

                if (bodyA.entity == nullptr || bodyB.entity == nullptr) {
                    continue;
                }

                // ---- 法向穿透修正（Baumgarte 稳定化） ----
                // 注意：位置求解仅处理穿透修正。
                // 摩擦效果在速度求解阶段已通过 Coulomb 摩擦锥完整处理，
                // 位置层面的切向漂移抑制需依赖接触点持久化数据（后续版本补充）。

                Eigen::Vector3d rawNormal = constraint.contactNormal;
                if (!rawNormal.allFinite() || rawNormal.squaredNorm() < 1e-12) {
                    continue;
                }
                const Eigen::Vector3d normal = rawNormal.normalized();

                const double invMassSum = bodyA.inverseMass + bodyB.inverseMass;
                if (!std::isfinite(invMassSum) || invMassSum <= 0.0) {
                    continue;
                }

                const double penetration = std::max(constraint.penetration - config.penetrationSlop, 0.0);
                if (!std::isfinite(penetration)) {
                    continue;
                }

                if (penetration > 0.0) {
                    const double correctionMagnitude = penetration * config.positionCorrectionBeta / invMassSum;
                    if (!std::isfinite(correctionMagnitude)) {
                        continue;
                    }
                    const Eigen::Vector3d correction = correctionMagnitude * normal;

                    if (bodyA.inverseMass > 0.0) {
                        // 每个实体实际位移单独钳制，防止退化接触造成位置突变
                        Eigen::Vector3d correctionA = correction * bodyA.inverseMass;
                        const double correctionANorm = correctionA.norm();
                        if (correctionANorm > config.maxPositionCorrectionPerStep && correctionANorm > 1e-12) {
                            correctionA *= config.maxPositionCorrectionPerStep / correctionANorm;
                        }
                        const Eigen::Vector3d correctedPos = bodyA.entity->getPosition()
                            - correctionA;
                        if (correctedPos.allFinite()) {
                            bodyA.entity->setPosition(correctedPos);
                        }
                    }

                    if (bodyB.inverseMass > 0.0) {
                        Eigen::Vector3d correctionB = correction * bodyB.inverseMass;
                        const double correctionBNorm = correctionB.norm();
                        if (correctionBNorm > config.maxPositionCorrectionPerStep && correctionBNorm > 1e-12) {
                            correctionB *= config.maxPositionCorrectionPerStep / correctionBNorm;
                        }
                        const Eigen::Vector3d correctedPos = bodyB.entity->getPosition()
                            + correctionB;
                        if (correctedPos.allFinite()) {
                            bodyB.entity->setPosition(correctedPos);
                        }
                    }
                }

                ++stats.resolvedContactCount;
            }
        }
    }

} // namespace egret
