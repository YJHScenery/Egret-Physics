/**
 * @file        octree_broad_phase_strategy.cpp
 * @brief       八叉树广相碰撞检测策略实现文件
 * @details     作为薄适配层，将 SolverBodyHandle 转换为 PhysicalEntity* 后
 *              委托 Octree 内核完成八叉树构建和碰撞对查询。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-10
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Strategy
 */

#include "octree_broad_strategy.h"

#include "physical_entity.h"

namespace egret
{

    // -------------------------------------------------------------------------
    // 构造
    // -------------------------------------------------------------------------

    OctreeBroadStrategy::OctreeBroadStrategy(const std::uint64_t leafSize,
                                                       const std::uint64_t maxDepth)
        : m_octree(leafSize, maxDepth)
    {
    }

    // -------------------------------------------------------------------------
    // 公共接口
    // -------------------------------------------------------------------------

    void OctreeBroadStrategy::buildCandidatePairs(
        const SolverSceneSnapshotBase& scene,
        [[maybe_unused]] const SolverConfig& config,
        std::vector<SolverBodyPair>& outPairs,
        SolverStats& stats) const
    {
        const auto bodies = scene.getBodies();

        // ---- 提取实体指针列表（参照 BVH 策略） ----
        std::vector<PhysicalEntity*> entities{};
        entities.reserve(bodies.size());
        for (const auto& body : bodies) {
            entities.push_back(body.entity);
        }
        outPairs.clear();

        // ---- 委托内核构建并查询 ----
        m_octree.build(std::move(entities));

        const auto collisions = m_octree.findSelfCollisions();

        // ---- 映射为 SolverBodyPair ----
        for (const auto& [first, second] : collisions) {
            outPairs.push_back(SolverBodyPair{
                static_cast<std::uint64_t>(first),
                static_cast<std::uint64_t>(second)
            });
        }

        stats.broadPhasePairCount = outPairs.size();
    }

    std::string_view OctreeBroadStrategy::name() const
    {
        static const std::string s_name{"Octree"};
        return s_name;
    }

} // egret