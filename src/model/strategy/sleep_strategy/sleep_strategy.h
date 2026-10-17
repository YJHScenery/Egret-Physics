/**
 * @file        sleep_strategy.h
 * @brief       休眠策略抽象头文件
 * @details     定义 SleepStrategy 抽象接口，作为实体休眠/冻结判定的策略扩展点。
 *              派生类在每个计算帧评估实体指标、在每个渲染帧结算冻结计数，
 *              并维护实体 ID 到冻结状态的映射表。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-08-31
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Strategy
 * @defgroup    SleepStrategy
 */

#ifndef EGRET_PHYSICS_SLEEP_STRATEGY_H
#define EGRET_PHYSICS_SLEEP_STRATEGY_H

#include <cstdint>
#include <string_view>
#include <span>

#include "sleep_config.h"

namespace egret
{
    class SolverSceneSnapshotBase;
    struct SolverContactConstraint;

    /**
     * @brief       休眠判定阶段的策略接口。
     * @details     SleepStrategy 是休眠判定阶段的策略接口。
     *              派生类实现：
     *              1. evaluate()：每个计算帧调用，检查实体运动指标并执行解冻检查。
     *              2. onRenderFrameEnd()：每个渲染帧结束时调用，结算本渲染帧内
     *                 所有计算帧的通过情况，累加冻结计数并在达到阈值时冻结实体。
     *              策略器内部维护实体 ID 到冻结计数/冻结快照的映射表。
     * @remark      纯抽象接口，不能直接实例化。
     * @see         KineticSleepStrategy, SleepConfig, SleepMetric
     */
    class SleepStrategy
    {
    public:
        /** 多态接口的虚析构函数。 */
        virtual ~SleepStrategy() = default;

        /**
         * @brief 每个计算帧评估所有非固定实体。
         * @details 对清醒实体检查运动指标是否符合冻结候选条件；
         *          对冻结实体执行受力比对以决定是否解冻。
         * @param scene 可变场景快照。
         * @param dt 固定步长，单位为秒。
         */
        virtual void evaluate(SolverSceneSnapshotBase &scene, double dt) = 0;

        /** @brief 按仿真时间评估接触支撑；缺省兼容原有策略。 */
        virtual void evaluateSupported(SolverSceneSnapshotBase& scene, double dt,
                                       std::span<const SolverContactConstraint> contacts)
        {
            evaluate(scene,dt);
        }

        /**
         * @brief 渲染帧结束回调。
         * @details 结算本渲染帧内所有计算帧的指标通过情况，
         *          累加连续通过渲染帧数，并在达到阈值时冻结实体。
         */
        virtual void onRenderFrameEnd() = 0;

        /**
         * @brief 查询实体当前是否处于冻结状态。
         * @param id 实体 ID。
         * @return true 表示实体已冻结，false 表示清醒或未跟踪。
         */
        [[nodiscard]] virtual bool isAsleep(std::uint64_t id) const = 0;

        /**
         * @brief 替换运行时配置。
         * @param config 新的休眠策略配置。
         */
        virtual void setConfig(const SleepConfig &config) = 0;

        /**
         * @brief 返回当前生效的运行时配置。
         * @return 当前配置的只读引用。
         */
        [[nodiscard]] virtual const SleepConfig &getConfig() const = 0;

        /**
         * @brief 清空所有内部状态表。
         * @details 场景重置或实体集合重建时调用，防止陈旧 ID 与新实体冲突。
         */
        virtual void reset() = 0;

        /**
         * @brief 返回策略器名称。
         * @return 策略器名称的只读字符串视图。
         */
        [[nodiscard]] virtual std::string_view name() const = 0;
    };
}

#endif // EGRET_PHYSICS_SLEEP_STRATEGY_H
