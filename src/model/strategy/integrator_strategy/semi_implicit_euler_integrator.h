/**
 * @file        semi_implicit_euler_integrator.h
 * @brief       半隐式欧拉积分器头文件
 * @details     定义SemiImplicitEulerIntegrator类，提供半隐式欧拉积分。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-26
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Strategy
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#ifndef EGRET_PHYSICS_SEMI_IMPLICIT_EULER_INTEGRATOR_H
#define EGRET_PHYSICS_SEMI_IMPLICIT_EULER_INTEGRATOR_H

#include <algorithm>

#include "integrator_strategy.h"

namespace egret
{
    /**
     * @brief       半隐式 Euler 积分策略类。
     * @details     SemiImplicitEulerIntegratorStrategy 继承自 IntegratorStrategy，
     *              实现半隐式 Euler 积分算法，是积分公式的唯一所有者。
     *
     *              ## 平动（所有实体）
     *              a = F/m → v += a·dt → x += v·dt
     *              使用新速度更新位置是半隐式 Euler 的核心特征。
     *
     *              ## 转动（仅 hasRotation() == true 的实体，如 RigidBody）
     *              α = I⁻¹·τ → ω += α·dt → 四元数姿态积分
     *              逆惯量矩阵 I⁻¹ 由 Solver::updateBodyInertiaCache 预计算，
     *              存储在 SolverBodyHandle::inverseInertiaTensor 中。
     *
     *              跳过条件：enableIntegration == false、inverseMass <= 0、isFixed() == true。
     *
     * @invariant   integrate() 方法先更新速度再更新位置（半隐式顺序）
     * @invariant   转动积分仅对 hasRotation() 为 true 的实体执行
     * @remark      SemiImplicitEulerIntegratorStrategy 是具体实现类，可以直接实例化
     * @see         IntegratorStrategy, PhysicalEntity, RigidBody, SolverSceneSnapshotBase
     */
    class SemiImplicitEulerIntegratorStrategy final : public IntegratorStrategy
    {
    public:
        /** 默认构造。 */
        SemiImplicitEulerIntegratorStrategy() = default;

        /** 默认析构。 */
        ~SemiImplicitEulerIntegratorStrategy() override = default;

        /**
         * @brief 按半隐式 Euler 推进所有可积分实体。
         * @param scene 可变场景快照。
         * @param dt 固定步长。
         * @param config 求解器配置。
         * @param stats 每步统计信息。
         */
        void integrate(SolverSceneSnapshotBase &scene,
                       double dt,
                       [[maybe_unused]] const SolverConfig &config,
                       SolverStats &stats) const override;

        std::string_view name() const override;
    };
}

#endif // EGRET_PHYSICS_SEMI_IMPLICIT_EULER_INTEGRATOR_H
