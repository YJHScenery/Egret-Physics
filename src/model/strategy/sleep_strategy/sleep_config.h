/**
 * @file        sleep_config.h
 * @brief       休眠策略配置头文件
 * @details     定义 SleepConfig 与 SleepMetricType，作为休眠策略器的运行时配置。
 *              所有阈值均可在运行时调整，解冻比例默认 1%（可调参数）。
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

#ifndef EGRET_PHYSICS_SLEEP_CONFIG_H
#define EGRET_PHYSICS_SLEEP_CONFIG_H

#include <cstdint>

namespace egret
{
    /**
     * @brief       休眠判定所使用的指标类型。
     * @details     策略器通过 SleepMetric 抽象计算实体的"运动指标"，
     *              该枚举决定使用哪一种指标与哪一个阈值进行比较。
     * @see         SleepMetric, SleepConfig
     */
    enum class SleepMetricType : std::uint8_t
    {
        /** 线速度平方与角速度平方之和，单位 m²/s² 与 rad²/s²。抗抖动能力最强，推荐默认值。 */
        SpeedSquared = 0,

        /** 平动动能，单位焦耳（J）。 */
        KineticEnergy = 1,

        /** 线动量平方（不含角动量），单位 (kg·m/s)²。 */
        Momentum = 2,
    };

    /**
     * @brief       休眠策略器的运行时配置。
     * @details     SleepConfig 汇总休眠判定与唤醒判定的全部可调参数。
     *              冻结判定：一个渲染帧内所有计算帧的指标均低于冻结阈值时，
     *              该渲染帧计为"通过"；连续通过 frozenFrameCount 个渲染帧后冻结。
     *              解冻判定：冻结期间每个计算帧比较当前合力与冻结时基线合力，
     *              向量差模长超过基线模长的 wakeUpForceRatio（默认 1%）时解冻；
     *              基线模长接近零时改用 wakeUpAbsoluteForceThreshold 兜底。
     *
     * @invariant   wakeUpForceRatio > 0
     * @invariant   frozenFrameCount > 0
     * @invariant   各冻结阈值 >= 0
     * @see         KineticSleepStrategy, SleepMetricType
     */
    struct SleepConfig
    {
        /** 是否启用休眠策略。关闭时策略器不改变任何实体状态。 */
        bool enabled{true};

        /** 当前使用的运动指标类型。 */
        SleepMetricType metricType{SleepMetricType::SpeedSquared};

        /** SpeedSquared 指标的冻结阈值：线速度² + 角速度² 低于该值视为静止候选（默认 0.01 m/s 的平方）。 */
        double speedSquaredThreshold{1e-4};

        /** KineticEnergy 指标的冻结阈值，单位焦耳。 */
        double kineticEnergyThreshold{1e-3};

        /** Momentum 指标的冻结阈值，单位 (kg·m/s)²。 */
        double momentumThreshold{1e-3};

        /** 解冻判定比例：|当前合力 - 基线合力| > 比例 × |基线合力| 时解冻。默认 1%。 */
        double wakeUpForceRatio{0.01};

        /** 基线合力模长低于该值时，改用绝对比较：向量差模长超过该值即解冻，单位牛顿。 */
        double wakeUpAbsoluteForceThreshold{1e-4};

        /** 连续通过的渲染帧数达到该值后冻结实体（默认 30 帧，约 0.5 秒）。 */
        std::uint32_t frozenFrameCount{30};

        /** 冻结时是否将线速度与角速度清零（推荐开启，消除残余微颤）。 */
        bool zeroVelocityOnFreeze{true};
    };
}

#endif // EGRET_PHYSICS_SLEEP_CONFIG_H
