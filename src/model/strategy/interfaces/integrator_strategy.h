//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#ifndef EGRET_PHYSICS_INTEGRATOR_STRATEGY_H
#define EGRET_PHYSICS_INTEGRATOR_STRATEGY_H

#include "solver_scene_snapshot.h"

namespace egret
{
    /**
     * @brief 时间积分阶段的策略接口。
     *
     * 第一版推荐使用半隐式 Euler，因为它在稳定性和实现复杂度之间更均衡。
     * 未来可扩展为 RK4 或兼容 XPBD 的变体。
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
        virtual void integrate(ISolverSceneSnapshot& scene,
                               double dt,
                               const SolverConfig& config,
                               SolverStats& stats) const = 0;
    };
}

#endif // EGRET_PHYSICS_INTEGRATOR_STRATEGY_H
