/**
 * @file        analysis_exporter.cpp
 * @brief       xlsx / CSV 导出实现。
 * @details     列名沿用实时仿真器追踪导出的命名习惯（中文名 + 斜杠 + 单位），
 *              使「数据分析」导出的表格与「实时仿真」导出的表格列结构一致。
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

#include "analysis_exporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <chrono>

#include <xlsxdocument.h>
#include <xlsxformat.h>

namespace egret::analysis
{
    namespace
    {
        /** 进度与取消检查的行间隔。 */
        constexpr std::uint64_t kProgressInterval{4096};

        /** 表格列宽。 */
        constexpr double kColumnWidth{15.0};

        /** 单个实体的导出计划。 */
        struct EntityPlan
        {
            /** 实体描述。 */
            EntityDescriptor descriptor{};

            /** 该实体对应的序列，按 Channel 枚举顺序排列。 */
            QVector<const ExtractedSeries*> series{};
        };

        /**
         * @brief  生成列标题。
         * @param  channel 通道。
         * @return 形如「位置-x/m」的标题；无量纲通道不带斜杠。
         */
        QString channelHeader(const Channel channel)
        {
            const ChannelInfo& info = channelInfo(channel);
            const QString name = QString::fromUtf8(info.displayName);
            const QString unit = QString::fromUtf8(info.unit);
            return unit.isEmpty() ? name : name + QStringLiteral("/") + unit;
        }

        /**
         * @brief  把实体名规范成合法的工作表名。
         * @param  name 实体名。
         * @param  used 已用名称集合，函数会把最终名称写入其中以保证唯一。
         * @return 合法且唯一的工作表名。
         */
        QString uniqueSheetName(const QString& name, QSet<QString>& used)
        {
            QString sanitized = name;
            for (const QChar forbidden : QStringLiteral("[]:*?/\\"))
            {
                sanitized.remove(forbidden);
            }
            sanitized = sanitized.trimmed();
            if (sanitized.isEmpty())
            {
                sanitized = QStringLiteral("实体");
            }
            if (sanitized.size() > 28)
            {
                sanitized = sanitized.left(28);
            }

            QString candidate = sanitized;
            int suffix = 2;
            while (used.contains(candidate) || candidate.compare(QStringLiteral("Sheet1"), Qt::CaseInsensitive) == 0)
            {
                candidate = QStringLiteral("%1_%2").arg(sanitized).arg(suffix);
                ++suffix;
            }
            used.insert(candidate);
            return candidate;
        }

        /**
         * @brief  生成 CSV 单元格文本（必要时加引号）。
         * @param  text 原始文本。
         * @return 可直接写入 CSV 的文本。
         */
        QString csvField(const QString& text)
        {
            if (!text.contains(QLatin1Char(',')) && !text.contains(QLatin1Char('"')) &&
                !text.contains(QLatin1Char('\n')) && !text.contains(QLatin1Char('\r')))
            {
                return text;
            }
            QString escaped = text;
            escaped.replace(QLatin1String("\""), QLatin1String("\"\""));
            return QLatin1Char('"') + escaped + QLatin1Char('"');
        }

        /**
         * @brief  把 double 转成高精度文本。
         * @param  value 数值。
         * @return 至多 17 位有效数字的文本。
         */
        QString numberText(const double value)
        {
            return QString::number(value, 'g', 17);
        }

        /**
         * @brief  按实体整理导出计划。
         * @param  result 抽取结果。
         * @return 导出计划列表。
         */
        QVector<EntityPlan> buildPlans(const ExtractionResult& result)
        {
            QVector<EntityPlan> plans{};
            plans.reserve(result.entities.size());
            for (const EntityDescriptor& descriptor : result.entities)
            {
                EntityPlan plan{};
                plan.descriptor = descriptor;
                for (const ExtractedSeries& series : result.series)
                {
                    if (series.entity == descriptor.ref)
                    {
                        plan.series.append(&series);
                    }
                }
                std::sort(plan.series.begin(),
                          plan.series.end(),
                          [](const ExtractedSeries* lhs, const ExtractedSeries* rhs)
                          {
                              return static_cast<std::uint16_t>(lhs->channel) <
                                     static_cast<std::uint16_t>(rhs->channel);
                          });
                if (!plan.series.isEmpty())
                {
                    plans.append(plan);
                }
            }
            return plans;
        }

        /**
         * @brief  计算导出所需的单元格数。
         * @param  result 抽取结果。
         * @param  plans 导出计划。
         * @return 单元格总数（行包含表头）。
         */
        std::uint64_t countCells(const ExtractionResult& result, const QVector<EntityPlan>& plans)
        {
            std::uint64_t cells = 0;
            for (const EntityPlan& plan : plans)
            {
                const std::uint64_t columns = static_cast<std::uint64_t>(plan.series.size()) + 1;
                cells += (result.sampledCount + 1) * columns;
            }
            return cells;
        }

        /**
         * @brief  写出 xlsx。
         * @param  result 抽取结果。
         * @param  plans 导出计划。
         * @param  options 导出参数。
         * @param  callbacks 进度与取消回调。
         * @param  report 导出报告。
         * @param  errorMessage 失败原因。
         * @return 成功返回 true。
         */
        bool writeXlsx(const ExtractionResult& result,
                       const QVector<EntityPlan>& plans,
                       const ExportOptions& options,
                       const ExtractionCallbacks& callbacks,
                       ExportReport* report,
                       QString* errorMessage)
        {
            QXlsx::Document xlsx{};
            QXlsx::Format titleFormat{};
            titleFormat.setFontBold(true);

            const std::uint64_t totalRows =
                static_cast<std::uint64_t>(plans.size()) * result.sampledCount;
            std::uint64_t doneRows = 0;
            QSet<QString> usedNames{};

            bool firstSheet = true;
            for (const EntityPlan& plan : plans)
            {
                const QString sheetName = uniqueSheetName(plan.descriptor.name, usedNames);
                if (firstSheet)
                {
                    const QString defaultName = xlsx.sheetNames().value(0);
                    if (defaultName.isEmpty() || !xlsx.renameSheet(defaultName, sheetName))
                    {
                        xlsx.addSheet(sheetName);
                    }
                    firstSheet = false;
                }
                else
                {
                    xlsx.addSheet(sheetName);
                }
                xlsx.selectSheet(sheetName);

                int column = 0;
                xlsx.write(1, ++column, QStringLiteral("时间"), titleFormat);
                xlsx.setColumnWidth(column, kColumnWidth);
                for (const ExtractedSeries* series : plan.series)
                {
                    xlsx.write(1, ++column, channelHeader(series->channel), titleFormat);
                    xlsx.setColumnWidth(column, kColumnWidth);
                }

                for (std::uint64_t index = 0; index < result.sampledCount; ++index)
                {
                    if (callbacks.cancelled && callbacks.cancelled())
                    {
                        if (report != nullptr)
                        {
                            report->cancelled = true;
                        }
                        break;
                    }

                    const int row = static_cast<int>(index) + 2;
                    column = 0;
                    xlsx.write(row, ++column, result.time[static_cast<std::size_t>(index)]);
                    for (const ExtractedSeries* series : plan.series)
                    {
                        const std::size_t sampleIndex = static_cast<std::size_t>(index);
                        const double value = sampleIndex < series->values.size()
                                                 ? series->values[sampleIndex]
                                                 : 0.0;
                        xlsx.write(row, ++column, value);
                    }

                    ++doneRows;
                    if (callbacks.progress && (doneRows % kProgressInterval == 0))
                    {
                        callbacks.progress(doneRows, totalRows);
                    }
                    if (report != nullptr && report->cancelled)
                    {
                        break;
                    }
                }
                if (report != nullptr && report->cancelled)
                {
                    break;
                }
            }

            if (options.writeMetadataSheet && !(report != nullptr && report->cancelled))
            {
                const QString metadataName = uniqueSheetName(QStringLiteral("元数据"), usedNames);
                xlsx.addSheet(metadataName);
                xlsx.selectSheet(metadataName);
                int row = 0;
                const auto put = [&xlsx, &row](const QString& key, const QString& value)
                {
                    ++row;
                    xlsx.write(row, 1, key);
                    xlsx.write(row, 2, value);
                };
                put(QStringLiteral("源文件"), result.sourcePath);
                put(QStringLiteral("生成者"), result.generator);
                put(QStringLiteral("总帧数"), QString::number(result.totalFrameCount));
                put(QStringLiteral("样本数"), QString::number(result.sampledCount));
                put(QStringLiteral("抽帧比"), QString::number(result.stride));
                put(QStringLiteral("采样间隔/ms"),
                    numberText(static_cast<double>(result.sampleIntervalNs) / 1.0e6));
            }

            if (!xlsx.saveAs(options.targetPath))
            {
                if (errorMessage != nullptr)
                {
                    *errorMessage = QStringLiteral("写入 xlsx 失败：%1").arg(options.targetPath);
                }
                return false;
            }

            if (report != nullptr)
            {
                report->writtenFiles.append(options.targetPath);
                report->fileCount += 1;
            }
            return true;
        }

        /**
         * @brief  写出 CSV（每实体一个文件）。
         * @param  result 抽取结果。
         * @param  plans 导出计划。
         * @param  options 导出参数。
         * @param  callbacks 进度与取消回调。
         * @param  report 导出报告。
         * @param  errorMessage 失败原因。
         * @return 成功返回 true。
         */
        bool writeCsv(const ExtractionResult& result,
                      const QVector<EntityPlan>& plans,
                      const ExportOptions& options,
                      const ExtractionCallbacks& callbacks,
                      ExportReport* report,
                      QString* errorMessage)
        {
            const QFileInfo targetInfo{options.targetPath};
            QDir directory = targetInfo.absoluteDir();
            const QString baseName = targetInfo.completeBaseName().isEmpty()
                                         ? QStringLiteral("analysis")
                                         : targetInfo.completeBaseName();
            const bool singleFile = plans.size() == 1;

            const std::uint64_t totalRows =
                static_cast<std::uint64_t>(plans.size()) * result.sampledCount;
            std::uint64_t doneRows = 0;

            for (const EntityPlan& plan : plans)
            {
                QString filePath = options.targetPath;
                if (!singleFile)
                {
                    QSet<QString> unused{};
                    const QString suffix = uniqueSheetName(plan.descriptor.name, unused);
                    filePath = directory.filePath(
                        QStringLiteral("%1_%2.csv").arg(baseName, suffix));
                }

                QFile file{filePath};
                if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                {
                    if (errorMessage != nullptr)
                    {
                        *errorMessage = QStringLiteral("无法写入文件：%1").arg(filePath);
                    }
                    return false;
                }

                QTextStream stream{&file};
                stream.setEncoding(QStringConverter::Utf8);
                // 带 BOM，保证 Excel 以 UTF-8 打开中文列名
                stream << QStringLiteral("\uFEFF");
                stream << QStringLiteral("时间");
                for (const ExtractedSeries* series : plan.series)
                {
                    stream << QLatin1Char(',') << csvField(channelHeader(series->channel));
                }
                stream << QLatin1String("\r\n");

                for (std::uint64_t index = 0; index < result.sampledCount; ++index)
                {
                    if (callbacks.cancelled && callbacks.cancelled())
                    {
                        if (report != nullptr)
                        {
                            report->cancelled = true;
                        }
                        break;
                    }

                    stream << numberText(result.time[static_cast<std::size_t>(index)]);
                    for (const ExtractedSeries* series : plan.series)
                    {
                        stream << QLatin1Char(',')
                               << numberText(series->values[static_cast<std::size_t>(index)]);
                    }
                    stream << QLatin1String("\r\n");

                    ++doneRows;
                    if (callbacks.progress && (doneRows % kProgressInterval == 0))
                    {
                        callbacks.progress(doneRows, totalRows);
                    }
                    if (report != nullptr && report->cancelled)
                    {
                        break;
                    }
                }

                stream.flush();
                file.close();

                if (report != nullptr)
                {
                    report->writtenFiles.append(filePath);
                    report->fileCount += 1;
                }
                if (report != nullptr && report->cancelled)
                {
                    break;
                }
            }
            return true;
        }
    }

    const char* AnalysisExporter::formatKey(const ExportFormat format) noexcept
    {
        return format == ExportFormat::Csv ? "csv" : "xlsx";
    }

    bool AnalysisExporter::exportResult(const ExtractionResult& result,
                                        const ExportOptions& options,
                                        const ExtractionCallbacks& callbacks,
                                        ExportReport* report,
                                        QString* errorMessage)
    {
        const auto startedAt = std::chrono::steady_clock::now();
        if (report != nullptr)
        {
            *report = ExportReport{};
        }

        const auto fail = [errorMessage](const QString& message)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = message;
            }
            return false;
        };

        if (options.targetPath.trimmed().isEmpty())
        {
            return fail(QStringLiteral("未指定导出文件路径"));
        }
        if (result.sampledCount == 0 || result.time.empty())
        {
            return fail(QStringLiteral("没有可导出的数据，请先抽取序列"));
        }

        const QVector<EntityPlan> plans = buildPlans(result);
        if (plans.isEmpty())
        {
            return fail(QStringLiteral("没有可导出的序列"));
        }

        const std::uint64_t cells = countCells(result, plans);
        if (options.format == ExportFormat::Xlsx && cells > options.maxCells)
        {
            return fail(QStringLiteral("数据量约 %1 个单元格，超过 xlsx 上限 %2；"
                                       "请改用 CSV 逐行导出")
                            .arg(cells)
                            .arg(options.maxCells));
        }

        bool ok = false;
        if (options.format == ExportFormat::Xlsx)
        {
            ok = writeXlsx(result, plans, options, callbacks, report, errorMessage);
        }
        else
        {
            ok = writeCsv(result, plans, options, callbacks, report, errorMessage);
        }
        if (!ok)
        {
            return false;
        }

        if (report != nullptr)
        {
            report->cells = cells;
            report->rows = result.sampledCount;
            report->elapsedSeconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - startedAt).count();
        }
        if (callbacks.progress)
        {
            callbacks.progress(cells, cells);
        }
        return true;
    }
}
