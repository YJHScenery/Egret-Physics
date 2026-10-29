/**
 * @file        velocity_verlet_integrator.cpp
 * @brief       Velocity Verlet 积分器实现文件
 * @details     实现 VelocityVerletIntegratorStrategy 的 Velocity Verlet 积分算法，
 *              包含三步：保存旧加速度 → 位置全步 → 中步力重算 → 速度平均。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-06
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Strategy
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "velocity_verlet_integrator.h"

#include <vector>

#include "constraints_base.h"
#include "field_base.h"
#include "physical_entity.h"

namespace egret
{
    void VelocityVerletIntegratorStrategy::integrate(SolverSceneSnapshotBase& scene,
                                                     const double dt,
                                                     [[maybe_unused]] const SolverConfig& config,
                                                     SolverStats& stats) const
    {
        auto bodies = scene.getBodies();
        const std::size_t bodyCount = bodies.size();


        // Phase 0: 收集活跃实体，保存 a(t) 和 v(t)

        struct BodyState
        {
            SolverBodyHandle* handle;
            Eigen::Vector3d aOld;
            Eigen::Vector3d vOld;
            // 转动（仅 hasRotation() 实体）
            Eigen::Vector3d alphaOld = Eigen::Vector3d::Zero();
            Eigen::Vector3d omegaOld = Eigen::Vector3d::Zero();
        };

        std::vector<BodyState> states;
        states.reserve(bodyCount);

        for (SolverBodyHandle& body : bodies) {
            if (body.entity == nullptr || !body.enableIntegration) {
                continue;
            }

            if (body.inverseMass <= 0.0) {
                continue;
            }

            PhysicalEntity* const entity = body.entity;

            if (entity->isFixed()) {
                continue;
            }

            // 休眠实体跳过积分，保持冻结状态
            if (entity->isAsleep()) {
                continue;
            }

            BodyState s{};
            s.handle = &body;
            s.aOld = entity->getJoinForce() / entity->getMass();
            s.vOld = entity->getVelocity();

            if (entity->hasRotation()) {
                s.alphaOld = body.inverseInertiaTensor * entity->getTorque();
                s.omegaOld = entity->getAngularVelocity();
            }

            states.push_back(s);
        }

        if (states.empty()) {
            std::ignore = stats.resolvedContactCount;
            return;
        }


        // Phase 1: 位置全步 + 转动半步
        //   平动：x(t+dt) = x + v·dt + 0.5·a·dt²
        //   转动：ω_half = ω + 0.5·α·dt → 用 ω_half 积分四元数姿态
        //   转动半步放在力重算之前，确保当未来引入朝向相关力矩时
        //   重算的 τ(t+dt) 基于新姿态。

        for (const BodyState& s : states) {
            PhysicalEntity* const entity = s.handle->entity;

            // 平动
            const Eigen::Vector3d x = entity->getPosition();
            entity->setPosition(x + s.vOld * dt + 0.5 * s.aOld * dt * dt);

            // 转动：半步角速度 → 四元数姿态积分
            if (entity->hasRotation()) {
                const Eigen::Vector3d omegaHalf = s.omegaOld + 0.5 * s.alphaOld * dt;
                entity->setAngular(omegaHalf);

                if (omegaHalf.norm() >= 1e-10) {
                    Eigen::Quaterniond q = entity->getTransformCR().getRotation();
                    const Eigen::Quaterniond omegaQuat(0, omegaHalf.x(), omegaHalf.y(), omegaHalf.z());

                    Eigen::Quaterniond dq = omegaQuat * q;
                    dq.coeffs() *= 0.5;
                    q.coeffs() += dq.coeffs() * dt;
                    q.normalize();

                    entity->setRotation(q);
                }
            }
        }


        // Phase 2: 在新位置重算力 → a(t+dt)
        //   ① 清空所有活跃实体的力/力矩累加器
        //   ② 遍历所有场，重新施加场力
        //   ③ 遍历所有约束，重新施加约束力

        for (const BodyState& s : states) {
            PhysicalEntity* const entity = s.handle->entity;

            // 清空力（removeForce(0) 循环是当前实体层的唯一逐个删除方式）
            const std::size_t forceCount = entity->getForcesCR().size();
            for (std::size_t i = 0; i < forceCount; ++i) {
                entity->removeForce(0);
            }

            // 清空力矩
            entity->clearTorques();
        }

        // 重算场力
        {
            const auto fields = scene.getFields();
            for (FieldBase* field : fields) {
                if (field == nullptr) {
                    continue;
                }
                for (const BodyState& s : states) {
                    field->applyToEntity(s.handle->entity);
                }
            }
        }

        // 重算约束力
        {
            const auto constraints = scene.getConstraints();
            for (ConstraintsBase* constraint : constraints) {
                if (constraint == nullptr || !constraint->isEnabled()) {
                    continue;
                }
                constraint->applyConstraintForces();
            }
            for (ConstraintsBase* constraint : constraints) {
                if (constraint && constraint->isEnabled()) constraint->applyFrictionForces(dt);
            }
        }


        // Phase 3: 速度用平均加速度（平动 + 转动）
        //   平动：v(t+dt) = v + 0.5·(a_old + a_new)·dt
        //   转动：ω(t+dt) = ω + 0.5·(α_old + α_new)·dt
        //   注意 Phase 1 已将角速度设为 ω_half，这里从 α_old 和 α_new
        //   算出最终 ω，无需保留 ω_half。

        for (const BodyState& s : states) {
            PhysicalEntity* const entity = s.handle->entity;

            // --- 平动 ---
            const Eigen::Vector3d aNew = entity->getJoinForce() / entity->getMass();
            const Eigen::Vector3d vNew = s.vOld + 0.5 * (s.aOld + aNew) * dt;
            entity->setSpeed(vNew);

            // --- 转动 ---
            if (entity->hasRotation()) {
                // α_new = I⁻¹ · τ_new（Phase 2 已重算力矩）
                const Eigen::Vector3d alphaNew = s.handle->inverseInertiaTensor * entity->getTorque();
                // ω(t+dt) = ω + 0.5·(α_old + α_new)·dt
                const Eigen::Vector3d omegaNew = s.omegaOld + 0.5 * (s.alphaOld + alphaNew) * dt;
                entity->setAngular(omegaNew);
            }
        }

        std::ignore = stats.resolvedContactCount;
    }

    std::string_view VelocityVerletIntegratorStrategy::name() const
    {
        const static std::string name{"Velocity Verlet"};
        return name;
    }
}
