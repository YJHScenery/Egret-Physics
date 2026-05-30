/**
 * @file        solver_types.h
 * @brief       求解器类型定义头文件，定义求解器中使用的数据结构。
 * @details     定义 SolverBodyHandle、SolverFieldHandle 等数据结构，
 *              用于在求解器流水线中表示物理体的可变视图。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-04
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Solver
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#ifndef EGRET_PHYSICS_SOLVER_TYPES_H
#define EGRET_PHYSICS_SOLVER_TYPES_H

#include <cstddef>
#include <cstdint>

#include "Eigen/Dense"
#include "basic_utils.h"
#include "physics_utils.h"

namespace egret
{
    class PhysicalEntity;
    class FieldBase;
    class ShapeBase;
    class Transform;

    /**
     * @brief       求解器流水线中单个物理体的可变视图。
     * @details     SolverBodyHandle 是求解器流水线中单个物理体的可变视图结构体。
     *              场景层拥有所有被指向的对象。求解器只会在一次步进过程中借用它们，
     *              不能在当前步之外长期保存这些指针。
     *              包含实体ID、实体指针、变换指针、逆质量、逆转动惯量等。
     *
     * @invariant   id 是稳定的实体标识符
     * @invariant   entity 指针在一次步进过程中有效
     * @invariant   transform 指针在一次步进过程中有效
     * @invariant   inverseMass >= 0，静态体为0
     * @invariant   restitution 在 [0, 1] 范围内
     * @see         SolverSceneSnapshotBase, PhysicalEntity, Transform
     */
    struct SolverBodyHandle
    {
        /** 指向仿真实体状态的非拥有指针（质量、速度、外力等）。 */
        PhysicalEntity *entity{nullptr};

        /** 指向碰撞形状的非拥有指针，在窄相位中使用。 */
        // ShapeBase *shape{nullptr};

        /** 逆质量，用于快速冲量求解；静态体或无限质量体设为 0。 */
        double inverseMass{0.0};

        /** 世界坐标系下的逆转动惯量矩阵，用于碰撞角动量传递。 */
        Eigen::Matrix3d inverseInertiaTensor{Eigen::Matrix3d::Zero()};

        /** 当前实体是否参与广相位/窄相位碰撞检测。 */
        bool enableCollision{true};

        /** 当前实体在本步是否参与积分更新。 */
        bool enableIntegration{true};
    };

    /**
     * @brief       广相位生成的候选碰撞对。
     * @details     SolverBodyPair 是广相位生成的候选碰撞对结构体。
     *              这里保存的是当前实体数组中的索引，而不是稳定的场景 ID。
     *              用于窄相位碰撞检测的输入。
     *
     * @invariant   bodyAIndex < bodyBIndex（可选约定）
     * @invariant   bodyAIndex 和 bodyBIndex 是有效的实体数组索引
     * @see         BroadPhaseStrategy, SolverBodyHandle
     */
    struct SolverBodyPair
    {
        /** 物体 A 在求解器实体缓冲区中的索引。 */
        std::size_t bodyAIndex{0};

        /** 物体 B 在求解器实体缓冲区中的索引。 */
        std::size_t bodyBIndex{0};
    };

    /**
     * @brief       当前步需要解算的窄相位接触约束。
     * @details     SolverContactConstraint 是当前步需要解算的窄相位接触约束结构体。
     *              包含碰撞的两个实体索引、接触点数据、恢复系数和碰撞时间（TOI）。
     *              用于接触解算器的输入。
     *
     * @invariant   bodyAIndex 和 bodyBIndex 是有效的实体数组索引
     * @invariant   contact 包含有效的接触点数据
     * @invariant   restitution 在 [0, 1] 范围内
     * @invariant   toi 在 [0, dt] 范围内
     * @see         ContactResolverStrategy, ContactPoint
     */
    struct SolverContactConstraint
    {
        /** 物体 A 在求解器实体缓冲区中的索引。 */
        std::size_t bodyAIndex{0};

        /** 物体 B 在求解器实体缓冲区中的索引。 */
        std::size_t bodyBIndex{0};

        /** 来自碰撞检测器的接触点数据（位置/法线/穿透深度）。 */
        ContactPoint contact{};

        /** 该接触对的有效恢复系数（通常取两个物体的最小值或平均值）。 */
        double restitution{0.2};

        /** 碰撞发生的时间（Time of Impact），范围 [0, dt]。 */
        double toi{0.0};
    };

    /**
     * @brief       CCD碰撞事件，包含碰撞时间和接触信息。
     * @details     CcdCollisionEvent 是CCD碰撞事件结构体，用于TOI事件队列。
     *              包含碰撞发生的时间（TOI）、碰撞的两个实体索引、接触流形和恢复系数。
     *              支持时间步进处理多个碰撞。
     *
     * @invariant   toi 在 [0, dt] 范围内
     * @invariant   bodyAIndex 和 bodyBIndex 是有效的实体数组索引
     * @invariant   manifold 包含有效的接触点数据
     * @see         SolverContactConstraint, ContactManifold
     */
    struct CcdCollisionEvent
    {
        /** 碰撞发生的时间（Time of Impact），范围 [0, dt]。 */
        double toi{0.0};

        /** 物体 A 在求解器实体缓冲区中的索引。 */
        std::size_t bodyAIndex{0};

        /** 物体 B 在求解器实体缓冲区中的索引。 */
        std::size_t bodyBIndex{0};

        /** 接触流形，包含所有接触点。 */
        ContactManifold manifold{};

        /** 该接触对的有效恢复系数。 */
        double restitution{0.2};

        /**
         * @brief 比较两个碰撞事件的TOI。
         * @param other 另一个碰撞事件。
         * @return 如果this的TOI小于other，返回true。
         */
        bool operator<(const CcdCollisionEvent &other) const
        {
            return toi < other.toi;
        }
    };

    /**
     * @brief       控制一次固定步长求解的运行时选项。
     * @details     SolverConfig 是控制一次固定步长求解的运行时选项结构体。
     *              包含速度迭代次数、位置迭代次数、穿透容差、修正强度等参数。
     *              用于配置求解器的行为。
     *
     * @invariant   velocityIterations > 0
     * @invariant   positionIterations > 0
     * @invariant   penetrationSlop >= 0
     * @invariant   positionCorrectionBeta 在 [0, 1] 范围内
     * @see         Solver, SolverBase
     */
    struct SolverConfig
    {
        /** 用于速度修正的序列冲量迭代次数。 */
        std::uint32_t velocityIterations{8};

        /** 用于穿透恢复的位置修正迭代次数。 */
        std::uint32_t positionIterations{2};

        /** 位置修正时忽略的穿透容差（单位：米），用于降低抖动。 */
        double penetrationSlop{0.001};

        /** 类 Baumgarte 的修正强度，范围为 [0, 1]，用于位置稳定化。 */
        double positionCorrectionBeta{0.2};

        /** 单步积分允许的最大平移幅度上限。 */
        double maxTranslationPerStep{10.0};

        /** 若为真，则在场作用前清空实体外力累加器。 */
        bool clearForcesBeforeFieldApply{true};

        /** 若为真，则每一步都应用场景中的所有场。 */
        bool applyFieldsEachStep{true};

        /** 若为真，则运行广相位生成候选碰撞对。 */
        bool enableBroadPhase{true};

        /** 若为真，则对候选碰撞对运行窄相位检测。 */
        bool enableNarrowPhase{true};

        /** 若为真，则使用法向冲量 + 位置修正解算接触。 */
        bool enableContactResolution{true};

        /** 若为真，则在外力/接触处理后积分实体状态。 */
        bool enableIntegration{true};

        /** 若为真，则使用TOI事件队列进行时间步进的CCD碰撞检测。 */
        bool enableToiQueue{true};
    };

    /**
     * @brief       每一步的统计信息，用于调试、测试和界面展示。
     * @details     SolverStats 是每一步的统计信息结构体，用于调试、测试和界面展示。
     *              包含实体数量、候选对数量、接触约束数量、动能等统计数据。
     *              用于监控求解器的性能和行为。
     *
     * @invariant   bodyCount >= 0
     * @invariant   dynamicBodyCount >= 0
     * @invariant   broadPhasePairCount >= 0
     * @invariant   totalKineticEnergy >= 0
     * @see         SolverStepResult, Solver
     */
    struct SolverStats
    {
        /** 求解器在本步看到的实体总数。 */
        std::size_t bodyCount{0};

        /** 逆质量非零的动态实体数量。 */
        std::size_t dynamicBodyCount{0};

        /** 广相位返回的候选碰撞对数量。 */
        std::size_t broadPhasePairCount{0};

        /** 窄相位执行的碰撞测试次数。 */
        std::size_t narrowPhaseTestCount{0};

        /** 窄相位生成的接触点/约束数量。 */
        std::size_t contactConstraintCount{0};

        /** 在解算器中产生有效冲量的约束数量。 */
        std::size_t resolvedContactCount{0};

        /** 约束数量。 */
        std::size_t constraintCount{0};

        /** 积分后动态实体的平动动能总和。 */
        double totalKineticEnergy{0.0};
    };

    /**
     * @brief       一次求解步的结构化结果。
     * @details     SolverStepResult 是一次求解步的结构化结果结构体。
     *              包含汇总后的每步统计信息和本次求解消耗的固定时间步长。
     *              用于返回求解器的执行结果。
     *
     * @invariant   dt > 0
     * @see         SolverBase, SolverStats
     */
    struct SolverStepResult
    {
        /** 汇总后的每步统计信息。 */
        SolverStats stats{};

        /** 本次求解消耗的固定时间步长（单位：秒）。 */
        double dt{0.0};
    };
}

#endif // EGRET_PHYSICS_SOLVER_TYPES_H
