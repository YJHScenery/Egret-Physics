/**
 * @file        contact_resolver_strategy.h
 * @brief       接触求解器策略头文件
 * @details     定义ContactResolverStrategy类，作为接触求解器策略的抽象接口。
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

#ifndef EGRET_PHYSICS_CONTACT_RESOLVER_STRATEGY_H
#define EGRET_PHYSICS_CONTACT_RESOLVER_STRATEGY_H

#include <span>

#include "solver_scene_snapshot.h"

namespace egret
{
    /**
     * @brief       接触冲量与位置修正的策略接口。
     * @details     ContactResolverStrategy 是接触冲量与位置修正的策略接口。
     *              第一版聚焦于无摩擦法向冲量 + 穿透修正。
     *              切向摩擦可以在后续版本中加入。
     *              采用策略模式，派生类实现具体的接触解算逻辑。
     *
     * @invariant   resolveContacts() 方法在每次调用后更新实体速度和位置
     * @remark      ContactResolverStrategy 是抽象接口，不能直接实例化
     * @see         FrictionlessContactResolverStrategy, SolverContactConstraint, SolverConfig
     */
    class ContactResolverStrategy
    {
    public:
        /** 策略多态所需的虚析构函数。 */
        virtual ~ContactResolverStrategy() = default;

        /**
         * @brief 对一次固定步长中的接触约束进行解算。
         * @param scene 可变场景快照。
         * @param constraints 只读接触约束集合。
         * @param dt 固定步长，单位为秒。
         * @param config 运行时求解器选项。
         * @param stats 可变的每步统计信息。
         */
        virtual void resolveContacts(SolverSceneSnapshotBase &scene,
                                     std::span<const SolverContactConstraint> constraints,
                                     double dt,
                                     const SolverConfig &config,
                                     SolverStats &stats) const = 0;
    };
}

#endif // EGRET_PHYSICS_CONTACT_RESOLVER_STRATEGY_H
