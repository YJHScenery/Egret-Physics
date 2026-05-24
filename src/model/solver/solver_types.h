//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

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
     * @brief 求解器流水线中单个物理体的可变视图。
     *
     * 场景层拥有所有被指向的对象。求解器只会在一次步进过程中借用它们，
     * 不能在当前步之外长期保存这些指针。
     */
    struct SolverBodyHandle
    {
        /** 来自场景层/模型层的稳定实体 ID，用于映射与调试。 */
        std::uint64_t id{0};

        /** 指向仿真实体状态的非拥有指针（质量、速度、外力等）。 */
        PhysicalEntity *entity{nullptr};

        /** 指向世界变换的非拥有指针，用于形状碰撞与渲染桥接。 */
        Transform *transform{nullptr};

        /** 指向碰撞形状的非拥有指针，在窄相位中使用。 */
        // ShapeBase *shape{nullptr};

        /** 逆质量，用于快速冲量求解；静态体或无限质量体设为 0。 */
        double inverseMass{0.0};

        /** 世界坐标系下的逆转动惯量矩阵，用于碰撞角动量传递。 */
        Eigen::Matrix3d inverseInertiaTensor{Eigen::Matrix3d::Zero()};

        /** 恢复系数，范围为 [0, 1]，用于无摩擦法向冲量。 */
        double restitution{0.2};

        /** 当前实体是否参与广相位/窄相位碰撞检测。 */
        bool enableCollision{true};

        /** 当前实体在本步是否参与积分更新。 */
        bool enableIntegration{true};
    };

    /**
     * @brief 广相位生成的候选碰撞对。
     *
     * 这里保存的是当前实体数组中的索引，而不是稳定的场景 ID。
     */
    struct SolverBodyPair
    {
        /** 物体 A 在求解器实体缓冲区中的索引。 */
        std::size_t bodyAIndex{0};

        /** 物体 B 在求解器实体缓冲区中的索引。 */
        std::size_t bodyBIndex{0};
    };

    /**
     * @brief 当前步需要解算的窄相位接触约束。
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
     * @brief CCD碰撞事件，包含碰撞时间和接触信息。
     *
     * 用于TOI事件队列，支持时间步进处理多个碰撞。
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
     * @brief 控制一次固定步长求解的运行时选项。
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
     * @brief 每一步的统计信息，用于调试、测试和界面展示。
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
     * @brief 一次求解步的结构化结果。
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
