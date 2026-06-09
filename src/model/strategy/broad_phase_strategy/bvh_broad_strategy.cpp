//
// Created by jehor on 2026/6/9.
//

#include "bvh_broad_strategy.h"

#include <iostream>

#include "physical_entity.h"
#include <unordered_map>

namespace egret
{

    BVHBroadStrategy::BVHBroadStrategy(const std::uint64_t leafSize, const std::uint64_t maxDepth): m_bvh(leafSize, maxDepth)
    {
    }

    void BVHBroadStrategy::buildCandidatePairs(const SolverSceneSnapshotBase& scene, const SolverConfig& config,
                                               std::vector<SolverBodyPair>& outPairs, SolverStats& stats) const
    {
        std::ignore = config;
        const auto bodies = scene.getBodies();

        std::vector<PhysicalEntity*> entities{};
        for (const auto& entity : bodies) {
            entities.push_back(entity.entity);
        }
        outPairs.clear();

        m_bvh.build(entities);

        const auto collisions = m_bvh.findSelfCollisions();

        for (const auto& collision : collisions) {
            outPairs.push_back(SolverBodyPair{static_cast<std::uint64_t>(collision.first), static_cast<std::uint64_t>(collision.second)});
        }
        for (const auto p: outPairs) {
            LOG_DEBUG(std::format("Broad Pair: {}, {}", p.bodyAIndex, p.bodyBIndex));
        }
        stats.broadPhasePairCount = outPairs.size();
    }

    std::string_view BVHBroadStrategy::name() const
    {
        static const std::string name{"BVH"};
        return name;
    }
} // egret