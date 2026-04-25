//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#ifndef EGRET_PHYSICS_SEMI_IMPLICIT_EULER_INTEGRATOR_H
#define EGRET_PHYSICS_SEMI_IMPLICIT_EULER_INTEGRATOR_H

#include <algorithm>

#include "integrator_strategy.h"

namespace egret
{
    /**
     * @brief 半隐式 Euler 积分策略。
     *
     * 该策略优先更新速度，再更新位置，适合第一版稳定推进。
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
        void integrate(ISolverSceneSnapshot& scene,
                       double dt,
                       const SolverConfig& config,
                       SolverStats& stats) const override;
    };
}

#endif // EGRET_PHYSICS_SEMI_IMPLICIT_EULER_INTEGRATOR_H
