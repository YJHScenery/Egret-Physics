/**
 * @file        expression_evaluator.cpp
 * @brief       理论方程求值与误差对比实现（muparser）。
 * @details     muparser 的变量通过指针绑定，因此这里把自变量与常量放在生命周期覆盖
 *              整个求值过程的对象里：常量放在 std::map（节点地址稳定），自变量放在
 *              栈上并在循环内更新，保证绑定指针始终有效。
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

#include "expression_evaluator.h"

#include <muParser.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace egret::analysis
{
    namespace
    {
        /**
         * @brief  把 Qt 字符串转成 muparser 使用的字符串类型。
         * @param  text 文本。
         * @return muparser 字符串（Windows 下 Qt 以 _UNICODE 构建，因此为 std::wstring）。
         */
        mu::string_type toParserString(const QString& text)
        {
#if defined(_UNICODE)
            return text.toStdWString();
#else
            return text.toStdString();
#endif
        }

        /**
         * @brief  把 muparser 字符串转成 Qt 字符串。
         * @param  text muparser 字符串。
         * @return Qt 文本。
         */
        QString fromParserString(const mu::string_type& text)
        {
#if defined(_UNICODE)
            return QString::fromStdWString(text);
#else
            return QString::fromStdString(text);
#endif
        }

        /**
         * @brief  把 muparser 异常翻译成中文可读信息。
         * @param  error muparser 异常。
         * @return 形如「位置 7：未定义的变量」的描述。
         */
        QString describeParserError(const mu::ParserError& error)
        {
            const QString message = fromParserString(error.GetMsg());
            const int position = error.GetPos();
            if (position >= 0)
            {
                return QStringLiteral("表达式位置 %1：%2").arg(position).arg(message);
            }
            return message;
        }

        /**
         * @brief       表达式求值器外壳。
         * @details     构造函数完成变量绑定与表达式编译；Evaluate() 在给定自变量值上求值。
         */
        class ScopedParser
        {
        public:
            /**
             * @brief  构造并编译表达式。
             * @param  expression 表达式文本。
             * @param  variableName 自变量名。
             * @param  variables 常量表。
             * @param  errorMessage 失败原因。
             */
            ScopedParser(const QString& expression,
                         const QString& variableName,
                         const QMap<QString, double>& variables,
                         QString* errorMessage)
            {
                for (auto it = variables.constBegin(); it != variables.constEnd(); ++it)
                {
                    const mu::string_type name = toParserString(it.key());
                    m_constants.emplace(name, it.value());
                }
                for (auto& entry : m_constants)
                {
                    m_parser.DefineVar(entry.first, &entry.second);
                }

                m_variableName = toParserString(variableName);
                if (!m_variableName.empty())
                {
                    m_parser.DefineVar(m_variableName, &m_variable);
                }

                try
                {
                    m_parser.SetExpr(toParserString(expression));
                    // 触发一次编译，把语法错误提前暴露
                    m_parser.Eval();
                    m_valid = true;
                }
                catch (mu::ParserError& error)
                {
                    m_valid = false;
                    if (errorMessage != nullptr)
                    {
                        *errorMessage = describeParserError(error);
                    }
                }
            }

            /** 表达式是否编译成功。 */
            [[nodiscard]] bool isValid() const noexcept
            {
                return m_valid;
            }

            /**
             * @brief  在给定自变量上求值。
             * @param  variableValue 自变量值。
             * @param  out 输出结果。
             * @return 成功返回 true；求值异常时写入 errorMessage。
             */
            bool evaluate(const double variableValue, double* out, QString* errorMessage)
            {
                m_variable = variableValue;
                try
                {
                    *out = m_parser.Eval();
                    if (!std::isfinite(*out))
                    {
                        if (errorMessage != nullptr)
                        {
                            *errorMessage = QStringLiteral("表达式在自变量 x = %1 处得到非有限值")
                                                .arg(QString::number(variableValue, 'g', 6));
                        }
                        return false;
                    }
                    return true;
                }
                catch (mu::ParserError& error)
                {
                    if (errorMessage != nullptr)
                    {
                        *errorMessage = describeParserError(error);
                    }
                    return false;
                }
            }

        private:
            /** muparser 解析器。 */
            mu::Parser m_parser{};

            /** 常量表（std::map 保证节点地址稳定）。 */
            std::map<mu::string_type, double> m_constants{};

            /** 自变量名。 */
            mu::string_type m_variableName{};

            /** 自变量当前值。 */
            double m_variable{0.0};

            /** 是否编译成功。 */
            bool m_valid{false};
        };
    }

    bool ExpressionEvaluator::validate(const QString& expression,
                                       const QString& variableName,
                                       const QMap<QString, double>& variables,
                                       QString* errorMessage)
    {
        if (expression.trimmed().isEmpty())
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("理论方程不能为空");
            }
            return false;
        }
        ScopedParser parser{expression, variableName, variables, errorMessage};
        return parser.isValid();
    }

    bool ExpressionEvaluator::evaluateOnGrid(const double* x,
                                             const std::size_t count,
                                             const QString& expression,
                                             const QString& variableName,
                                             const QMap<QString, double>& variables,
                                             QVector<double>* out,
                                             QString* errorMessage)
    {
        if (out == nullptr || x == nullptr || count == 0)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("没有可求值的样本");
            }
            return false;
        }

        ScopedParser parser{expression, variableName, variables, errorMessage};
        if (!parser.isValid())
        {
            return false;
        }

        out->clear();
        out->reserve(static_cast<qsizetype>(count));
        for (std::size_t i = 0; i < count; ++i)
        {
            double value = 0.0;
            if (!parser.evaluate(x[i], &value, errorMessage))
            {
                out->clear();
                return false;
            }
            out->append(value);
        }
        return true;
    }

    ComparisonResult ExpressionEvaluator::compare(const double* x,
                                                  const double* measured,
                                                  const std::size_t count,
                                                  const QString& expression,
                                                  const QString& variableName,
                                                  const QMap<QString, double>& variables)
    {
        ComparisonResult result{};
        if (x == nullptr || measured == nullptr || count == 0)
        {
            result.message = QStringLiteral("没有可对比的样本");
            return result;
        }

        QVector<double> predicted{};
        if (!evaluateOnGrid(x, count, expression, variableName, variables, &predicted, &result.message))
        {
            return result;
        }

        result.sampleCount = static_cast<int>(count);
        result.predicted = predicted;
        result.residual.reserve(static_cast<qsizetype>(count));

        double sumMeasured = 0.0;
        for (std::size_t i = 0; i < count; ++i)
        {
            sumMeasured += measured[i];
        }
        const double mean = sumMeasured / static_cast<double>(count);

        double minMeasured = measured[0];
        double maxMeasured = measured[0];
        double ssRes = 0.0;
        double ssTot = 0.0;
        double absSum = 0.0;
        double maxAbs = 0.0;
        double relativeSum = 0.0;
        double maxRelative = 0.0;
        int relativeCount = 0;

        for (std::size_t i = 0; i < count; ++i)
        {
            const double residual = predicted[static_cast<qsizetype>(i)] - measured[i];
            result.residual.append(residual);

            minMeasured = std::min(minMeasured, measured[i]);
            maxMeasured = std::max(maxMeasured, measured[i]);

            ssRes += residual * residual;
            const double deviation = measured[i] - mean;
            ssTot += deviation * deviation;
            const double absolute = std::abs(residual);
            absSum += absolute;
            maxAbs = std::max(maxAbs, absolute);

            if (std::abs(measured[i]) > kRelativeErrorFloor)
            {
                const double relative = absolute / std::abs(measured[i]);
                relativeSum += relative;
                maxRelative = std::max(maxRelative, relative);
                ++relativeCount;
            }
        }

        const double n = static_cast<double>(count);
        result.rSquared = ssTot > 0.0 ? 1.0 - ssRes / ssTot : 0.0;
        result.rmse = std::sqrt(ssRes / n);
        result.mae = absSum / n;
        result.maxAbsError = maxAbs;
        const double range = maxMeasured - minMeasured;
        result.nrmse = range > 0.0 ? result.rmse / range : 0.0;
        result.meanRelativeError = relativeCount > 0
                                       ? relativeSum / static_cast<double>(relativeCount)
                                       : 0.0;
        result.maxRelativeError = maxRelative;
        result.ok = true;
        return result;
    }
}
