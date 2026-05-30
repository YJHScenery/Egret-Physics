/**
 * @file        integrator_strategy.h
 * @brief       积分器策略头文件
 * @details     定义IntegratorStrategy类，作为积分器策略的抽象接口。
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

#ifndef EGRET_PHYSICS_INTEGRATOR_STRATEGY_H
#define EGRET_PHYSICS_INTEGRATOR_STRATEGY_H

#include "solver_scene_snapshot.h"

namespace egret
{
    /**
     * @brief       时间积分阶段的策略接口。
     * @details     IntegratorStrategy 是时间积分阶段的策略接口。
     *              第一版推荐使用半隐式 Euler，因为它在稳定性和实现复杂度之间更均衡。
     *              未来可扩展为 RK4 或兼容 XPBD 的变体。
     *              采用策略模式，派生类实现具体的积分逻辑。
     *
     * @invariant   integrate() 方法在每次调用后更新实体状态
     * @remark      IntegratorStrategy 是抽象接口，不能直接实例化
     * @see         SemiImplicitEulerIntegratorStrategy, SolverSceneSnapshotBase, SolverConfig
     */
    class IntegratorStrategy
    {
    public:
        /** 策略多态所需的虚析构函数。 */
        virtual ~IntegratorStrategy() = default;

        /**
         * @brief 按固定 dt 对实体状态做一次积分。
         * @param scene 可变场景快照。
         * @param dt 固定步长，单位为秒。
         * @param config 运行时求解器选项。
         * @param stats 可变的每步统计信息。
         */
        virtual void integrate(SolverSceneSnapshotBase &scene,
                               double dt,
                               const SolverConfig &config,
                               SolverStats &stats) const = 0;
    };
}

#endif // EGRET_PHYSICS_INTEGRATOR_STRATEGY_H
