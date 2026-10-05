/**
 * @file        curve_fitter.cpp
 * @brief       曲线拟合实现。
 * @details     参数线性模型走列主元 QR 求解（奇异时退化为 SVD 最小范数解）；
 *              参数非线性模型走自实现的 Levenberg–Marquardt 迭代，数值雅可比。
 *              三角函数在频率未知时先用对数均匀频率栅格做粗搜（周期图思想），
 *              再以线性解的频域均方误差最小者为初值。
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

#include "curve_fitter.h"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

namespace egret::analysis
{
    namespace
    {
        /** 频率粗搜的栅格步数。 */
        constexpr int kFrequencyGridSteps{256};

        /** 频率粗搜最多使用的样本数。 */
        constexpr std::size_t kFrequencySearchSamples{1024};

        /** Levenberg–Marquardt 最大迭代次数。 */
        constexpr int kMaxIterations{80};

        /**
         * @brief  求解线性最小二乘。
         * @param  design 设计矩阵。
         * @param  target 目标向量。
         * @param  out 输出系数。
         * @return 求解成功返回 true。
         */
        bool solveLinear(const Eigen::MatrixXd& design, const Eigen::VectorXd& target, Eigen::VectorXd* out)
        {
            if (design.rows() < design.cols() || design.cols() == 0)
            {
                return false;
            }
            const Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr{design};
            if (qr.rank() < design.cols())
            {
                const Eigen::JacobiSVD<Eigen::MatrixXd> svd{
                    design, Eigen::ComputeThinU | Eigen::ComputeThinV};
                *out = svd.solve(target);
            }
            else
            {
                *out = qr.solve(target);
            }
            return out->allFinite();
        }

        /**
         * @brief  计算拟合优度指标。
         * @param  y 实测值。
         * @param  fitted 拟合值。
         * @param  count 样本数。
         * @param  parameterCount 参数个数（用于调整 R²）。
         * @param  result 输出结果，写入 R² / RMSE / MAE / 最大残差。
         */
        void computeMetrics(const double* y,
                            const double* fitted,
                            const std::size_t count,
                            const int parameterCount,
                            FitResult* result)
        {
            double mean = 0.0;
            for (std::size_t i = 0; i < count; ++i)
            {
                mean += y[i];
            }
            mean /= static_cast<double>(count);

            double ssRes = 0.0;
            double ssTot = 0.0;
            double absSum = 0.0;
            double maxAbs = 0.0;
            for (std::size_t i = 0; i < count; ++i)
            {
                const double residual = y[i] - fitted[i];
                ssRes += residual * residual;
                absSum += std::abs(residual);
                maxAbs = std::max(maxAbs, std::abs(residual));
                const double deviation = y[i] - mean;
                ssTot += deviation * deviation;
            }

            const double n = static_cast<double>(count);
            result->rSquared = ssTot > 0.0 ? 1.0 - ssRes / ssTot : 0.0;
            const double denominator = n - static_cast<double>(parameterCount);
            result->adjustedRSquared =
                (ssTot > 0.0 && denominator > 0.0)
                    ? 1.0 - (ssRes / denominator) / (ssTot / (n - 1.0))
                    : result->rSquared;
            result->rmse = std::sqrt(ssRes / n);
            result->mae = absSum / n;
            result->maxAbsResidual = maxAbs;
        }

        /**
         * @brief  构造三角函数设计矩阵。
         * @param  x 自变量数组。
         * @param  count 样本数。
         * @param  omega 角频率。
         * @param  harmonics 谐波数。
         * @return 设计矩阵，列数为 1 + 2 × harmonics。
         */
        Eigen::MatrixXd trigDesign(const double* x,
                                   const std::size_t count,
                                   const double omega,
                                   const int harmonics)
        {
            Eigen::MatrixXd design(static_cast<Eigen::Index>(count),
                                   static_cast<Eigen::Index>(1 + 2 * harmonics));
            for (std::size_t i = 0; i < count; ++i)
            {
                const auto row = static_cast<Eigen::Index>(i);
                design(row, 0) = 1.0;
                for (int k = 1; k <= harmonics; ++k)
                {
                    const double angle = static_cast<double>(k) * omega * x[i];
                    design(row, static_cast<Eigen::Index>(2 * k - 1)) = std::sin(angle);
                    design(row, static_cast<Eigen::Index>(2 * k)) = std::cos(angle);
                }
            }
            return design;
        }

        /**
         * @brief  估计三角函数的主频率。
         * @param  x 自变量数组。
         * @param  y 因变量数组。
         * @param  count 样本数。
         * @param  harmonics 谐波数。
         * @return 估计出的角频率；无法估计时返回 0。
         */
        double estimateOmega(const double* x, const double* y, const std::size_t count, const int harmonics)
        {
            if (count < 4)
            {
                return 0.0;
            }

            // 抽样到不超过 kFrequencySearchSamples 个点，控制粗搜开销
            const std::size_t stride = std::max<std::size_t>(1, count / kFrequencySearchSamples);
            std::vector<double> sampleX{};
            std::vector<double> sampleY{};
            sampleX.reserve(count / stride + 1);
            sampleY.reserve(count / stride + 1);
            for (std::size_t i = 0; i < count; i += stride)
            {
                sampleX.push_back(x[i]);
                sampleY.push_back(y[i]);
            }
            const std::size_t sampleCount = sampleX.size();
            if (sampleCount < 4)
            {
                return 0.0;
            }

            const double span = sampleX.back() - sampleX.front();
            if (!(span > 0.0))
            {
                return 0.0;
            }
            const double step = span / static_cast<double>(sampleCount - 1);
            const double omegaMin = 2.0 * std::numbers::pi / (2.0 * span);
            const double omegaMax = std::numbers::pi / (2.0 * step);
            if (!(omegaMax > omegaMin))
            {
                return omegaMin;
            }

            Eigen::VectorXd target(static_cast<Eigen::Index>(sampleCount));
            for (std::size_t i = 0; i < sampleCount; ++i)
            {
                target(static_cast<Eigen::Index>(i)) = sampleY[i];
            }

            double bestOmega = omegaMin;
            double bestError = std::numeric_limits<double>::max();
            for (int stepIndex = 0; stepIndex < kFrequencyGridSteps; ++stepIndex)
            {
                const double ratio = static_cast<double>(stepIndex) /
                                     static_cast<double>(kFrequencyGridSteps - 1);
                const double omega = omegaMin * std::pow(omegaMax / omegaMin, ratio);
                const Eigen::MatrixXd design = trigDesign(sampleX.data(), sampleCount, omega, harmonics);
                Eigen::VectorXd coefficients{};
                if (!solveLinear(design, target, &coefficients))
                {
                    continue;
                }
                const double error = (target - design * coefficients).squaredNorm();
                if (error < bestError)
                {
                    bestError = error;
                    bestOmega = omega;
                }
            }
            return bestOmega;
        }

        /**
         * @brief       通用 Levenberg–Marquardt 最小二乘。
         * @details     仅依赖 Eigen 稠密求解器，数值雅可比用中心差商。
         * @tparam      ResidualFn 残差函数类型，签名 void(const Eigen::VectorXd&, Eigen::VectorXd&)。
         * @param       parameters 初值（原地更新为解）。
         * @param       residual 残差函数。
         * @return      迭代后代价下降返回 true。
         */
        template <typename ResidualFn>
        bool levenbergMarquardt(Eigen::VectorXd* parameters, ResidualFn residual)
        {
            Eigen::VectorXd current{};
            residual(*parameters, current);
            double cost = current.squaredNorm();

            const Eigen::Index parameterCount = parameters->size();
            double lambda = 1.0e-3;

            for (int iteration = 0; iteration < kMaxIterations; ++iteration)
            {
                Eigen::MatrixXd jacobian(current.size(), parameterCount);
                for (Eigen::Index column = 0; column < parameterCount; ++column)
                {
                    const double step = 1.0e-7 * std::max(1.0, std::abs((*parameters)(column)));
                    Eigen::VectorXd shifted = *parameters;
                    shifted(column) += step;
                    Eigen::VectorXd shiftedResidual{};
                    residual(shifted, shiftedResidual);
                    jacobian.col(column) = (shiftedResidual - current) / step;
                }

                const Eigen::MatrixXd normal = jacobian.transpose() * jacobian;
                const Eigen::VectorXd gradient = jacobian.transpose() * current;

                bool improved = false;
                for (int attempt = 0; attempt < 12; ++attempt)
                {
                    const Eigen::MatrixXd damped =
                        normal + lambda * Eigen::MatrixXd::Identity(parameterCount, parameterCount);
                    const Eigen::VectorXd solution = damped.ldlt().solve(-gradient);
                    if (!solution.allFinite())
                    {
                        lambda *= 10.0;
                        continue;
                    }

                    const Eigen::VectorXd candidate = *parameters + solution;
                    Eigen::VectorXd candidateResidual{};
                    residual(candidate, candidateResidual);
                    const double candidateCost = candidateResidual.squaredNorm();
                    if (candidateCost < cost)
                    {
                        *parameters = candidate;
                        current = candidateResidual;
                        cost = candidateCost;
                        lambda = std::max(1.0e-12, lambda * 0.3);
                        improved = true;
                        break;
                    }
                    lambda *= 10.0;
                }

                if (!improved)
                {
                    break;
                }
            }
            return parameters->allFinite();
        }

        /**
         * @brief  生成形如 "a + b·u + c·u²" 的多项式文本。
         * @param  coefficients 系数。
         * @param  variable 变量名。
         * @return 表达式文本。
         */
        QString polynomialText(const Eigen::VectorXd& coefficients, const QString& variable)
        {
            QString text{};
            for (Eigen::Index index = 0; index < coefficients.size(); ++index)
            {
                const double value = coefficients(index);
                if (index > 0)
                {
                    text += value >= 0.0 ? QStringLiteral(" + ") : QStringLiteral(" - ");
                }
                const double magnitude = index > 0 ? std::abs(value) : value;
                const QString number = QString::number(magnitude, 'g', 6);
                if (index == 0)
                {
                    text += number;
                }
                else if (index == 1)
                {
                    text += number + QStringLiteral("·") + variable;
                }
                else
                {
                    text += number + QStringLiteral("·") + variable + QStringLiteral("^")
                            + QString::number(index);
                }
            }
            return text;
        }
    }

    const char* CurveFitter::modelKey(const FitModel model) noexcept
    {
        switch (model)
        {
        case FitModel::Polynomial:
            return "polynomial";
        case FitModel::Trigonometric:
            return "trigonometric";
        case FitModel::Exponential:
            return "exponential";
        case FitModel::Logarithmic:
            return "logarithmic";
        case FitModel::Linear:
        default:
            return "linear";
        }
    }

    QString CurveFitter::modelDisplayName(const FitModel model)
    {
        switch (model)
        {
        case FitModel::Polynomial:
            return QStringLiteral("多项式");
        case FitModel::Trigonometric:
            return QStringLiteral("三角函数");
        case FitModel::Exponential:
            return QStringLiteral("指数");
        case FitModel::Logarithmic:
            return QStringLiteral("对数");
        case FitModel::Linear:
        default:
            return QStringLiteral("线性");
        }
    }

    FitResult CurveFitter::fit(const double* x,
                               const double* y,
                               const std::size_t count,
                               const FitRequest& request)
    {
        FitResult result{};
        result.model = request.model;
        result.sampleCount = static_cast<int>(count);

        if (x == nullptr || y == nullptr || count < 2)
        {
            result.message = QStringLiteral("样本数不足以拟合（至少需要 2 个点）");
            return result;
        }

        Eigen::VectorXd target(static_cast<Eigen::Index>(count));
        for (std::size_t i = 0; i < count; ++i)
        {
            target(static_cast<Eigen::Index>(i)) = y[i];
        }

        Eigen::VectorXd coefficients{};
        std::vector<double> fitted(count, 0.0);
        int parameterCount = 0;

        switch (request.model)
        {
        case FitModel::Linear:
        {
            Eigen::MatrixXd design(static_cast<Eigen::Index>(count), 2);
            for (std::size_t i = 0; i < count; ++i)
            {
                design(static_cast<Eigen::Index>(i), 0) = x[i];
                design(static_cast<Eigen::Index>(i), 1) = 1.0;
            }
            if (!solveLinear(design, target, &coefficients))
            {
                result.message = QStringLiteral("线性方程组求解失败");
                return result;
            }
            for (std::size_t i = 0; i < count; ++i)
            {
                fitted[i] = coefficients(0) * x[i] + coefficients(1);
            }
            parameterCount = 2;
            result.coefficientNames = {QStringLiteral("a"), QStringLiteral("b")};
            result.equation = QStringLiteral("y = %1·x %2 %3")
                                  .arg(QString::number(coefficients(0), 'g', 6),
                                       coefficients(1) >= 0.0 ? QStringLiteral("+")
                                                              : QStringLiteral("-"),
                                       QString::number(std::abs(coefficients(1)), 'g', 6));
            break;
        }

        case FitModel::Polynomial:
        {
            const int degree = std::clamp(request.polynomialDegree, 1, kMaxPolynomialDegree);
            double mean = 0.0;
            for (std::size_t i = 0; i < count; ++i)
            {
                mean += x[i];
            }
            mean /= static_cast<double>(count);
            double variance = 0.0;
            for (std::size_t i = 0; i < count; ++i)
            {
                variance += (x[i] - mean) * (x[i] - mean);
            }
            double sigma = std::sqrt(variance / static_cast<double>(count));
            if (sigma < 1.0e-12)
            {
                sigma = 1.0;
            }

            Eigen::MatrixXd design(static_cast<Eigen::Index>(count),
                                   static_cast<Eigen::Index>(degree + 1));
            for (std::size_t i = 0; i < count; ++i)
            {
                const double u = (x[i] - mean) / sigma;
                double power = 1.0;
                for (int k = 0; k <= degree; ++k)
                {
                    design(static_cast<Eigen::Index>(i), k) = power;
                    power *= u;
                }
            }
            if (!solveLinear(design, target, &coefficients))
            {
                result.message = QStringLiteral("多项式方程组求解失败");
                return result;
            }
            for (std::size_t i = 0; i < count; ++i)
            {
                const double u = (x[i] - mean) / sigma;
                double value = 0.0;
                double power = 1.0;
                for (Eigen::Index k = 0; k < coefficients.size(); ++k)
                {
                    value += coefficients(k) * power;
                    power *= u;
                }
                fitted[i] = value;
            }
            parameterCount = degree + 1;
            for (int k = 0; k <= degree; ++k)
            {
                result.coefficientNames.append(QStringLiteral("a%1").arg(k));
            }
            result.transform = {mean, sigma};
            result.equation = QStringLiteral("y = %1，u = (x - %2) / %3")
                                  .arg(polynomialText(coefficients, QStringLiteral("u")),
                                       QString::number(mean, 'g', 6),
                                       QString::number(sigma, 'g', 6));
            break;
        }

        case FitModel::Trigonometric:
        {
            const int harmonics = std::clamp(request.harmonicCount, 1, kMaxHarmonics);
            double omega = request.angularFrequency;
            if (!(omega > 0.0))
            {
                omega = estimateOmega(x, y, count, harmonics);
                if (!(omega > 0.0))
                {
                    result.message = QStringLiteral("无法估计三角函数频率，请手动指定角频率");
                    return result;
                }
                result.message = QStringLiteral("已自动估计角频率 ω = %1")
                                     .arg(QString::number(omega, 'g', 6));
            }

            // ω 固定时为线性模型；这里对 ω 再做一次一维精修，提升拟合精度
            const auto solveForOmega = [&](const double candidateOmega, Eigen::VectorXd* out) -> bool
            {
                const Eigen::MatrixXd design = trigDesign(x, count, candidateOmega, harmonics);
                return solveLinear(design, target, out);
            };

            if (!solveForOmega(omega, &coefficients))
            {
                result.message = QStringLiteral("三角函数方程组求解失败");
                return result;
            }

            double bestOmega = omega;
            double bestError = std::numeric_limits<double>::max();
            {
                const Eigen::MatrixXd design = trigDesign(x, count, bestOmega, harmonics);
                bestError = (target - design * coefficients).squaredNorm();
            }
            double bracket = std::max(1.0e-6, bestOmega * 0.05);
            for (int refine = 0; refine < 12; ++refine)
            {
                bool improved = false;
                for (const double sign : {-1.0, 1.0})
                {
                    const double candidateOmega = bestOmega + sign * bracket;
                    if (!(candidateOmega > 0.0))
                    {
                        continue;
                    }
                    Eigen::VectorXd candidateCoefficients{};
                    if (!solveForOmega(candidateOmega, &candidateCoefficients))
                    {
                        continue;
                    }
                    const Eigen::MatrixXd design =
                        trigDesign(x, count, candidateOmega, harmonics);
                    const double error =
                        (target - design * candidateCoefficients).squaredNorm();
                    if (error < bestError)
                    {
                        bestError = error;
                        bestOmega = candidateOmega;
                        coefficients = candidateCoefficients;
                        improved = true;
                    }
                }
                if (!improved)
                {
                    bracket *= 0.5;
                }
            }
            omega = bestOmega;
            result.angularFrequency = omega;
            result.transform = {omega};

            const Eigen::MatrixXd design = trigDesign(x, count, omega, harmonics);
            const Eigen::VectorXd values = design * coefficients;
            for (std::size_t i = 0; i < count; ++i)
            {
                fitted[i] = values(static_cast<Eigen::Index>(i));
            }
            parameterCount = 1 + 2 * harmonics;

            QString text = QStringLiteral("y = %1").arg(QString::number(coefficients(0), 'g', 6));
            result.coefficientNames.append(QStringLiteral("a0"));
            for (int k = 1; k <= harmonics; ++k)
            {
                const double a = coefficients(2 * k - 1);
                const double b = coefficients(2 * k);
                text += QStringLiteral(" %1 %2·sin(%3ωx) %4 %5·cos(%6ωx)")
                            .arg(a >= 0.0 ? QStringLiteral("+") : QStringLiteral("-"),
                                 QString::number(std::abs(a), 'g', 6))
                            .arg(k)
                            .arg(b >= 0.0 ? QStringLiteral("+") : QStringLiteral("-"))
                            .arg(QString::number(std::abs(b), 'g', 6))
                            .arg(k);
                result.coefficientNames.append(QStringLiteral("a%1").arg(k));
                result.coefficientNames.append(QStringLiteral("b%1").arg(k));
            }
            result.equation = text + QStringLiteral("，ω = %1").arg(QString::number(omega, 'g', 6));
            break;
        }

        case FitModel::Exponential:
        {
            bool allPositive = true;
            for (std::size_t i = 0; i < count; ++i)
            {
                if (!(y[i] > 0.0))
                {
                    allPositive = false;
                    break;
                }
            }

            if (!request.exponentialWithOffset)
            {
                if (!allPositive)
                {
                    result.message = QStringLiteral("指数拟合要求全部 y > 0（或改用带偏移的指数模型）");
                    return result;
                }
                Eigen::VectorXd logTarget(static_cast<Eigen::Index>(count));
                for (std::size_t i = 0; i < count; ++i)
                {
                    logTarget(static_cast<Eigen::Index>(i)) = std::log(y[i]);
                }
                Eigen::MatrixXd design(static_cast<Eigen::Index>(count), 2);
                for (std::size_t i = 0; i < count; ++i)
                {
                    design(static_cast<Eigen::Index>(i), 0) = x[i];
                    design(static_cast<Eigen::Index>(i), 1) = 1.0;
                }
                Eigen::VectorXd lineCoefficients{};
                if (!solveLinear(design, logTarget, &lineCoefficients))
                {
                    result.message = QStringLiteral("指数拟合的对数线性化失败");
                    return result;
                }
                coefficients.resize(2);
                coefficients(0) = std::exp(lineCoefficients(1)); // a
                coefficients(1) = lineCoefficients(0);           // b
                for (std::size_t i = 0; i < count; ++i)
                {
                    fitted[i] = coefficients(0) * std::exp(coefficients(1) * x[i]);
                }
                parameterCount = 2;
                result.coefficientNames = {QStringLiteral("a"), QStringLiteral("b")};
                result.equation = QStringLiteral("y = %1·exp(%2·x)")
                                      .arg(QString::number(coefficients(0), 'g', 6),
                                           QString::number(coefficients(1), 'g', 6));
            }
            else
            {
                // 带偏移：y = a·exp(b x) + c，用 LM 迭代
                Eigen::VectorXd parameters(3);
                parameters(0) = allPositive ? std::exp(std::log(y[0])) : 1.0;
                parameters(1) = -1.0;
                parameters(2) = 0.0;
                if (allPositive)
                {
                    Eigen::VectorXd logTarget(static_cast<Eigen::Index>(count));
                    for (std::size_t i = 0; i < count; ++i)
                    {
                        logTarget(static_cast<Eigen::Index>(i)) = std::log(y[i]);
                    }
                    Eigen::MatrixXd design(static_cast<Eigen::Index>(count), 2);
                    for (std::size_t i = 0; i < count; ++i)
                    {
                        design(static_cast<Eigen::Index>(i), 0) = x[i];
                        design(static_cast<Eigen::Index>(i), 1) = 1.0;
                    }
                    Eigen::VectorXd lineCoefficients{};
                    if (solveLinear(design, logTarget, &lineCoefficients))
                    {
                        parameters(0) = std::exp(lineCoefficients(1));
                        parameters(1) = lineCoefficients(0);
                    }
                }

                const auto residual = [x, count, &target](const Eigen::VectorXd& p,
                                                           Eigen::VectorXd& out)
                {
                    out.resize(target.size());
                    for (std::size_t i = 0; i < count; ++i)
                    {
                        const auto index = static_cast<Eigen::Index>(i);
                        out(index) = p(0) * std::exp(p(1) * x[i]) + p(2) - target(index);
                    }
                };

                if (!levenbergMarquardt(&parameters, residual))
                {
                    result.message = QStringLiteral("带偏移指数模型的迭代未收敛");
                    return result;
                }

                coefficients.resize(3);
                coefficients(0) = parameters(0);
                coefficients(1) = parameters(1);
                coefficients(2) = parameters(2);
                for (std::size_t i = 0; i < count; ++i)
                {
                    fitted[i] = coefficients(0) * std::exp(coefficients(1) * x[i]) + coefficients(2);
                }
                parameterCount = 3;
                result.coefficientNames = {QStringLiteral("a"), QStringLiteral("b"),
                                           QStringLiteral("c")};
                result.equation = QStringLiteral("y = %1·exp(%2·x) %3 %4")
                                      .arg(QString::number(coefficients(0), 'g', 6),
                                           QString::number(coefficients(1), 'g', 6),
                                           coefficients(2) >= 0.0 ? QStringLiteral("+")
                                                                  : QStringLiteral("-"),
                                           QString::number(std::abs(coefficients(2)), 'g', 6));
            }
            break;
        }

        case FitModel::Logarithmic:
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                if (!(x[i] > 0.0))
                {
                    result.message = QStringLiteral("对数拟合要求全部 x > 0");
                    return result;
                }
            }
            Eigen::MatrixXd design(static_cast<Eigen::Index>(count), 2);
            for (std::size_t i = 0; i < count; ++i)
            {
                design(static_cast<Eigen::Index>(i), 0) = std::log(x[i]);
                design(static_cast<Eigen::Index>(i), 1) = 1.0;
            }
            if (!solveLinear(design, target, &coefficients))
            {
                result.message = QStringLiteral("对数方程组求解失败");
                return result;
            }
            for (std::size_t i = 0; i < count; ++i)
            {
                fitted[i] = coefficients(0) * std::log(x[i]) + coefficients(1);
            }
            parameterCount = 2;
            result.coefficientNames = {QStringLiteral("a"), QStringLiteral("b")};
            result.equation = QStringLiteral("y = %1·ln(x) %2 %3")
                                  .arg(QString::number(coefficients(0), 'g', 6),
                                       coefficients(1) >= 0.0 ? QStringLiteral("+")
                                                              : QStringLiteral("-"),
                                       QString::number(std::abs(coefficients(1)), 'g', 6));
            break;
        }
        }

        result.coefficients.reserve(static_cast<qsizetype>(coefficients.size()));
        for (Eigen::Index index = 0; index < coefficients.size(); ++index)
        {
            result.coefficients.append(coefficients(index));
        }
        computeMetrics(y, fitted.data(), count, parameterCount, &result);
        result.ok = true;
        return result;
    }

    QVector<double> CurveFitter::evaluate(const FitResult& result,
                                          const double* x,
                                          std::size_t count)
    {
        QVector<double> values{};
        if (!result.ok || x == nullptr || count == 0)
        {
            return values;
        }
        values.reserve(static_cast<qsizetype>(count));

        const QVector<double>& c = result.coefficients;
        switch (result.model)
        {
        case FitModel::Linear:
            for (std::size_t i = 0; i < count; ++i)
            {
                values.append(c[0] * x[i] + c[1]);
            }
            break;
        case FitModel::Polynomial:
        {
            const double mean = result.transform.value(0, 0.0);
            const double sigma = result.transform.value(1, 1.0);
            for (std::size_t i = 0; i < count; ++i)
            {
                const double u = (x[i] - mean) / sigma;
                double value = 0.0;
                double power = 1.0;
                for (const double coefficient : c)
                {
                    value += coefficient * power;
                    power *= u;
                }
                values.append(value);
            }
            break;
        }
        case FitModel::Trigonometric:
        {
            const double omega = result.angularFrequency;
            const int harmonics = (c.size() - 1) / 2;
            for (std::size_t i = 0; i < count; ++i)
            {
                double value = c[0];
                for (int k = 1; k <= harmonics; ++k)
                {
                    const double angle = static_cast<double>(k) * omega * x[i];
                    value += c[2 * k - 1] * std::sin(angle) + c[2 * k] * std::cos(angle);
                }
                values.append(value);
            }
            break;
        }
        case FitModel::Exponential:
            for (std::size_t i = 0; i < count; ++i)
            {
                const double value = c[0] * std::exp(c[1] * x[i]);
                values.append(c.size() > 2 ? value + c[2] : value);
            }
            break;
        case FitModel::Logarithmic:
            for (std::size_t i = 0; i < count; ++i)
            {
                values.append(c[0] * std::log(x[i]) + c[1]);
            }
            break;
        }
        return values;
    }
}
