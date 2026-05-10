//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#ifndef EGRET_PHYSICS_SOLVER_SCENE_SNAPSHOT_H
#define EGRET_PHYSICS_SOLVER_SCENE_SNAPSHOT_H

#include <span>

#include "solver_types.h"

namespace egret
{
    /**
     * @brief 求解器层使用的非拥有式场景适配接口。
     *
     * 该接口将求解器与具体场景存储解耦。模型层的场景管理器可以通过
     * 暴露 body/field 缓冲区的临时 span 来实现它，供一次固定步长使用。
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
        [[nodiscard]] virtual std::span<FieldBase*> getFields() = 0;

        /**
         * @brief 返回活动场的只读连续视图。
         * @return 非拥有式场指针的 span。
         */
        [[nodiscard]] virtual std::span<FieldBase* const> getFields() const = 0;

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
    };
}

#endif // EGRET_PHYSICS_SOLVER_SCENE_SNAPSHOT_H
