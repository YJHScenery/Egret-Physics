/**
 * @file        sleep_metric.h
 * @brief       休眠判定指标抽象头文件
 * @details     定义 SleepMetric 抽象接口与三种内置指标实现，
 *              将"能量、动量或其他计算模式"抽象为可插拔的指标计算器。
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

#ifndef EGRET_PHYSICS_SLEEP_METRIC_H
#define EGRET_PHYSICS_SLEEP_METRIC_H

#include <memory>
#include <string>
#include <string_view>

#include "physical_entity.h"
#include "sleep_config.h"

namespace egret
{
    /**
     * @brief       休眠判定指标的抽象接口。
     * @details     SleepMetric 计算实体的"运动指标"标量，用于与冻结阈值比较。
     *              指标越小表示实体越接近静止。派生类实现具体的计算模式
     *              （速度平方、动能、动量等）。
     * @remark      纯抽象接口，不能直接实例化。
     * @see         SpeedSquaredMetric, KineticEnergyMetric, MomentumMetric
     */
    class SleepMetric
    {
    public:
        /** 多态接口的虚析构函数。 */
        virtual ~SleepMetric() = default;

        /**
         * @brief 计算指定实体的运动指标。
         * @param entity 待评估的物理实体。
         * @return 非负指标标量，单位取决于具体实现。
         */
        virtual double compute(PhysicalEntity &entity) const = 0;

        /**
         * @brief 返回指标名称（用于日志与调试）。
         * @return 指标名称的只读字符串视图。
         */
        [[nodiscard]] virtual std::string_view name() const = 0;
    };

    /**
     * @brief       速度平方指标：线速度² + 角速度²。
     * @details     该指标不依赖质量，对所有实体使用统一阈值，抗单帧抖动能力最强。
     *              角速度项仅对具有转动自由度的实体计入。
     * @see         SleepMetric
     */
    class SpeedSquaredMetric final : public SleepMetric
    {
    public:
        /**
         * @brief 计算线速度平方与角速度平方之和。
         * @param entity 待评估的物理实体。
         * @return 速度平方和，单位 m²/s² + rad²/s²。
         */
        double compute(PhysicalEntity &entity) const override
        {
            const double linearSq = entity.getVelocity().squaredNorm();
            const double angularSq = entity.hasRotation() ? entity.getAngularVelocity().squaredNorm() : 0.0;
            return linearSq + angularSq;
        }

        [[nodiscard]] std::string_view name() const override
        {
            static const std::string metricName{"SpeedSquared"};
            return metricName;
        }
    };

    /**
     * @brief       动能指标：平动动能（+ 刚体转动动能）。
     * @details     通过 PhysicalEntity::getKineticEnergy() 复用实体自身的能量定义，
     *              质量越大的实体需要越低的能量才能满足冻结条件。
     * @see         SleepMetric
     */
    class KineticEnergyMetric final : public SleepMetric
    {
    public:
        /**
         * @brief 计算实体动能。
         * @param entity 待评估的物理实体。
         * @return 动能，单位焦耳（J）。
         */
        double compute(PhysicalEntity &entity) const override
        {
            return entity.getKineticEnergy();
        }

        [[nodiscard]] std::string_view name() const override
        {
            static const std::string metricName{"KineticEnergy"};
            return metricName;
        }
    };

    /**
     * @brief       动量指标：线动量平方。
     * @details     使用 getMomentum().squaredNorm()，反映实体平动动量大小，
     *              与速度平方指标的区别在于携带了质量信息。
     * @see         SleepMetric
     */
    class MomentumMetric final : public SleepMetric
    {
    public:
        /**
         * @brief 计算线动量平方。
         * @param entity 待评估的物理实体。
         * @return 线动量平方，单位 (kg·m/s)²。
         */
        double compute(PhysicalEntity &entity) const override
        {
            return entity.getMomentum().squaredNorm();
        }

        [[nodiscard]] std::string_view name() const override
        {
            static const std::string metricName{"Momentum"};
            return metricName;
        }
    };

    /**
     * @brief 根据指标类型创建对应的指标计算器。
     * @param type 指标类型。
     * @return 新的指标计算器实例。
     */
    [[nodiscard]] inline std::unique_ptr<SleepMetric> makeSleepMetric(const SleepMetricType type)
    {
        switch (type) {
        case SleepMetricType::SpeedSquared:
            return std::make_unique<SpeedSquaredMetric>();
        case SleepMetricType::KineticEnergy:
            return std::make_unique<KineticEnergyMetric>();
        case SleepMetricType::Momentum:
            return std::make_unique<MomentumMetric>();
        }
        return std::make_unique<SpeedSquaredMetric>();
    }
}

#endif // EGRET_PHYSICS_SLEEP_METRIC_H
