//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#ifndef EGRET_PHYSICS_BROAD_PHASE_STRATEGY_H
#define EGRET_PHYSICS_BROAD_PHASE_STRATEGY_H

#include <vector>

#include "solver_scene_snapshot.h"

namespace egret
{
    /**
     * @brief 广相位候选对生成的策略接口。
     *
     * 典型实现包括：
     * 1. 暴力枚举所有组合（适合小规模场景）。
     * 2. Sweep and Prune 扫掠剪枝。
     * 3. 空间哈希或 BVH。
     */
    class BroadPhaseStrategy
    {
    public:
        /** 策略多态所需的虚析构函数。 */
        virtual ~BroadPhaseStrategy() = default;

        /**
         * @brief 生成供窄相位使用的候选碰撞对。
         * @param scene 只读场景快照。
         * @param config 运行时求解器选项。
         * @param outPairs 输出的候选对缓冲区，可追加或覆盖。
         * @param stats 可变的每步统计信息。
         */
        virtual void buildCandidatePairs(const ISolverSceneSnapshot& scene,
                                         const SolverConfig& config,
                                         std::vector<SolverBodyPair>& outPairs,
                                         SolverStats& stats) const = 0;
    };
}

#endif // EGRET_PHYSICS_BROAD_PHASE_STRATEGY_H
