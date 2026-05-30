/**
 * @file        solver_scene_snapshot.h
 * @brief       求解器场景快照头文件，定义场景数据的只读快照结构。
 * @details     定义 SceneSnapshot 类及其相关结构，提供场景数据的只读快照，
 *              用于在物理模拟过程中传递场景状态。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-27
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Solver
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_SOLVER_SCENE_SNAPSHOT_H
#define EGRET_PHYSICS_SOLVER_SCENE_SNAPSHOT_H

#include <span>

#include "solver_types.h"
#include "constraints_base.h"

namespace egret
{
    /**
     * @brief       求解器层使用的非拥有式场景适配接口。
     * @details     SolverSceneSnapshotBase 是求解器与具体场景存储解耦的接口。
     *              该接口将求解器与具体场景存储解耦。模型层的场景管理器可以通过
     *              暴露 body/field 缓冲区的临时 span 来实现它，供一次固定步长使用。
     *              提供实体句柄、场、约束的可变/只读视图。
     *              支持仿真时间的查询和推进。
     * 
     * @invariant   getBodies() 返回有效的实体句柄视图
     * @invariant   getFields() 返回有效的场指针视图
     * @invariant   getConstraints() 返回有效的约束指针视图
     * @remark      SolverSceneSnapshotBase 是抽象接口，不能直接实例化
     * @see         SolverBodyHandle, FieldBase, ConstraintsBase
     */
    class SolverSceneSnapshotBase
    {
    public:
        /** 多态接口的虚析构函数。 */
        virtual ~SolverSceneSnapshotBase() = default;

        /**
         * @brief 返回当前步骤中实体句柄的可变连续视图。
         * @return 指向可变实体句柄的 span。
         */
        [[nodiscard]] virtual std::span<SolverBodyHandle> getBodies() = 0;

        /**
         * @brief 返回用于检查的实体句柄只读连续视图。
         * @return 指向只读实体句柄的 span。
         */
        [[nodiscard]] virtual std::span<const SolverBodyHandle> getBodies() const = 0;

        /**
         * @brief 返回活动场的可变连续视图。
         * @return 非拥有式场指针的 span。
         */
        [[nodiscard]] virtual std::span<FieldBase *> getFields() = 0;

        /**
         * @brief 返回活动场的只读连续视图。
         * @return 非拥有式场指针的 span。
         */
        [[nodiscard]] virtual std::span<FieldBase *const> getFields() const = 0;

        /**
         * @brief 返回当前求解步骤开始前的仿真时间。
         * @return 当前仿真时间，单位为秒。
         */
        [[nodiscard]] virtual double getSimulationTime() const = 0;

        /**
         * @brief 在一次求解成功后推进模型场景的仿真时钟。
         * @param dt 固定步长，单位为秒。
         */
        virtual void advanceSimulationTime(double dt) = 0;

        /**
         * @brief 返回活动约束的可变连续视图。
         * @return 约束指针的 span。
         */
        [[nodiscard]] virtual std::span<ConstraintsBase *> getConstraints() = 0;

        /**
         * @brief 返回活动约束的只读连续视图。
         * @return 约束指针的 const span。
         */
        [[nodiscard]] virtual std::span<const ConstraintsBase *> getConstraints() const = 0;
    };
}

#endif // EGRET_PHYSICS_SOLVER_SCENE_SNAPSHOT_H
