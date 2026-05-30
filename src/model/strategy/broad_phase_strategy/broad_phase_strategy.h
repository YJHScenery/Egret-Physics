/**
 * @file        broad_phase_strategy.h
 * @brief       宽相碰撞检测策略头文件
 * @details     定义BroadPhaseStrategy类，作为宽相碰撞检测策略的抽象接口。
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
#ifndef EGRET_PHYSICS_BROAD_PHASE_STRATEGY_H
#define EGRET_PHYSICS_BROAD_PHASE_STRATEGY_H

#include <vector>

#include "solver_scene_snapshot.h"

namespace egret
{
    /**
     * @brief       广相位候选对生成的策略接口。
     * @details     BroadPhaseStrategy 是广相位候选对生成的策略接口。
     *              典型实现包括：
     *              1. 暴力枚举所有组合（适合小规模场景）。
     *              2. Sweep and Prune 扫掠剪枝。
     *              3. 空间哈希或 BVH。
     *              采用策略模式，派生类实现具体的广相位逻辑。
     *
     * @invariant   buildCandidatePairs() 方法生成候选碰撞对
     * @remark      BroadPhaseStrategy 是抽象接口，不能直接实例化
     * @see         BruteForceBroadPhaseStrategy, SolverBodyPair, SolverConfig
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
        virtual void buildCandidatePairs(const SolverSceneSnapshotBase &scene,
                                         const SolverConfig &config,
                                         std::vector<SolverBodyPair> &outPairs,
                                         SolverStats &stats) const = 0;
    };
}

#endif // EGRET_PHYSICS_BROAD_PHASE_STRATEGY_H
