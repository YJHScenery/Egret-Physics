/**
 * @file        frictionless_contact_resolver.h
 * @brief       无摩擦接触求解器头文件
 * @details     定义FrictionlessContactResolver类，提供无摩擦的接触求解。
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
#ifndef EGRET_PHYSICS_FRICTIONLESS_CONTACT_RESOLVER_H
#define EGRET_PHYSICS_FRICTIONLESS_CONTACT_RESOLVER_H

#include "contact_resolver_strategy.h"

namespace egret
{
    /**
     * @brief       无摩擦法向冲量解算器类。
     * @details     FrictionlessContactResolverStrategy 继承自 ContactResolverStrategy，
     *              实现无摩擦法向冲量与位置修正的接触解算算法。
     *              该实现只处理法向冲量与基础位置修正，不处理切向摩擦，
     *              适合第一版的稳定联调与可视化验证。
     *              使用序列冲量（Sequential Impulses）方法。
     *
     * @invariant   resolveContacts() 方法只处理法向冲量
     * @remark      FrictionlessContactResolverStrategy 是具体实现类，可以直接实例化
     * @see         ContactResolverStrategy, SolverContactConstraint, SolverConfig
     */
    class FrictionlessContactResolverStrategy final : public ContactResolverStrategy
    {
    public:
        /** 默认构造。 */
        FrictionlessContactResolverStrategy() = default;

        /** 默认析构。 */
        ~FrictionlessContactResolverStrategy() override = default;

        /**
         * @brief 对接触约束执行无摩擦法向冲量与位置修正。
         * @param scene 可变场景快照。
         * @param constraints 只读接触约束集合。
         * @param dt 固定步长。
         * @param config 求解器配置。
         * @param stats 每步统计信息。
         */
        void resolveContacts(SolverSceneSnapshotBase &scene,
                             std::span<const SolverContactConstraint> constraints,
                             double dt,
                             const SolverConfig &config,
                             SolverStats &stats) const override;
    };
}

#endif // EGRET_PHYSICS_FRICTIONLESS_CONTACT_RESOLVER_H
