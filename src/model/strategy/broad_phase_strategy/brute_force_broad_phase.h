/**
* @file        brute_force_broad_phase.h
 * @brief       蛮力宽相碰撞检测策略头文件
 * @details     定义BruteForceBroadPhase类，提供简单的蛮力宽相碰撞检测。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-04
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Strategy
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#ifndef EGRET_PHYSICS_BRUTE_FORCE_BROAD_PHASE_H
#define EGRET_PHYSICS_BRUTE_FORCE_BROAD_PHASE_H

#include "broad_phase_strategy.h"

namespace egret
{
    /**
     * @brief       暴力枚举的广相位实现类。
     * @details     BruteForceBroadPhaseStrategy 继承自 BroadPhaseStrategy，
     *              实现暴力枚举的广相位碰撞检测算法。
     *              该实现简单直接，适合实体数量较少的第一版与调试阶段。
     *              使用 AABB 包围盒进行初步碰撞检测。
     *              时间复杂度 O(n²)，不适合大规模场景。
     * 
     * @invariant   buildCandidatePairs() 方法枚举所有可能的碰撞对
     * @remark      BruteForceBroadPhaseStrategy 是具体实现类，可以直接实例化
     * @see         BroadPhaseStrategy, SolverBodyPair, AABB
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
        void buildCandidatePairs(const SolverSceneSnapshotBase& scene,
                                 const SolverConfig& config,
                                 std::vector<SolverBodyPair>& outPairs,
                                 SolverStats& stats) const override;
    };
}

#endif // EGRET_PHYSICS_BRUTE_FORCE_BROAD_PHASE_H
