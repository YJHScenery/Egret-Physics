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

    void BVHBroadStrategy::buildCandidatePairs(const SolverSceneSnapshotBase& scene, [[maybe_unused]] const SolverConfig& config,
                                               std::vector<SolverBodyPair>& outPairs, SolverStats& stats) const
    {
        const auto bodies = scene.getBodies();

        std::vector<PhysicalEntity*> entities{};
        std::vector<std::size_t> bodyIndices{};
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const auto& body = bodies[i];
            if (!body.enableCollision || body.entity == nullptr || body.entity->getShape() == nullptr) {
                continue;
            }
            entities.push_back(body.entity);
            bodyIndices.push_back(i);
        }
        outPairs.clear();

        if (entities.empty()) {
            stats.broadPhasePairCount = 0;
            return;
        }

        m_bvh.build(entities);

        const auto collisions = m_bvh.findSelfCollisions();

        for (const auto& collision : collisions) {
            outPairs.push_back(SolverBodyPair{bodyIndices[collision.first], bodyIndices[collision.second]});
        }
        stats.broadPhasePairCount = outPairs.size();
    }

    std::string_view BVHBroadStrategy::name() const
    {
        static const std::string name{"BVH"};
        return name;
    }
} // egret
