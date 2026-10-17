/**
 * @file        frictional_contact_resolver.cpp
 * @brief       含摩擦接触求解器实现文件
 * @details     实现 FrictionalContactResolverStrategy 的各项成员函数：
 *              整体法向速度求解、Coulomb 摩擦与整岛同步位姿投影。
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
#include "contact_graph_projector.h"
#include <algorithm>
#include <cmath>
#include <Eigen/Sparse>

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
         * @brief 同时求解所有法向速度约束，避免面流形各点的恢复产生交替力矩。
         * @details 使用稀疏雅可比的矩阵自由对偶迭代，保留共享刚体的全部耦合。
         */
        void solveNormalGraph(std::span<SolverBodyHandle> bodies,
                              std::span<SolverContactConstraint> constraints,
                              const SolverConfig& config)
        {
            const Eigen::Index count = static_cast<Eigen::Index>(constraints.size());
            const Eigen::Index dofs = static_cast<Eigen::Index>(6*bodies.size());
            if (count==0) return;
            std::vector<Eigen::Triplet<double>> jEntries, wEntries;
            Eigen::VectorXd velocity=Eigen::VectorXd::Zero(dofs), target=Eigen::VectorXd::Zero(count);
            for (std::size_t i=0;i<bodies.size();++i) {
                const auto& body=bodies[i]; if (!body.entity) continue;
                velocity.segment<3>(6*i)=body.entity->getVelocity();
                velocity.segment<3>(6*i+3)=body.entity->getAngularVelocity();
                for (int k=0;k<3;++k) {
                    wEntries.emplace_back(6*i+k,6*i+k,body.inverseMass);
                    if (body.entity->hasRotation() && body.inverseMass>0)
                        for (int j=0;j<3;++j) wEntries.emplace_back(6*i+3+k,6*i+3+j,body.inverseInertiaTensor(k,j));
                }
            }
            for (Eigen::Index i=0;i<count;++i) {
                const auto& c=constraints[i];
                if (c.bodyAIndex>=bodies.size() || c.bodyBIndex>=bodies.size() ||
                    !c.contactNormal.allFinite() || c.contactNormal.norm()<1e-12) continue;
                const Eigen::Vector3d n=c.contactNormal.normalized();
                target[i]=c.targetNormalVelocity;
                for (auto index : {c.bodyAIndex,c.bodyBIndex}) {
                    if (!bodies[index].entity) continue;
                    const double sign=index==c.bodyAIndex ? -1.0:1.0;
                    const Eigen::Vector3d arm=c.contactPosition-bodies[index].entity->getPosition();
                    const Eigen::Vector3d angular=sign*arm.cross(n);
                    for (int k=0;k<3;++k) {
                        jEntries.emplace_back(i,6*index+k,sign*n[k]);
                        jEntries.emplace_back(i,6*index+3+k,angular[k]);
                    }
                }
            }
            Eigen::SparseMatrix<double> J(count,dofs), W(dofs,dofs);
            J.setFromTriplets(jEntries.begin(),jEntries.end()); W.setFromTriplets(wEntries.begin(),wEntries.end());
            const Eigen::SparseMatrix<double> absJ=J.cwiseAbs(), absW=W.cwiseAbs();
            const double bound=(absJ*(absW*(absJ.transpose()*Eigen::VectorXd::Ones(count)))).maxCoeff();
            if (!std::isfinite(bound) || bound<=1e-15) return;
            const Eigen::VectorXd rhs=target-J*velocity;
            Eigen::VectorXd lambda=Eigen::VectorXd::Zero(count);
            for (std::uint32_t iteration=0;iteration<config.graphIterations;++iteration) {
                const Eigen::VectorXd residual=rhs-J*(W*(J.transpose()*lambda));
                const Eigen::VectorXd next=(lambda+residual/bound).cwiseMax(0.0);
                const double error=bound*(next-lambda).lpNorm<Eigen::Infinity>();
                lambda=next;
                if (error<1e-9) break;
            }
            const Eigen::VectorXd delta=W*(J.transpose()*lambda);
            if (!delta.allFinite()) return;
            for (std::size_t i=0;i<bodies.size();++i) {
                if (!bodies[i].entity || bodies[i].inverseMass<=0) continue;
                bodies[i].entity->setSpeed(velocity.segment<3>(6*i)+delta.segment<3>(6*i));
                bodies[i].entity->setAngular(velocity.segment<3>(6*i+3)+delta.segment<3>(6*i+3));
            }
            for (Eigen::Index i=0;i<count;++i) constraints[i].accumulatedNormalImpulse+=lambda[i];
        }

        /**
         * @brief 对单个约束执行一次速度求解（法向恢复 + Coulomb 摩擦）。
         * @param bodyA 实体 A 的句柄。
         * @param bodyB 实体 B 的句柄。
         * @param constraint 当前约束引用（累积冲量状态在调用间更新）。
         * @return 如果实际施加了冲量（法向或切向）则返回 true。
         *
         * @details 对单个约束执行一次完整的法向 + 切向冲量求解：
         *          - 法向：固定恢复目标下的有符号增量，累积裁剪至 ≥ 0
         *          - 切向：增量求解，累积裁剪至 Coulomb 摩擦锥内
         *          - 仅施加增量冲量（避免重复计数）
         *          - 穿透修正由独立的整岛位置阶段处理
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
            // 恢复目标在迭代前固定；允许负增量撤回过量冲量。
            // 穿透由整岛位置投影处理，不以速度偏置重复注入能量。
            const double relativeNormalVelocity = (contactVelB - contactVelA).dot(normal);
            double normalDelta = (constraint.targetNormalVelocity - relativeNormalVelocity) / effectiveMassNormal;

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

            // 法向增量先计入接触点相对速度，切向使用完整二维有效质量。
            const Eigen::Vector3d normalImpulse = normalDelta * normal;
            const Eigen::Vector3d normalVelocityChange =
                normalImpulse * (bodyA.inverseMass + bodyB.inverseMass)
                + (bodyA.inverseInertiaTensor * rA.cross(normalImpulse)).cross(rA)
                + (bodyB.inverseInertiaTensor * rB.cross(normalImpulse)).cross(rB);
            Eigen::Vector2d tangentVelocity(
                relativeTangentVelocity1 + normalVelocityChange.dot(tangent1),
                relativeTangentVelocity2 + normalVelocityChange.dot(tangent2));
            Eigen::Matrix2d tangentMass;
            const double offDiagonal = rA_cross_t1.dot(bodyA.inverseInertiaTensor*rA_cross_t2)
                                     + rB_cross_t1.dot(bodyB.inverseInertiaTensor*rB_cross_t2);
            tangentMass << effectiveMassTangent1, offDiagonal, offDiagonal, effectiveMassTangent2;
            const double oldTangentAccum1 = constraint.accumulatedTangentImpulse1;
            const double oldTangentAccum2 = constraint.accumulatedTangentImpulse2;
            Eigen::Vector2d tangentAccum(oldTangentAccum1, oldTangentAccum2);
            if (tangentMass.determinant() > 1e-20)
                tangentAccum -= tangentMass.ldlt().solve(tangentVelocity);
            const double frictionLimit = std::max(0.0, frictionCoeff) * newNormalAccum;
            if (tangentAccum.norm() > frictionLimit && tangentAccum.norm() > 0.0)
                tangentAccum *= frictionLimit / tangentAccum.norm();
            const double newTangentAccum1 = tangentAccum.x();
            const double newTangentAccum2 = tangentAccum.y();
            const double tangentDelta1 = newTangentAccum1 - oldTangentAccum1;
            const double tangentDelta2 = newTangentAccum2 - oldTangentAccum2;

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

        // 中间状态放在本次求解拥有的缓冲区，不修改只读输入。
        std::vector<SolverContactConstraint> working(constraints.begin(), constraints.end());
        auto mutableConstraints = std::span<SolverContactConstraint>(working);
        for (auto& c : working) {
            if (c.bodyAIndex >= bodies.size() || c.bodyBIndex >= bodies.size()) continue;
            const auto& a = bodies[c.bodyAIndex]; const auto& b = bodies[c.bodyBIndex];
            if (!a.entity || !b.entity || !c.contactNormal.allFinite() || c.contactNormal.norm() < 1e-12) continue;
            const Eigen::Vector3d va = a.entity->getVelocity() + a.entity->getAngularVelocity().cross(c.contactPosition-a.entity->getPosition());
            const Eigen::Vector3d vb = b.entity->getVelocity() + b.entity->getAngularVelocity().cross(c.contactPosition-b.entity->getPosition());
            const double vn = (vb-va).dot(c.contactNormal.normalized());
            c.targetNormalVelocity = vn < -config.restitutionVelocityThreshold ? -c.restitution * vn : 0.0;
        }

        // ---- 阶段 1：热启动 ----
        // 若调用方提供初始冲量，则先应用；本实现不维护跨帧接触缓存。
        warmStart(bodies, mutableConstraints);

        // ---- 阶段 2：先整体求解法向冲击，再迭代耦合法向与摩擦 ----
        solveNormalGraph(bodies, mutableConstraints, config);
        velocitySolve(bodies, mutableConstraints, config, stats);

        // ---- 阶段 3：位置求解 ----
        ContactGraphProjector::project(bodies, mutableConstraints, config, stats, scene.getConstraints());
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
        const std::span<SolverContactConstraint> constraints,
        const SolverConfig &config,
        SolverStats &stats) const
    {
        auto mutableConstraints = constraints;

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
        ContactGraphProjector::project(bodies, constraints, config, stats);
    }

} // namespace egret
