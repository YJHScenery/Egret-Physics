//
// Created by jehor on 2026/6/9.
//

#ifndef EGRET_PHYSICS_BVH_BROAD_STRATEGY_H
#define EGRET_PHYSICS_BVH_BROAD_STRATEGY_H

#include "broad_phase_strategy.h"
#include <cstdint>
#include "physics_utils.h"
#include "kernel/bvh_kernel.h"


namespace egret
{
    class BVHBroadStrategy : public BroadPhaseStrategy
    {
    public:
        BVHBroadStrategy() = default;

        BVHBroadStrategy(std::uint64_t leafSize, std::uint64_t maxDepth);

        void buildCandidatePairs(const SolverSceneSnapshotBase& scene, [[maybe_unused]] const SolverConfig& config,
                                 std::vector<SolverBodyPair>& outPairs, SolverStats& stats) const override;

        [[nodiscard]]std::string_view name() const override;

    private:

        mutable BVH<std::int64_t, std::uint64_t> m_bvh;
    };
} // egret

#endif //EGRET_PHYSICS_BVH_BROAD_STRATEGY_H
