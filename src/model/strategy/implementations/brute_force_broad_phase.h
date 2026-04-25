//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#ifndef EGRET_PHYSICS_BRUTE_FORCE_BROAD_PHASE_H
#define EGRET_PHYSICS_BRUTE_FORCE_BROAD_PHASE_H

#include "broad_phase_strategy.h"

namespace egret
{
    /**
     * @brief 暴力枚举的广相位实现。
     *
     * 该实现简单直接，适合实体数量较少的第一版与调试阶段。
     */
    class BruteForceBroadPhaseStrategy final : public BroadPhaseStrategy
    {
    public:
        /** 默认构造。 */
        BruteForceBroadPhaseStrategy() = default;

        /** 默认析构。 */
        ~BruteForceBroadPhaseStrategy() override = default;

        /**
         * @brief 使用 AABB 暴力检测生成候选碰撞对。
         * @param scene 只读场景快照。
         * @param config 求解器配置。
         * @param outPairs 输出候选对。
         * @param stats 每步统计信息。
         */
        void buildCandidatePairs(const ISolverSceneSnapshot& scene,
                                 const SolverConfig& config,
                                 std::vector<SolverBodyPair>& outPairs,
                                 SolverStats& stats) const override;
    };
}

#endif // EGRET_PHYSICS_BRUTE_FORCE_BROAD_PHASE_H
