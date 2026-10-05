/**
 * @file        analysis_session.cpp
 * @brief       数据分析工作区 QML 门面实现。
 * @details     抽取与导出都在 QtConcurrent 线程池中执行：工作线程只做纯计算，
 *              结果通过 QMetaObject::invokeMethod 回到 UI 线程再写入成员并发出信号，
 *              因此 QML 绑定与属性通知始终发生在 UI 线程。
 *
 *              取消用 shared_ptr<atomic<bool>> 传递：即使控制器先于任务销毁，
 *              工作线程也不会访问悬空对象（沿用 SimulationExportController 的写法）。
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

#include "analysis_session.h"

#include "analysis_exporter.h"
#include "epsx_reader.h"
#include "epsx_series_extractor.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointF>
#include <QtConcurrent>

#include <cstddef>
#include <tuple>
#include <utility>

namespace egret::analysis
{
    namespace
    {
        /** 单条曲线默认最大绘图点数。 */
        constexpr int kDefaultMaxPoints{4000};

        /** 表示「纳入全部约束」的伪实体键。 */
        constexpr auto kAllConstraintsKey{"*constraint"};

        /** 表示「纳入全部力场」的伪实体键。 */
        constexpr auto kAllFieldsKey{"*field"};

        /**
         * @brief  由通道键反查通道。
         * @param  key 通道键。
         * @return 命中返回值，未命中返回 std::nullopt。
         */
        std::optional<Channel> channelFromKey(const QString& key)
        {
            for (std::size_t index = 0; index < kChannelCount; ++index)
            {
                const auto channel = static_cast<Channel>(index);
                if (key == QLatin1String(channelInfo(channel).key))
                {
                    return channel;
                }
            }
            return std::nullopt;
        }

        /**
         * @brief  在抽取结果中查找实体显示名。
         * @param  result 抽取结果。
         * @param  ref 实体引用。
         * @return 实体名；找不到时返回「类别 #ID」。
         */
        QString displayNameOf(const ExtractionResult& result, const EntityRef& ref)
        {
            for (const EntityDescriptor& descriptor : result.entities)
            {
                if (descriptor.ref == ref)
                {
                    return descriptor.name;
                }
            }
            return QStringLiteral("%1 #%2")
                .arg(QString::fromUtf8(entityKindDisplayName(ref.kind)))
                .arg(ref.id);
        }
    }

    AnalysisSession::AnalysisSession(QObject* parent)
        : QObject(parent)
    {
    }

    AnalysisSession::~AnalysisSession() = default;

    bool AnalysisSession::isBusy() const
    {
        return m_busy;
    }

    double AnalysisSession::progress() const
    {
        return m_progress;
    }

    QString AnalysisSession::statusMessage() const
    {
        return m_statusMessage;
    }

    QString AnalysisSession::errorMessage() const
    {
        return m_errorMessage;
    }

    QString AnalysisSession::sourcePath() const
    {
        return m_sourcePath;
    }

    QVariantList AnalysisSession::fileInfo() const
    {
        QVariantList info{};
        if (m_sourcePath.isEmpty())
        {
            return info;
        }

        const auto put = [&info](const QString& key, const QVariant& value)
        {
            QVariantMap item{};
            item[QStringLiteral("key")] = key;
            item[QStringLiteral("value")] = value;
            info.append(item);
        };

        put(QStringLiteral("文件"), m_sourcePath);
        put(QStringLiteral("生成者"), m_catalog.generator);
        put(QStringLiteral("总帧数"), QVariant::fromValue(m_catalog.declaredFrameCount));
        put(QStringLiteral("采样间隔/ms"),
            (m_catalog.sampleIntervalNs) / 1.0e6);
        put(QStringLiteral("总时长/s"),
            (m_catalog.declaredDurationNs) / 1.0e9);
        put(QStringLiteral("刚体数量"), m_catalog.bodies.size());
        put(QStringLiteral("含约束段"), m_catalog.hasConstraintSection);
        put(QStringLiteral("含力场段"), m_catalog.hasFieldSection);
        for (const QString& warning : m_catalog.warnings)
        {
            put(QStringLiteral("提示"), warning);
        }
        return info;
    }

    QVariantList AnalysisSession::entities() const
    {
        return m_entities;
    }

    QVariantList AnalysisSession::channels() const
    {
        return m_channels;
    }

    QVariantList AnalysisSession::seriesSummary() const
    {
        return m_summary;
    }

    int AnalysisSession::sampleCount() const
    {
        return static_cast<int>(m_result.sampledCount);
    }

    double AnalysisSession::sampleIntervalMs() const
    {
        const std::uint64_t interval =
            m_result.sampleIntervalNs != 0 ? m_result.sampleIntervalNs : m_catalog.sampleIntervalNs;
        return static_cast<double>(interval) / 1.0e6;
    }

    QString AnalysisSession::entityKey(const EntityRef& ref)
    {
        return QStringLiteral("%1:%2").arg(QString::fromUtf8(entityKindKey(ref.kind))).arg(ref.id);
    }

    std::optional<EntityRef> AnalysisSession::parseEntityKey(const QString& key)
    {
        const qsizetype separator = key.indexOf(QLatin1Char(':'));
        if (separator <= 0)
        {
            return std::nullopt;
        }

        const QString kindText = key.left(separator);
        EntityKind kind = EntityKind::Body;
        if (kindText == QLatin1String("body"))
        {
            kind = EntityKind::Body;
        }
        else if (kindText == QLatin1String("constraint"))
        {
            kind = EntityKind::Constraint;
        }
        else if (kindText == QLatin1String("field"))
        {
            kind = EntityKind::Field;
        }
        else
        {
            return std::nullopt;
        }

        bool ok = false;
        const qulonglong id = key.mid(separator + 1).toULongLong(&ok);
        if (!ok)
        {
            return std::nullopt;
        }

        EntityRef ref{};
        ref.kind = kind;
        ref.id = static_cast<std::uint64_t>(id);
        return ref;
    }

    QString AnalysisSession::seriesKey(const EntityRef& ref, const Channel channel)
    {
        return QStringLiteral("%1/%2")
            .arg(entityKey(ref), QLatin1String(channelInfo(channel).key));
    }

    void AnalysisSession::setError(const QString& message)
    {
        if (m_errorMessage == message)
        {
            return;
        }
        m_errorMessage = message;
        emit errorMessageChanged();
    }

    void AnalysisSession::setBusy(const bool busy)
    {
        if (m_busy == busy)
        {
            return;
        }
        m_busy = busy;
        emit busyChanged();
    }

    void AnalysisSession::setStatus(const QString& message)
    {
        if (m_statusMessage == message)
        {
            return;
        }
        m_statusMessage = message;
        emit statusMessageChanged();
    }

    bool AnalysisSession::openFile(const QString& filePath)
    {
        if (m_busy)
        {
            setError(QStringLiteral("正在执行其他任务，请稍候"));
            return false;
        }
        if (filePath.trimmed().isEmpty())
        {
            setError(QStringLiteral("文件路径为空"));
            return false;
        }

        epsx::EpsxReader reader{filePath};
        QString openError{};
        if (!reader.open(&openError))
        {
            setError(openError.isEmpty() ? QStringLiteral("无法打开 EPSX 文件") : openError);
            return false;
        }

        SceneCatalog catalog{};
        QString catalogError{};
        if (!buildSceneCatalog(reader.sceneJson(), &catalog, &catalogError))
        {
            // 场景块解析失败只影响实体命名，不影响后续按帧抽取
            catalog = SceneCatalog{};
            catalog.warnings.append(catalogError);
        }
        if (catalog.sampleIntervalNs == 0)
        {
            catalog.sampleIntervalNs = reader.sampleIntervalNs();
        }
        if (catalog.declaredFrameCount == 0)
        {
            catalog.declaredFrameCount = reader.frameCount();
        }

        m_sourcePath = filePath;
        m_catalog = catalog;
        m_result = ExtractionResult{};
        m_lod.clear();
        m_summary.clear();
        setError(QString{});

        // ---- 实体列表 ----
        m_entities.clear();
        const auto appendEntities = [this](const QVector<EntityDescriptor>& source)
        {
            for (const EntityDescriptor& descriptor : source)
            {
                QVariantMap item{};
                item[QStringLiteral("key")] = entityKey(descriptor.ref);
                item[QStringLiteral("kind")] = QString::fromUtf8(entityKindKey(descriptor.ref.kind));
                item[QStringLiteral("kindName")] =
                    QString::fromUtf8(entityKindDisplayName(descriptor.ref.kind));
                item[QStringLiteral("id")] = QVariant::fromValue(descriptor.ref.id);
                item[QStringLiteral("slot")] = static_cast<int>(descriptor.ref.slot);
                item[QStringLiteral("name")] = descriptor.name;
                item[QStringLiteral("mass")] = descriptor.mass;
                item[QStringLiteral("fixed")] = descriptor.fixed;
                item[QStringLiteral("type")] = static_cast<int>(descriptor.type);
                m_entities.append(item);
            }
        };
        appendEntities(m_catalog.bodies);
        appendEntities(m_catalog.constraints);
        appendEntities(m_catalog.fields);

        // 帧段存在但场景块没有命名时，给出可勾选的「全部约束 / 全部力场」入口
        if (m_catalog.hasConstraintSection && m_catalog.constraints.isEmpty())
        {
            QVariantMap item{};
            item[QStringLiteral("key")] = QString::fromUtf8(kAllConstraintsKey);
            item[QStringLiteral("kind")] = QStringLiteral("constraint");
            item[QStringLiteral("kindName")] = QStringLiteral("约束");
            item[QStringLiteral("name")] = QStringLiteral("（全部约束，按帧记录自动识别）");
            m_entities.append(item);
        }
        if (m_catalog.hasFieldSection && m_catalog.fields.isEmpty())
        {
            QVariantMap item{};
            item[QStringLiteral("key")] = QString::fromUtf8(kAllFieldsKey);
            item[QStringLiteral("kind")] = QStringLiteral("field");
            item[QStringLiteral("kindName")] = QStringLiteral("力场");
            item[QStringLiteral("name")] = QStringLiteral("（全部力场，按帧记录自动识别）");
            m_entities.append(item);
        }

        // ---- 通道列表 ----
        m_channels.clear();
        for (std::size_t index = 0; index < kChannelCount; ++index)
        {
            const auto channel = static_cast<Channel>(index);
            if (!isEntityChannel(channel))
            {
                continue;
            }
            const ChannelInfo& info = channelInfo(channel);
            QVariantMap item{};
            item[QStringLiteral("key")] = QString::fromUtf8(info.key);
            item[QStringLiteral("name")] = QString::fromUtf8(info.displayName);
            item[QStringLiteral("unit")] = QString::fromUtf8(info.unit);
            item[QStringLiteral("owner")] = QString::fromUtf8(entityKindKey(info.owner));
            item[QStringLiteral("derived")] = info.derived;
            item[QStringLiteral("populated")] = info.populated;
            m_channels.append(item);
        }

        emit catalogChanged();
        emit resultChanged();
        setStatus(QStringLiteral("已打开：%1（%2 帧）")
                      .arg(QFileInfo{filePath}.fileName())
                      .arg(m_catalog.declaredFrameCount));
        return true;
    }

    void AnalysisSession::closeFile()
    {
        if (m_busy)
        {
            setError(QStringLiteral("正在执行其他任务，请稍候"));
            return;
        }
        m_sourcePath.clear();
        m_catalog = SceneCatalog{};
        m_result = ExtractionResult{};
        m_lod.clear();
        m_summary.clear();
        m_entities.clear();
        m_channels.clear();
        setError(QString{});
        setStatus(QStringLiteral("已关闭文件"));
        emit catalogChanged();
        emit resultChanged();
    }

    void AnalysisSession::extract(const QVariantList& entityKeys,
                                  const QStringList& channelKeys,
                                  const int stride)
    {
        if (m_busy)
        {
            setError(QStringLiteral("正在执行其他任务，请稍候"));
            return;
        }
        if (m_sourcePath.isEmpty())
        {
            setError(QStringLiteral("请先打开 EPSX 文件"));
            return;
        }

        ExtractionRequest request{};
        request.filePath = m_sourcePath;
        request.stride = stride > 0 ? static_cast<std::uint32_t>(stride) : 1u;

        for (const QVariant& value : entityKeys)
        {
            const QString key = value.toString();
            if (key == QLatin1String(kAllConstraintsKey))
            {
                request.includeAllConstraints = true;
                continue;
            }
            if (key == QLatin1String(kAllFieldsKey))
            {
                request.includeAllFields = true;
                continue;
            }
            const std::optional<EntityRef> ref = parseEntityKey(key);
            if (ref.has_value())
            {
                request.entities.append(*ref);
            }
        }
        for (const QString& key : channelKeys)
        {
            const std::optional<Channel> channel = channelFromKey(key);
            if (channel.has_value())
            {
                request.channels.append(*channel);
            }
        }

        if (request.entities.isEmpty() && !request.includeAllConstraints && !request.includeAllFields)
        {
            setError(QStringLiteral("请至少选择一个实体"));
            return;
        }
        if (request.channels.isEmpty())
        {
            setError(QStringLiteral("请至少选择一个通道"));
            return;
        }

        m_cancelFlag->store(false);
        setError(QString{});
        setBusy(true);
        m_progress = 0.0;
        emit progressChanged();
        setStatus(QStringLiteral("正在抽取数据…"));

        const std::shared_ptr<std::atomic<bool>> cancelFlag = m_cancelFlag;
        std::ignore = QtConcurrent::run(
            [this, request, cancelFlag]()
            {
                ExtractionCallbacks callbacks{};
                callbacks.cancelled = [cancelFlag]()
                {
                    return cancelFlag->load();
                };
                callbacks.progress = [this, cancelFlag](const std::uint64_t done,
                                                        const std::uint64_t total)
                {
                    if (total == 0 || cancelFlag->load())
                    {
                        return;
                    }
                    const double ratio = static_cast<double>(done) / static_cast<double>(total);
                    QMetaObject::invokeMethod(
                        this,
                        [this, ratio]()
                        {
                            m_progress = ratio;
                            emit progressChanged();
                        },
                        Qt::QueuedConnection);
                };

                ExtractionResult result{};
                ExtractionReport report{};
                QString errorMessage{};
                const bool ok =
                    EpsxSeriesExtractor::extract(request, callbacks, &result, &report, &errorMessage);
                if (ok && report.elapsedSeconds > 0.0)
                {
                    QMetaObject::invokeMethod(
                        this,
                        [this, seconds = report.elapsedSeconds]()
                        {
                            setStatus(QStringLiteral("抽取完成，用时 %1 s").arg(seconds, 0, 'f', 3));
                        },
                        Qt::QueuedConnection);
                }
                QMetaObject::invokeMethod(
                    this,
                    [this, ok, result = std::move(result), errorMessage, cancelled = report.cancelled]() mutable
                    {
                        applyExtractionResult(ok, std::move(result), errorMessage, cancelled);
                    },
                    Qt::QueuedConnection);
            });
    }

    void AnalysisSession::cancel()
    {
        m_cancelFlag->store(true);
    }

    void AnalysisSession::applyExtractionResult(bool success,
                                                ExtractionResult result,
                                                const QString& errorMessage,
                                                const bool cancelled)
    {
        setBusy(false);
        if (!success)
        {
            setError(errorMessage.isEmpty() ? QStringLiteral("抽取失败") : errorMessage);
            setStatus(QStringLiteral("抽取失败"));
            emit operationFinished(false);
            return;
        }

        m_result = std::move(result);
        rebuildSeriesIndex();
        setError(QString{});
        m_progress = 1.0;
        emit progressChanged();
        emit resultChanged();
        if (cancelled)
        {
            setStatus(QStringLiteral("已取消，保留 %1 个样本").arg(m_result.sampledCount));
        }
        else
        {
            setStatus(QStringLiteral("已抽取 %1 个样本、%2 条序列")
                          .arg(m_result.sampledCount)
                          .arg(m_result.series.size()));
        }
        emit operationFinished(true);
    }

    void AnalysisSession::rebuildSeriesIndex()
    {
        m_lod.clear();
        m_summary.clear();

        const std::size_t count = m_result.time.size();
        if (count == 0)
        {
            return;
        }
        const double* timeData = m_result.time.data();

        for (const ExtractedSeries& series : m_result.series)
        {
            if (series.values.size() != count)
            {
                continue;
            }

            LodEntry entry{};
            entry.entity = series.entity;
            entry.channel = series.channel;
            entry.values = series.values.data();
            entry.lod.build(timeData, entry.values, count);

            const QString key = seriesKey(series.entity, series.channel);
            const QString entityName = displayNameOf(m_result, series.entity);
            const ChannelInfo& info = channelInfo(series.channel);
            const QString unit = QString::fromUtf8(info.unit);
            const QString label = QStringLiteral("%1 · %2").arg(entityName,
                                                                QString::fromUtf8(info.displayName));

            QVariantMap item{};
            item[QStringLiteral("key")] = key;
            item[QStringLiteral("entityKey")] = entityKey(series.entity);
            item[QStringLiteral("entityName")] = entityName;
            item[QStringLiteral("channelKey")] = QString::fromUtf8(info.key);
            item[QStringLiteral("label")] = label;
            item[QStringLiteral("unit")] = unit;
            item[QStringLiteral("sampleCount")] = static_cast<int>(count);
            item[QStringLiteral("xMin")] = entry.lod.xMin();
            item[QStringLiteral("xMax")] = entry.lod.xMax();
            item[QStringLiteral("yMin")] = entry.lod.yMin();
            item[QStringLiteral("yMax")] = entry.lod.yMax();
            m_summary.append(item);

            m_lod.insert(key, entry);
        }
    }

    bool AnalysisSession::exportTable(const QString& filePath, const QString& format)
    {
        if (m_busy)
        {
            setError(QStringLiteral("正在执行其他任务，请稍候"));
            return false;
        }
        if (m_result.sampledCount == 0)
        {
            setError(QStringLiteral("还没有抽取结果，无法导出"));
            return false;
        }
        if (filePath.trimmed().isEmpty())
        {
            setError(QStringLiteral("导出路径为空"));
            return false;
        }

        ExportOptions options{};
        options.targetPath = filePath;
        options.format =
            format.compare(QStringLiteral("csv"), Qt::CaseInsensitive) == 0 ? ExportFormat::Csv
                                                                            : ExportFormat::Xlsx;

        // 导出期间 busy 会拦住 openFile / closeFile / extract，m_result 不会被替换，
        // 因此这里直接引用而不做整份拷贝（百万级样本的拷贝代价过高）。
        const ExtractionResult* snapshot = &m_result;
        m_cancelFlag->store(false);
        setError(QString{});
        setBusy(true);
        m_progress = 0.0;
        emit progressChanged();
        setStatus(QStringLiteral("正在导出…"));

        const std::shared_ptr<std::atomic<bool>> cancelFlag = m_cancelFlag;
        std::ignore = QtConcurrent::run(
            [this, options, snapshot, cancelFlag]()
            {
                ExtractionCallbacks callbacks{};
                callbacks.cancelled = [cancelFlag]()
                {
                    return cancelFlag->load();
                };
                callbacks.progress = [this, cancelFlag](const std::uint64_t done,
                                                        const std::uint64_t total)
                {
                    if (total == 0 || cancelFlag->load())
                    {
                        return;
                    }
                    const double ratio = static_cast<double>(done) / static_cast<double>(total);
                    QMetaObject::invokeMethod(
                        this,
                        [this, ratio]()
                        {
                            m_progress = ratio;
                            emit progressChanged();
                        },
                        Qt::QueuedConnection);
                };

                ExportReport report{};
                QString errorMessage{};
                const bool ok = AnalysisExporter::exportResult(
                    *snapshot, options, callbacks, &report, &errorMessage);

                QMetaObject::invokeMethod(
                    this,
                    [this, ok, errorMessage, report]()
                    {
                        setBusy(false);
                        if (!ok)
                        {
                            setError(errorMessage.isEmpty() ? QStringLiteral("导出失败")
                                                            : errorMessage);
                            setStatus(QStringLiteral("导出失败"));
                            emit operationFinished(false);
                            return;
                        }
                        m_progress = 1.0;
                        emit progressChanged();
                        setError(QString{});
                        setStatus(QStringLiteral("已导出 %1 行到 %2")
                                      .arg(report.rows)
                                      .arg(QDir::toNativeSeparators(
                                          report.writtenFiles.value(0))));
                        emit operationFinished(true);
                    },
                    Qt::QueuedConnection);
            });
        return true;
    }

    QVariantList AnalysisSession::seriesPoints(const QString& seriesKeyText,
                                               const double xMin,
                                               const double xMax,
                                               int maxPoints,
                                               const bool uniform) const
    {
        QVariantList points{};
        const auto found = m_lod.constFind(seriesKeyText);
        if (found == m_lod.constEnd())
        {
            return points;
        }
        if (maxPoints <= 0)
        {
            maxPoints = kDefaultMaxPoints;
        }

        std::vector<LodPoint> decimated{};
        if (uniform)
        {
            found->lod.decimateUniform(static_cast<std::size_t>(maxPoints), &decimated);
        }
        else
        {
            found->lod.decimate(xMin, xMax, static_cast<std::size_t>(maxPoints), &decimated);
        }

        points.reserve(static_cast<qsizetype>(decimated.size()));
        for (const LodPoint& point : decimated)
        {
            points.append(QVariant::fromValue(QPointF{point.x, point.y}));
        }
        return points;
    }

    QVariantMap AnalysisSession::seriesValues(const QString& seriesKeyText) const
    {
        QVariantMap values{};
        const auto found = m_lod.constFind(seriesKeyText);
        if (found == m_lod.constEnd() || found->values == nullptr)
        {
            return values;
        }

        const std::size_t count = m_result.time.size();
        QVariantList xs{};
        QVariantList ys{};
        xs.reserve(static_cast<qsizetype>(count));
        ys.reserve(static_cast<qsizetype>(count));
        for (std::size_t index = 0; index < count; ++index)
        {
            xs.append(m_result.time[index]);
            ys.append(found->values[index]);
        }
        values[QStringLiteral("xs")] = xs;
        values[QStringLiteral("ys")] = ys;
        return values;
    }
}
