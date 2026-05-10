//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#ifndef EGRET_PHYSICS_FRICTIONLESS_CONTACT_RESOLVER_H
#define EGRET_PHYSICS_FRICTIONLESS_CONTACT_RESOLVER_H

#include "contact_resolver_strategy.h"

namespace egret
{
    /**
     * @brief 无摩擦法向冲量解算器。
     *
     * 该实现只处理法向冲量与基础位置修正，不处理切向摩擦，
     * 适合第一版的稳定联调与可视化验证。
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
        void resolveContacts(SolverSceneSnapshotBase& scene,
                             std::span<const SolverContactConstraint> constraints,
                             double dt,
                             const SolverConfig& config,
                             SolverStats& stats) const override;
    };
}

#endif // EGRET_PHYSICS_FRICTIONLESS_CONTACT_RESOLVER_H
