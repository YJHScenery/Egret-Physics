/**
 * @file        curve_fitter.h
 * @brief       曲线拟合：线性 / 多项式 / 三角函数 / 指数 / 对数。
 * @details     CurveFitter 对一维样本 (x, y) 做最小二乘拟合，返回系数、可读方程与优度指标。
 *
 *              参数线性模型（线性、多项式、固定频率的三角函数、对数、a·e^{bx}）
 *              直接用线性最小二乘（列主元 QR，奇异时退化为 SVD）求解；
 *              参数非线性的模型（带偏移的指数、频率未知的三角函数）用
 *              自实现的 Levenberg–Marquardt 迭代，数值雅可比由 Eigen 稠密求解器驱动。
 *
 *              数值稳定性：多项式先把自变量归一化为 u = (x - x̄)/σ 再构造范德蒙德矩阵，
 *              避免高阶时矩阵病态；返回的方程在 u 上给出，并同时返回归一化参数。
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

#ifndef EGRET_PHYSICS_CURVE_FITTER_H
#define EGRET_PHYSICS_CURVE_FITTER_H

#include <QString>
#include <QStringList>
#include <QVector>

#include <cstddef>
#include <cstdint>

namespace egret::analysis
{
    /** 拟合模型族。 */
    enum class FitModel : std::uint8_t
    {
        /** y = a x + b。 */
        Linear = 0,

        /** y = Σ a_k u^k，u = (x - x̄)/σ。 */
        Polynomial = 1,

        /** y = a₀ + Σ [a_k sin(kωx) + b_k cos(kωx)]。 */
        Trigonometric = 2,

        /** y = a e^{bx}（可选 + c）。 */
        Exponential = 3,

        /** y = a ln x + b（要求 x > 0）。 */
        Logarithmic = 4,
    };

    /** 多项式阶数上限。 */
    inline constexpr int kMaxPolynomialDegree{12};

    /** 三角函数谐波数上限。 */
    inline constexpr int kMaxHarmonics{8};

    /** 一次拟合请求。 */
    struct FitRequest
    {
        /** 模型族。 */
        FitModel model{FitModel::Linear};

        /** 多项式阶数（1 ~ kMaxPolynomialDegree）。 */
        int polynomialDegree{2};

        /** 三角函数谐波数（1 ~ kMaxHarmonics）。 */
        int harmonicCount{1};

        /** 三角函数的角频率；≤ 0 表示自动估计。 */
        double angularFrequency{0.0};

        /** 指数模型是否带常数偏移项 c。 */
        bool exponentialWithOffset{false};
    };

    /** 一次拟合结果。 */
    struct FitResult
    {
        /** 是否拟合成功。 */
        bool ok{false};

        /** 实际使用的模型族。 */
        FitModel model{FitModel::Linear};

        /** 可读方程（多项式用归一化变量 u 表示）。 */
        QString equation{};

        /** 系数值，顺序与 coefficientNames 一致。 */
        QVector<double> coefficients{};

        /** 系数名。 */
        QStringList coefficientNames{};

        /** 决定系数 R²。 */
        double rSquared{0.0};

        /** 调整后的决定系数。 */
        double adjustedRSquared{0.0};

        /** 均方根误差。 */
        double rmse{0.0};

        /** 平均绝对误差。 */
        double mae{0.0};

        /** 最大绝对残差。 */
        double maxAbsResidual{0.0};

        /** 实际使用的角频率（三角函数）。 */
        double angularFrequency{0.0};

        /** 参与拟合的样本数。 */
        int sampleCount{0};

        /** 失败原因或提示（例如「x 必须大于 0」「已自动估计频率」）。 */
        QString message{};

        /** 归一化参数：多项式为 [x̄, σ]；三角函数为 [ω]；指数为 [偏移]；其余为空。 */
        QVector<double> transform{};
    };

    /**
     * @brief       最小二乘曲线拟合器。
     * @details     所有接口均为静态，调用方无需持有实例。
     *
     * @invariant   返回结果的 coefficientNames 与 coefficients 长度一致
     */
    class CurveFitter final
    {
    public:
        /**
         * @brief  执行一次拟合。
         * @param  x 自变量数组。
         * @param  y 因变量数组。
         * @param  count 样本数，至少为 2。
         * @param  request 拟合请求。
         * @return 拟合结果；失败时 ok 为 false 且 message 说明原因。
         */
        [[nodiscard]] static FitResult fit(const double* x,
                                           const double* y,
                                           std::size_t count,
                                           const FitRequest& request);

        /**
         * @brief  在给定自变量上求拟合曲线的值。
         * @param  result 拟合结果。
         * @param  x 自变量数组。
         * @param  count 样本数。
         * @return 拟合值数组；result 无效时返回空数组。
         */
        [[nodiscard]] static QVector<double> evaluate(const FitResult& result,
                                                      const double* x,
                                                      std::size_t count);

        /**
         * @brief  模型的稳定英文键。
         * @param  model 模型族。
         * @return "linear" / "polynomial" / "trigonometric" / "exponential" / "logarithmic"。
         */
        [[nodiscard]] static const char* modelKey(FitModel model) noexcept;

        /**
         * @brief  模型的中文显示名。
         * @param  model 模型族。
         * @return 中文名。
         */
        [[nodiscard]] static QString modelDisplayName(FitModel model);
    };
}

#endif // EGRET_PHYSICS_CURVE_FITTER_H
