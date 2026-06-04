//
// Created by jehor on 2026/6/4.
//

#ifndef EGRET_PHYSICS_MULTIPROCESSBRUTEFORCEBROADPHASE_H
#define EGRET_PHYSICS_MULTIPROCESSBRUTEFORCEBROADPHASE_H
#include "broad_phase_strategy.h"

namespace egret
{
    class MultiProcessBruteForceBroadPhaseStrategy: public BroadPhaseStrategy
    {
    public:
        /** 默认构造。 */
        MultiProcessBruteForceBroadPhaseStrategy() = default;

        /** 默认析构。 */
        ~MultiProcessBruteForceBroadPhaseStrategy() override = default;

        /**
         * @brief 使用 AABB 暴力检测生成候选碰撞对。
         * @param scene 只读场景快照。
         * @param config 求解器配置。
         * @param outPairs 输出候选对。
         * @param stats 每步统计信息。
         */
        void buildCandidatePairs(const SolverSceneSnapshotBase& scene,
                                 const SolverConfig& config,
                                 std::vector<SolverBodyPair>& outPairs,
                                 SolverStats& stats) const override;
    };
} // egret

#endif //EGRET_PHYSICS_MULTIPROCESSBRUTEFORCEBROADPHASE_H