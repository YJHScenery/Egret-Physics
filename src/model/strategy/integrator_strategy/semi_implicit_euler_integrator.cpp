/**
* @file        semi_implicit_euler_integrator.cpp
 * @brief       半隐式欧拉积分器实现文件
 * @details     实现 SemiImplicitEulerIntegratorStrategy 的半隐式欧拉积分算法，
 *              包含平动（v+=a·dt, x+=v·dt）和转动（ω+=α·dt, 四元数积分）两部分。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-04
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Strategy
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "physical_entity.h"
#include "semi_implicit_euler_integrator.h"

namespace egret
{
    void SemiImplicitEulerIntegratorStrategy::integrate(SolverSceneSnapshotBase& scene,
                                                        const double dt,
                                                        [[maybe_unused]] const SolverConfig& config,
                                                        SolverStats& stats) const
    {
        auto bodies = scene.getBodies();

        for (SolverBodyHandle& body : bodies) {
            if (body.entity == nullptr || !body.enableIntegration) {
                continue;
            }

            if (body.inverseMass <= 0.0) {
                continue;
            }

            PhysicalEntity* const entity = body.entity;

            // 固定实体（如 GravitationalField 的 m_fixed=true）不参与积分
            if (entity->isFixed()) {
                continue;
            }

            // === 平动：半隐式欧拉 ===
            // a = F / m
            const Eigen::Vector3d acceleration = entity->getJoinForce() / entity->getMass();
            // v_new = v + a * dt
            Eigen::Vector3d velocity = entity->getVelocity();
            velocity += acceleration * dt;
            entity->setSpeed(velocity);
            // x_new = x + v_new * dt  （使用新速度，半隐式欧拉的特征）
            entity->setPosition(entity->getPosition() + velocity * dt);

            // === 转动：半隐式欧拉（仅刚体） ===
            if (entity->hasRotation()) {
                // α = I⁻¹ · τ（使用求解器预计算的逆惯量矩阵）
                const Eigen::Vector3d angularAccel = body.inverseInertiaTensor * entity->getTorque();
                // ω_new = ω + α * dt
                Eigen::Vector3d angularVel = entity->getAngularVelocity();
                angularVel += angularAccel * dt;
                entity->setAngular(angularVel);

                // 四元数微分：dq/dt = 0.5 * ω_q * q
                if (angularVel.norm() >= 1e-10) {
                    Eigen::Quaterniond currentRot = entity->getTransformCR().getRotation();
                    const Eigen::Quaterniond omegaQuat(0, angularVel.x(), angularVel.y(), angularVel.z());

                    Eigen::Quaterniond deltaQ = omegaQuat * currentRot;
                    deltaQ.coeffs() *= 0.5;
                    currentRot.coeffs() += deltaQ.coeffs() * dt;
                    currentRot.normalize();

                    entity->setRotation(currentRot);
                }
            }
        }

        std::ignore = stats.resolvedContactCount;
    }

    std::string_view SemiImplicitEulerIntegratorStrategy::name() const
    {
        static const std::string name {"Semi Implicit Euler"};
        return name;
    }
}
