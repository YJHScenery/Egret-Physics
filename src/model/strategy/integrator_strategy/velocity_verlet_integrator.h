/**
 * @file        velocity_verlet_integrator.h
 * @brief       Velocity Verlet 积分器头文件
 * @details     定义 VelocityVerletIntegratorStrategy 类，提供 Velocity Verlet 积分。
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
#ifndef EGRET_PHYSICS_VELOCITY_VERLET_INTEGRATOR_H
#define EGRET_PHYSICS_VELOCITY_VERLET_INTEGRATOR_H

#include "integrator_strategy.h"

namespace egret
{
    /**
     * @brief       Velocity Verlet 积分策略类。
     * @details     VelocityVerletIntegratorStrategy 继承自 IntegratorStrategy，
     *              实现 Velocity Verlet 积分算法，是积分公式的唯一所有者。
     *
     *              ## 算法流程
     *
     *              ### Phase 0 — 保存旧状态
     *              a(t) = F(t)/m,  v(t),  α(t) = I⁻¹·τ(t),  ω(t)
     *
     *              ### Phase 1 — 位置全步 + 转动半步
     *              平动：x(t+dt) = x + v·dt + 0.5·a·dt²
     *              转动：ω_half = ω + 0.5·α·dt → 用 ω_half 积分四元数姿态 q(t+dt)
     *
     *              ### Phase 2 — 在新位置/新姿态重算力与力矩
     *              清空力/力矩 → 遍历 fields → 遍历 constraints
     *              → a(t+dt) = F(t+dt)/m,  τ(t+dt)
     *
     *              ### Phase 3 — 速度用平均加速度
     *              平动：v(t+dt) = v + 0.5·(a(t) + a(t+dt))·dt
     *              转动：ω(t+dt) = ω + 0.5·(α(t) + α(t+dt))·dt
     *
     *              ## 优势
     *              - 二阶精度 O(h²)，高于半隐式 Euler 的一阶 O(h)
     *              - 辛结构，长期能量守恒良好
     *              - 时间可逆（在数值误差范围内）
     *              - 仅需一次额外的力计算（非 RK4 的四次）
     *
     *              ## 转动
     *              当前力矩来源为 m_torques（用户 upsert），不依赖朝向，α(t+dt) = α(t)。
     *              但算法结构已为朝向相关力矩（如空气阻力矩）预留：Phase 1 先更新姿态，
     *              Phase 2 在新姿态下重算力矩，Phase 3 用平均角加速度更新角速度。
     *
     *              ## 跳过条件
     *              enableIntegration == false、inverseMass <= 0、isFixed() == true。
     *
     *              ## 中步力重算
     *              位置更新后，策略内部清空力/力矩累加器并重新遍历 fields + constraints。
     *              此行为独立于 SolverConfig 的 clearForcesBeforeFieldApply 等配置项，
     *              因为中步重算是算法正确性的内在要求。
     *
     * @invariant   integrate() 方法先更新位置再更新速度（Verlet 顺序）
     * @invariant   中步力重算后 enforce 力与当前实体位置一致
     * @remark      VelocityVerletIntegratorStrategy 是具体实现类，可以直接实例化
     * @see         IntegratorStrategy, SemiImplicitEulerIntegratorStrategy, PhysicalEntity
     */
    class VelocityVerletIntegratorStrategy final : public IntegratorStrategy
    {
    public:
        /** 默认构造。 */
        VelocityVerletIntegratorStrategy() = default;

        /** 默认析构。 */
        ~VelocityVerletIntegratorStrategy() override = default;

        /**
         * @brief 按 Velocity Verlet 推进所有可积分实体。
         * @param scene 可变场景快照，提供实体、场、约束的访问。
         * @param dt 固定步长，单位为秒。
         * @param config 求解器配置（中步力重算不受其 clearForces 等标志影响）。
         * @param stats 每步统计信息。
         */
        void integrate(SolverSceneSnapshotBase &scene,
                       double dt,
                       [[maybe_unused]] const SolverConfig &config,
                       SolverStats &stats) const override;

        std::string_view name() const override;
    };
}

#endif // EGRET_PHYSICS_VELOCITY_VERLET_INTEGRATOR_H
