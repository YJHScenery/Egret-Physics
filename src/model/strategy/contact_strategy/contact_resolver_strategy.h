//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#ifndef EGRET_PHYSICS_CONTACT_RESOLVER_STRATEGY_H
#define EGRET_PHYSICS_CONTACT_RESOLVER_STRATEGY_H

#include <span>

#include "solver_scene_snapshot.h"

namespace egret
{
    /**
     * @brief 接触冲量与位置修正的策略接口。
     *
     * 第一版聚焦于无摩擦法向冲量 + 穿透修正。
     * 切向摩擦可以在后续版本中加入。
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
        virtual void resolveContacts(SolverSceneSnapshotBase& scene,
                                     std::span<const SolverContactConstraint> constraints,
                                     double dt,
                                     const SolverConfig& config,
                                     SolverStats& stats) const = 0;
    };
}

#endif // EGRET_PHYSICS_CONTACT_RESOLVER_STRATEGY_H
