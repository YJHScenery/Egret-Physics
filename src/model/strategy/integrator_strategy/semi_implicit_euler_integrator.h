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
     *              实现半隐式 Euler 积分算法。
     *              该策略优先更新速度，再更新位置，适合第一版稳定推进。
     *              半隐式 Euler 比显式 Euler 更稳定，适合物理模拟。
     *
     * @invariant   integrate() 方法先更新速度再更新位置
     * @remark      SemiImplicitEulerIntegratorStrategy 是具体实现类，可以直接实例化
     * @see         IntegratorStrategy, SolverSceneSnapshotBase, SolverConfig
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
                       const SolverConfig &config,
                       SolverStats &stats) const override;
    };
}

#endif // EGRET_PHYSICS_SEMI_IMPLICIT_EULER_INTEGRATOR_H
