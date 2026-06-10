/**
 * @file        octree_broad_phase_strategy.h
 * @brief       八叉树广相碰撞检测策略头文件
 * @details     定义 OctreeBroadPhaseStrategy 类，作为八叉树内核（Octree）的
 *              求解器适配层。负责将 SolverBodyHandle 转换为 PhysicalEntity*
 *              并委托给内核完成构建和碰撞查询。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-10
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Strategy
 * @see         Octree, bvh_kernel.h, BVHBroadStrategy
 */
#ifndef EGRET_PHYSICS_OCTREE_BROAD_PHASE_STRATEGY_H
#define EGRET_PHYSICS_OCTREE_BROAD_PHASE_STRATEGY_H

#include <cstdint>

#include "broad_phase_strategy.h"
#include "physics_utils.h"
#include "kernel/octree_kernel.h"

namespace egret
{
    /**
     * @brief       基于八叉树空间划分的广相位策略。
     * @details     OctreeBroadPhaseStrategy 继承自 BroadPhaseStrategy，
     *              持有 Octree 内核实例。在 buildCandidatePairs() 中：
     *              1. 从场景快照提取 PhysicalEntity* 列表
     *              2. 委托 Octree::build() 构建八叉树
     *              3. 调用 Octree::findSelfCollisions() 获取候选对
     *              4. 将结果映射为 SolverBodyPair 输出
     *
     * @invariant   每帧重建八叉树
     * @remark      作为薄适配器，算法逻辑全部在 octree_kernel.h 中
     * @see         BroadPhaseStrategy, Octree, BVHBroadStrategy
     */
    class OctreeBroadStrategy final : public BroadPhaseStrategy
    {
    public:
        /** 默认构造，使用 8 叶容量 / 6 深度的默认参数。 */
        OctreeBroadStrategy() = default;

        /**
         * @brief 带参构造。
         * @param leafSize 叶子节点最大实体数。
         * @param maxDepth 八叉树最大深度。
         */
        OctreeBroadStrategy(std::uint64_t leafSize, std::uint64_t maxDepth);

        /** 默认析构。 */
        ~OctreeBroadStrategy() override = default;

        /**
         * @brief 使用八叉树生成候选碰撞对。
         * @param scene 只读场景快照。
         * @param config 求解器配置。
         * @param outPairs 输出候选对。
         * @param stats 每步统计信息。
         */
        void buildCandidatePairs(const SolverSceneSnapshotBase& scene,
                                 [[maybe_unused]] const SolverConfig& config,
                                 std::vector<SolverBodyPair>& outPairs,
                                 SolverStats& stats) const override;

        /** @return 策略名称 "Octree"。 */
        std::string_view name() const override;

    private:
        /** 八叉树内核，每帧 build 时重建。 */
        mutable Octree<std::int64_t, std::uint64_t> m_octree;
    };
} // egret

#endif //EGRET_PHYSICS_OCTREE_BROAD_PHASE_STRATEGY_H
