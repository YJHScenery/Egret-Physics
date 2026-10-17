/**
 * @file contact_graph_projector.h
 * @brief 接触岛的质量加权同步位置投影接口。
 */
#pragma once
#include "solver_scene_snapshot.h"
#include <span>

namespace egret
{
    /** @brief 在同一位姿快照上求解整个接触岛，统一提交平移和转角。 */
    class ContactGraphProjector
    {
    public:
        /** @brief 投影单边接触约束，并记录最大剩余穿透与求解残差。 */
        static void project(std::span<SolverBodyHandle> bodies,
                            std::span<const SolverContactConstraint> contacts,
                            const SolverConfig& config, SolverStats& stats,
                            std::span<ConstraintsBase*> joints = {});
    };
}
