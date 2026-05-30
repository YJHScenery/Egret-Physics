/**
* @file        implementation.cpp
 * @brief       宽相碰撞检测策略实现文件
 * @details     实现宽相碰撞检测策略的各项成员函数。
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

#include "brute_force_broad_phase.h"
#include "standard/shape_base.h"
#include "physical_entity.h"


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
            if (!bodyA.enableCollision || bodyA.entity->getShape() == nullptr || bodyA.transform == nullptr) {
                continue;
            }
            if (!bodyA.transform->getTranslation().allFinite()) {
                continue;
            }

            const AABB aabbA = bodyA.entity->getShape()->getAABB(*bodyA.transform);
            if (!aabbA.min.allFinite() || !aabbA.max.allFinite()) {
                continue;
            }

            for (std::size_t j = i + 1; j < bodies.size(); ++j) {
                const SolverBodyHandle& bodyB = bodies[j];
                if (!bodyB.enableCollision || bodyB.entity->getShape() == nullptr || bodyB.transform == nullptr) {
                    continue;
                }
                if (!bodyB.transform->getTranslation().allFinite()) {
                    continue;
                }

                const AABB aabbB = bodyB.entity->getShape()->getAABB(*bodyB.transform);
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
