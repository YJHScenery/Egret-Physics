/**
 * @file        kinetic_sleep_strategy.h
 * @brief       基于运动指标的休眠策略具体实现头文件
 * @details     定义 KineticSleepStrategy，实现"运动指标 + 连续渲染帧计数"的
 *              冻结判定与"受力向量差"的解冻判定。
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

#ifndef EGRET_PHYSICS_KINETIC_SLEEP_STRATEGY_H
#define EGRET_PHYSICS_KINETIC_SLEEP_STRATEGY_H

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Eigen/Dense"

#include "sleep_config.h"
#include "sleep_strategy.h"

namespace egret
{
    class PhysicalEntity;
    class SleepMetric;

    /**
     * @brief       基于运动指标与受力比对的休眠策略实现。
     * @details     KineticSleepStrategy 是休眠策略的默认实现：
     *              1. 每个计算帧对清醒的非固定实体计算运动指标（SleepMetric），
     *                 低于阈值则本渲染帧继续"通过"，否则标记未通过。
     *              2. 渲染帧结束时，若本渲染帧内所有计算帧均通过，冻结计数 +1；
     *                 否则清零。连续通过 frozenFrameCount 个渲染帧后冻结实体。
     *              3. 冻结时记录位置、速度、加速度与合力基线，并通过日志宏输出。
     *              4. 冻结期间每个计算帧比较当前合力与基线合力，向量差模长超过
     *                 基线模长的 1%（可调）即解冻，并通过日志宏输出。
     *
     * @invariant   m_states 只包含清醒实体的计数状态
     * @invariant   m_frozenSnapshots 与实体 isAsleep() 状态一一对应
     * @remark      非线程安全，多线程访问需要外部同步。
     * @see         SleepStrategy, SleepMetric, SleepConfig
     */
    class KineticSleepStrategy final : public SleepStrategy
    {
    public:
        /**
         * @brief 使用配置构造休眠策略。
         * @param config 初始运行时配置。
         */
        explicit KineticSleepStrategy(SleepConfig config = {});

        /** 析构函数（在 .cpp 中定义以获取 SleepMetric 完整类型）。 */
        ~KineticSleepStrategy() override;

        /**
         * @brief 每个计算帧评估所有非固定实体。
         * @param scene 可变场景快照。
         * @param dt 固定步长，单位为秒。
         */
        void evaluate(SolverSceneSnapshotBase &scene, double dt) override;

        /**
         * @brief 渲染帧结束回调：结算冻结计数并冻结达阈值的实体。
         */
        void onRenderFrameEnd() override;

        /**
         * @brief 查询实体是否处于冻结状态。
         * @param id 实体 ID。
         * @return true 表示已冻结。
         */
        [[nodiscard]] bool isAsleep(std::uint64_t id) const override;

        /**
         * @brief 替换运行时配置。
         * @param config 新的休眠策略配置。
         */
        void setConfig(const SleepConfig &config) override;

        /**
         * @brief 返回当前生效配置。
         * @return 当前配置的只读引用。
         */
        [[nodiscard]] const SleepConfig &getConfig() const override;

        /**
         * @brief 清空所有内部状态表与冻结快照。
         */
        void reset() override;

        /**
         * @brief 返回策略器名称。
         * @return 名称的只读字符串视图。
         */
        [[nodiscard]] std::string_view name() const override;

    private:
        /** @brief 按支撑接触岛累计仿真时间，避免离线求解依赖渲染回调。 */
        void evaluateSupported(SolverSceneSnapshotBase& scene, double dt,
                               std::span<const SolverContactConstraint> contacts) override;

        /** 接触模式下不再由渲染回调重复推进计时。 */
        bool m_contactTiming{false};
        /** 实体持续稳定的仿真时长。 */
        std::unordered_map<std::uint64_t,double> m_stableSeconds;
        /** 上次支撑图的拓扑签名，用于撤去支撑时解冻。 */
        std::unordered_map<std::uint64_t,std::uint64_t> m_supportSignatures;
        /**
         * @brief       冻结时刻的实体状态快照。
         * @details     记录位置、速度、加速度与合力基线，用于解冻时的受力比对。
         */
        struct FrozenSnapshot
        {
            /** 冻结时刻位置，单位米。 */
            Eigen::Vector3d position{Eigen::Vector3d::Zero()};

            /** 冻结时刻线速度，单位 m/s。 */
            Eigen::Vector3d velocity{Eigen::Vector3d::Zero()};

            /** 冻结时刻加速度（合力 ÷ 质量），单位 m/s²。 */
            Eigen::Vector3d acceleration{Eigen::Vector3d::Zero()};

            /** 冻结时刻合力基线，单位牛顿。 */
            Eigen::Vector3d force{Eigen::Vector3d::Zero()};
        };

        /**
         * @brief       清醒实体的冻结计数状态。
         * @details     维护"本渲染帧是否全部计算帧通过"与"连续通过渲染帧数"。
         */
        struct SleepCountState
        {
            /** 对应的实体非拥有指针（仅用于渲染帧结算时执行冻结）。 */
            PhysicalEntity *entity{nullptr};

            /** 本渲染帧内目前所有计算帧是否均通过指标检查。 */
            bool frameAllPassed{true};

            /** 本渲染帧内是否已评估过至少一个计算帧。 */
            bool frameEvaluated{false};

            /** 连续通过的渲染帧计数。 */
            std::uint32_t passedRenderFrames{0};
        };

        /**
         * @brief 判断实体运动指标是否低于冻结阈值。
         * @param entity 待检查实体。
         * @return true 表示本计算帧满足冻结候选条件。
         */
        [[nodiscard]] bool isMetricBelowThreshold(PhysicalEntity &entity) const;

        /**
         * @brief 冻结实体：记录快照、清零速度并输出日志。
         * @param entity 待冻结实体。
         */
        void freezeEntity(PhysicalEntity *entity);

        /**
         * @brief 解冻检查：比较当前合力与基线合力。
         * @param entity 已冻结实体。
         */
        void checkWakeUp(PhysicalEntity *entity);

        /** 当前运行时配置。 */
        SleepConfig m_config{};

        /** 指标计算器（随 metricType 重建）。 */
        std::unique_ptr<SleepMetric> m_metric;

        /** 清醒实体的冻结计数表：实体 ID → 计数状态。 */
        std::unordered_map<std::uint64_t, SleepCountState> m_states;

        /** 冻结实体的快照表：实体 ID → 冻结时刻信息。 */
        std::unordered_map<std::uint64_t, FrozenSnapshot> m_frozenSnapshots;
    };
}

#endif // EGRET_PHYSICS_KINETIC_SLEEP_STRATEGY_H
