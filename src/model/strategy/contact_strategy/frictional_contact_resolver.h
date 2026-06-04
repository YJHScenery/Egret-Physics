/**
 * @file        frictional_contact_resolver.h
 * @brief       含摩擦接触求解器头文件
 * @details     定义FrictionalContactResolverStrategy类，实现带 Coulomb 摩擦、
 *              冲量累积与热启动的序列冲量接触解算。
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
#ifndef EGRET_PHYSICS_FRICTIONAL_CONTACT_RESOLVER_H
#define EGRET_PHYSICS_FRICTIONAL_CONTACT_RESOLVER_H

#include "contact_resolver_strategy.h"

namespace egret
{
    /**
     * @brief       含摩擦接触冲量解算器类。
     * @details     FrictionalContactResolverStrategy 继承自 ContactResolverStrategy，
     *              实现完整的序列冲量（Sequential Impulses）算法：
     *              - 跨帧热启动（Warm Start）：复用上一帧的累积冲量
     *              - Coulomb 摩擦模型：两个正交切向，摩擦锥裁剪
     *              - 增量冲量求解：每轮仅施加冲量差值，避免过冲
     *              - 位置求解中的切向漂移抑制
     *
     *              与 FrictionlessContactResolverStrategy 相比：
     *              - 额外处理切向摩擦冲量
     *              - 在 SolverContactConstraint 中维护跨帧累积冲量状态
     *              - 收敛更快、堆叠更稳定
     *
     * @invariant   resolveContacts() 方法更新实体速度/角速度/位置
     * @invariant   约束对象的累积冲量状态在调用间保持一致（支持热启动）
     * @remark      FrictionalContactResolverStrategy 是具体实现类，可以直接实例化
     * @see         ContactResolverStrategy, FrictionlessContactResolverStrategy,
     *              SolverContactConstraint, SolverConfig
     */
    class FrictionalContactResolverStrategy final : public ContactResolverStrategy
    {
    public:
        /** 默认构造。 */
        FrictionalContactResolverStrategy() = default;

        /** 默认析构。 */
        ~FrictionalContactResolverStrategy() override = default;

        /**
         * @brief 对接触约束执行含摩擦法向/切向冲量与位置修正。
         * @details 接口接收 const span 以保证与 ContactResolverStrategy 的签名兼容，
         *          但内部会通过 const_cast 修改约束对象中的累积冲量字段
         *          （accumulatedNormalImpulse 等），这是物理引擎中的常见模式。
         * @param scene 可变场景快照。
         * @param constraints 只读接触约束集合（内部会修改累积冲量状态）。
         * @param dt 固定步长。
         * @param config 求解器配置。
         * @param stats 每步统计信息。
         */
        void resolveContacts(SolverSceneSnapshotBase &scene,
                             std::span<const SolverContactConstraint> constraints,
                             double dt,
                             const SolverConfig &config,
                             SolverStats &stats) const override;

    private:
        /**
         * @brief 速度求解阶段：迭代施加法向与切向摩擦冲量。
         * @param bodies 可变实体句柄视图。
         * @param constraints 可变的接触约束集合。
         * @param config 求解器配置。
         * @param stats 每步统计信息。
         */
        void velocitySolve(std::span<SolverBodyHandle> bodies,
                           std::span<const SolverContactConstraint> constraints,
                           const SolverConfig &config,
                           SolverStats &stats) const;

        /**
         * @brief 位置求解阶段：穿透修正与切向漂移抑制。
         * @param bodies 可变实体句柄视图。
         * @param constraints 可变的接触约束集合。
         * @param config 求解器配置。
         * @param stats 每步统计信息。
         */
        void positionSolve(std::span<SolverBodyHandle> bodies,
                           std::span<const SolverContactConstraint> constraints,
                           const SolverConfig &config,
                           SolverStats &stats) const;

        /**
         * @brief 热启动：在速度求解前应用上一帧保存的累积冲量。
         * @param bodies 可变实体句柄视图。
         * @param constraints 只读接触约束集合。
         */
        void warmStart(std::span<SolverBodyHandle> bodies,
                       std::span<const SolverContactConstraint> constraints) const;
    };
}

#endif // EGRET_PHYSICS_FRICTIONAL_CONTACT_RESOLVER_H
