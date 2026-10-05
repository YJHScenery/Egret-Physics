/**
 * @file        expression_evaluator.h
 * @brief       理论方程（表达式）求值与误差对比。
 * @details     基于 muparser 解析用户输入的理论公式，例如
 *              `x0 + v0*t + 0.5*a*t*t`、`A*exp(-lambda*t)`，自变量名与常量由界面给出。
 *              求值后与实测序列逐点对比，给出决定系数、RMSE、MAE、最大绝对误差、
 *              归一化 RMSE 与相对误差等指标，用于量化物理仿真模型与理论解的偏差。
 *
 *              支持的函数：sin/cos/tan/asin/acos/atan/atan2、exp/log/ln/log10/log2、
 *              sqrt/cbrt/pow/abs/min/max/sinh/cosh/tanh 等；
 *              内置常量：pi、e。
 *
 * @author      jehor
 * @date        2026-10-05
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 * @defgroup    Analysis (数据分析)
 */

#ifndef EGRET_PHYSICS_EXPRESSION_EVALUATOR_H
#define EGRET_PHYSICS_EXPRESSION_EVALUATOR_H

#include <QMap>
#include <QString>
#include <QVector>

#include <cstddef>
#include <cstdint>

namespace egret::analysis
{
    /** 一次误差对比结果。 */
    struct ComparisonResult
    {
        /** 是否成功。 */
        bool ok{false};

        /** 参与的样本数。 */
        int sampleCount{0};

        /** 决定系数 R²。 */
        double rSquared{0.0};

        /** 均方根误差。 */
        double rmse{0.0};

        /** 平均绝对误差。 */
        double mae{0.0};

        /** 最大绝对误差。 */
        double maxAbsError{0.0};

        /** 归一化 RMSE = RMSE / (max y - min y)。 */
        double nrmse{0.0};

        /** 平均相对误差（仅统计 |y| > 阈值 的点）。 */
        double meanRelativeError{0.0};

        /** 最大相对误差。 */
        double maxRelativeError{0.0};

        /** 失败原因或提示。 */
        QString message{};

        /** 理论值序列。 */
        QVector<double> predicted{};

        /** 残差序列（理论 - 实测）。 */
        QVector<double> residual{};
    };

    /**
     * @brief       表达式求值与误差对比。
     * @details     所有接口均为静态，调用方无需持有实例；每次调用独立构造解析器，
     *              因此可以在任意线程安全使用。
     */
    class ExpressionEvaluator final
    {
    public:
        /** 判定相对误差时使用的 |y| 下限，避免除以接近零的值。 */
        static constexpr double kRelativeErrorFloor{1.0e-12};

        /**
         * @brief  校验表达式是否可解析。
         * @param  expression 表达式文本。
         * @param  variableName 自变量名（例如 "t"）。
         * @param  variables 常量表（可为空）。
         * @param  errorMessage 失败原因，可为 nullptr。
         * @return 可解析返回 true。
         */
        [[nodiscard]] static bool validate(const QString& expression,
                                           const QString& variableName,
                                           const QMap<QString, double>& variables,
                                           QString* errorMessage);

        /**
         * @brief  逐点求值并与实测序列对比。
         * @param  x 自变量数组。
         * @param  measured 实测值数组。
         * @param  count 样本数。
         * @param  expression 理论方程表达式。
         * @param  variableName 自变量名。
         * @param  variables 常量表。
         * @return 对比结果；失败时 ok 为 false 且 message 说明原因。
         */
        [[nodiscard]] static ComparisonResult compare(const double* x,
                                                      const double* measured,
                                                      std::size_t count,
                                                      const QString& expression,
                                                      const QString& variableName,
                                                      const QMap<QString, double>& variables);

        /**
         * @brief  在给定自变量上求理论值（用于把理论曲线叠加到图上）。
         * @param  x 自变量数组。
         * @param  count 样本数。
         * @param  expression 表达式。
         * @param  variableName 自变量名。
         * @param  variables 常量表。
         * @param  out 输出数组，不可为 nullptr。
         * @param  errorMessage 失败原因，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] static bool evaluateOnGrid(const double* x,
                                                 std::size_t count,
                                                 const QString& expression,
                                                 const QString& variableName,
                                                 const QMap<QString, double>& variables,
                                                 QVector<double>* out,
                                                 QString* errorMessage);
    };
}

#endif // EGRET_PHYSICS_EXPRESSION_EVALUATOR_H
