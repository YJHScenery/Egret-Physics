#include "brute_force_broad_phase.h"
#include "../../physics/rigid/shape/standard/shape_base.h"
#include <cmath>
//
// Created by jehor on 2026/4/25.
//
namespace egret
{

    void BruteForceBroadPhaseStrategy::buildCandidatePairs(const SolverSceneSnapshotBase& scene,
                                                           const SolverConfig& config,
                                                           std::vector<SolverBodyPair>& outPairs,
                                                           SolverStats& stats) const
    {
        std::ignore = config;
        const auto bodies = scene.getBodies();
        outPairs.clear();

        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const SolverBodyHandle& bodyA = bodies[i];
            if (!bodyA.enableCollision || bodyA.shape == nullptr || bodyA.transform == nullptr) {
                continue;
            }
            if (!bodyA.transform->getTranslation().allFinite()) {
                continue;
            }

            const AABB aabbA = bodyA.shape->getAABB(*bodyA.transform);
            if (!aabbA.min.allFinite() || !aabbA.max.allFinite()) {
                continue;
            }

            for (std::size_t j = i + 1; j < bodies.size(); ++j) {
                const SolverBodyHandle& bodyB = bodies[j];
                if (!bodyB.enableCollision || bodyB.shape == nullptr || bodyB.transform == nullptr) {
                    continue;
                }
                if (!bodyB.transform->getTranslation().allFinite()) {
                    continue;
                }

                const AABB aabbB = bodyB.shape->getAABB(*bodyB.transform);
                if (!aabbB.min.allFinite() || !aabbB.max.allFinite()) {
                    continue;
                }
                const bool overlapX = aabbA.max.x() >= aabbB.min.x() && aabbB.max.x() >= aabbA.min.x();
                const bool overlapY = aabbA.max.y() >= aabbB.min.y() && aabbB.max.y() >= aabbA.min.y();
                const bool overlapZ = aabbA.max.z() >= aabbB.min.z() && aabbB.max.z() >= aabbA.min.z();

                if (overlapX && overlapY && overlapZ) {
                    outPairs.push_back(SolverBodyPair{i, j});
                }
            }
        }

        stats.broadPhasePairCount = outPairs.size();
    }


}
