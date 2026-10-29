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
#include "curve_fitter.h"
#include "epsx_reader.h"
#include "epsx_series_extractor.h"
#include "expression_evaluator.h"

#include <fftw3.h>

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointF>
#include <QFutureWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <QtConcurrent>

#include <cstddef>
#include <cmath>
#include <limits>
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

        /** 样本下标区间 [begin, end)。 */
        struct SampleRange
        {
            /** 起始下标。 */
            std::size_t begin{0};

            /** 结束下标（不含）。 */
            std::size_t end{0};
        };

        /**
         * @brief  把横轴区间换算成样本下标区间。
         * @details 时间轴单调不减，因此用二分查找定位；rangeMax ≤ rangeMin 时返回全范围。
         * @param  time 横轴数组。
         * @param  rangeMin 区间左端。
         * @param  rangeMax 区间右端。
         * @return 下标区间；区间与数据无交集时返回空区间。
         */
        SampleRange sampleRangeFor(const std::vector<double>& time,
                                   const double rangeMin,
                                   const double rangeMax)
        {
            SampleRange range{};
            range.end = time.size();
            if (time.empty())
            {
                range.begin = 0;
                return range;
            }
            if (!(rangeMax > rangeMin))
            {
                return range;
            }

            const auto lowerBound = [&time](const double value) -> std::size_t
            {
                std::size_t low = 0;
                std::size_t high = time.size();
                while (low < high)
                {
                    const std::size_t mid = low + (high - low) / 2;
                    if (time[mid] < value)
                    {
                        low = mid + 1;
                    }
                    else
                    {
                        high = mid;
                    }
                }
                return low;
            };

            range.begin = lowerBound(rangeMin);
            const std::size_t after = lowerBound(rangeMax);
            range.end = std::min(time.size(), after + 1);
            if (range.begin >= range.end)
            {
                range.begin = 0;
                range.end = 0;
            }
            return range;
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
        clearAnalysis();
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
        clearAnalysis();
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
        clearAnalysis();
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

    /** @brief 保存完整频谱，供缩放后重新生成绘图点。 */
    struct SpectrumData
    {
        double spacing{0.0};
        double sampleRate{0.0};
        std::vector<double> amplitudes{};
        std::vector<double> phases{};
    };

    /** @brief 对指定频率范围按桶保留极值，限制界面线程的数据量。 */
    static QVariantList spectrumPoints(const std::vector<double>& values, double spacing,
                                       double lower, double upper, int maxPoints)
    {
        QVariantList points;
        if (values.empty() || spacing <= 0.0 || upper < 0.0 || lower > upper) return points;
        const double lastFrequency = spacing * static_cast<double>(values.size() - 1);
        if (lower > lastFrequency) return points;
        const auto first = static_cast<std::size_t>(std::max(0.0, std::floor(lower / spacing)));
        const auto last = static_cast<std::size_t>(std::min(static_cast<double>(values.size() - 1),
                                                            std::ceil(upper / spacing)));
        const std::size_t count = last - first + 1;
        const std::size_t buckets = std::max<std::size_t>(1, static_cast<std::size_t>(std::max(4, maxPoints)) / 2);
        const std::size_t step = std::max<std::size_t>(1, (count + buckets - 1) / buckets);
        points.append(QVariant::fromValue(QPointF(first * spacing, values[first])));
        for (std::size_t begin = first; begin <= last; begin += step)
        {
            const std::size_t end = std::min(last + 1, begin + step);
            std::size_t minIndex = begin, maxIndex = begin;
            for (std::size_t i = begin + 1; i < end; ++i)
            {
                if (values[i] < values[minIndex]) minIndex = i;
                if (values[i] > values[maxIndex]) maxIndex = i;
            }
            if (minIndex > maxIndex) std::swap(minIndex, maxIndex);
            points.append(QVariant::fromValue(QPointF(minIndex * spacing, values[minIndex])));
            if (maxIndex != minIndex)
                points.append(QVariant::fromValue(QPointF(maxIndex * spacing, values[maxIndex])));
        }
        if (last != first)
            points.append(QVariant::fromValue(QPointF(last * spacing, values[last])));
        return points;
    }

    /** @brief 生成指定频率范围的两组有限绘图点。 */
    static QVariantMap spectrumView(const SpectrumData& spectrum, double lower, double upper, int maxPoints)
    {
        QVariantMap result;
        result[QStringLiteral("amplitudes")] = spectrumPoints(spectrum.amplitudes, spectrum.spacing, lower, upper, maxPoints);
        result[QStringLiteral("phases")] = spectrumPoints(spectrum.phases, spectrum.spacing, lower, upper, maxPoints);
        result[QStringLiteral("sampleRate")] = spectrum.sampleRate;
        result[QStringLiteral("maxFrequency")] = spectrum.spacing * (spectrum.amplitudes.size() - 1);
        return result;
    }

    /** @brief 在工作线程中计算等间隔序列的单边频谱。 */
    static QVariantMap calculateSpectrum(const std::vector<double>& times, const std::vector<double>& values,
                                         SpectrumData& spectrum, int maxPoints)
    {
        QVariantMap result{};
        const std::size_t count = times.size();
        if (count < 2 ||
            count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            result[QStringLiteral("message")] = QStringLiteral("频谱分析至少需要两个有效样本。");
            return result;
        }
        const double dt = (times.back() - times.front()) / static_cast<double>(count - 1);
        if (!std::isfinite(dt) || dt <= 0.0)
        {
            result[QStringLiteral("message")] = QStringLiteral("采样时间无效，无法计算频谱。");
            return result;
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            if (!std::isfinite(values[i]) || !std::isfinite(times[i]) ||
                (i > 0 && std::abs((times[i] - times[i - 1]) - dt) >
                 std::max(dt * 1e-4, 2e-9)))
            {
                result[QStringLiteral("message")] = QStringLiteral("数据包含无效值或采样间隔不均匀，无法计算频谱。");
                return result;
            }
        }

        static QMutex fftwMutex;
        QMutexLocker locker(&fftwMutex);
        std::vector<double> input(values);
        std::vector<double> output(2 * (count / 2 + 1));
        fftw_plan plan = fftw_plan_dft_r2c_1d(static_cast<int>(count), input.data(),
                                              reinterpret_cast<fftw_complex*>(output.data()),
                                              FFTW_ESTIMATE);
        if (!plan)
        {
            result[QStringLiteral("message")] = QStringLiteral("无法创建傅里叶变换计划。");
            return result;
        }
        fftw_execute(plan);
        fftw_destroy_plan(plan);
        const std::size_t bins = count / 2 + 1;
        spectrum.spacing = 1.0 / (static_cast<double>(count) * dt);
        spectrum.sampleRate = 1.0 / dt;
        spectrum.amplitudes.resize(bins);
        spectrum.phases.resize(bins);
        for (std::size_t i = 0; i < bins; ++i)
        {
            const double real = output[2 * i];
            const double imaginary = output[2 * i + 1];
            const double scale = (i == 0 || (count % 2 == 0 && i == count / 2)) ? 1.0 : 2.0;
            spectrum.amplitudes[i] = scale * std::hypot(real, imaginary) / count;
            spectrum.phases[i] = std::atan2(imaginary, real);
        }
        result = spectrumView(spectrum, 0.0, spectrum.spacing * (bins - 1), maxPoints);
        return result;
    }

    void AnalysisSession::requestPlot(const QString& seriesKey, const bool frequencyDomain,
                                      int maxPoints, const bool uniform, const int requestId)
    {
        m_plotGeneration = requestId;
        m_spectrumCache.reset();
        const auto found = m_lod.constFind(seriesKey);
        if (found == m_lod.constEnd() || !found->values)
        {
            emit plotReady(requestId, {{QStringLiteral("message"), QStringLiteral("请选择有效序列。")}});
            return;
        }
        // 工作线程持有独立快照，关闭文件或重新抽取不会使其引用失效。
        auto times = m_result.time;
        std::vector<double> values(found->values, found->values + times.size());
        if (maxPoints <= 0) maxPoints = kDefaultMaxPoints;
        auto spectrum = std::make_shared<SpectrumData>();
        auto* watcher = new QFutureWatcher<QVariantMap>(this);
        connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, requestId, frequencyDomain, spectrum]()
        {
            const QVariantMap result = watcher->result();
            watcher->deleteLater();
            if (frequencyDomain && requestId == m_plotGeneration && !spectrum->amplitudes.empty())
                m_spectrumCache = spectrum;
            emit plotReady(requestId, result);
        });
        watcher->setFuture(QtConcurrent::run([times = std::move(times), values = std::move(values),
                                               frequencyDomain, maxPoints, uniform, spectrum]() -> QVariantMap
        {
            if (frequencyDomain) return calculateSpectrum(times, values, *spectrum, maxPoints);
            QVariantMap result{};
            if (times.empty()) return result;
            MinMaxLod lod;
            lod.build(times.data(), values.data(), times.size());
            std::vector<LodPoint> decimated;
            if (uniform) lod.decimateUniform(static_cast<std::size_t>(maxPoints), &decimated);
            else lod.decimate(lod.xMin(), lod.xMax(), static_cast<std::size_t>(maxPoints), &decimated);
            QVariantList points;
            points.reserve(static_cast<qsizetype>(decimated.size()));
            for (const auto& point : decimated)
                points.append(QVariant::fromValue(QPointF(point.x, point.y)));
            result[QStringLiteral("points")] = points;
            return result;
        }));
    }

    void AnalysisSession::requestSpectrumView(const double xMin, const double xMax,
                                              const int maxPoints, const int requestId)
    {
        const auto spectrum = m_spectrumCache;
        if (!spectrum) return;
        auto* watcher = new QFutureWatcher<QVariantMap>(this);
        connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, requestId]()
        {
            QVariantMap result = watcher->result();
            watcher->deleteLater();
            result[QStringLiteral("viewUpdate")] = true;
            emit plotReady(requestId, result);
        });
        watcher->setFuture(QtConcurrent::run([spectrum, xMin, xMax, maxPoints]()
        {
            return spectrumView(*spectrum, xMin, xMax, maxPoints);
        }));
    }

    QVariantMap AnalysisSession::fitInfo() const
    {
        return m_fitInfo;
    }

    QVariantMap AnalysisSession::comparisonInfo() const
    {
        return m_comparisonInfo;
    }

    QVariantList AnalysisSession::overlayPoints() const
    {
        QVariantList points{};
        points.reserve(static_cast<qsizetype>(m_overlayX.size()));
        for (std::size_t index = 0; index < m_overlayX.size(); ++index)
        {
            points.append(QVariant::fromValue(QPointF{m_overlayX[index], m_overlayY[index]}));
        }
        return points;
    }

    void AnalysisSession::setOverlay(const std::vector<double>& x,
                                     const std::vector<double>& y,
                                     const QString& label)
    {
        m_overlayX.clear();
        m_overlayY.clear();
        m_overlayLabel = label;

        const std::size_t count = std::min(x.size(), y.size());
        if (count >= 2)
        {
            constexpr std::size_t kMaxOverlayPoints{2000};
            const std::size_t stride = std::max<std::size_t>(1, count / kMaxOverlayPoints);
            m_overlayX.reserve(count / stride + 2);
            m_overlayY.reserve(count / stride + 2);
            for (std::size_t index = 0; index < count; index += stride)
            {
                m_overlayX.push_back(x[index]);
                m_overlayY.push_back(y[index]);
            }
            if (m_overlayX.back() != x[count - 1])
            {
                m_overlayX.push_back(x[count - 1]);
                m_overlayY.push_back(y[count - 1]);
            }
        }
        emit overlayChanged();
    }

    void AnalysisSession::clearAnalysis()
    {
        m_fitInfo = QVariantMap{};
        m_comparisonInfo = QVariantMap{};
        m_overlayX.clear();
        m_overlayY.clear();
        m_overlayLabel.clear();
        emit fitChanged();
        emit comparisonChanged();
        emit overlayChanged();
    }

    QVariantMap AnalysisSession::fitSeries(const QString& seriesKeyText,
                                           const QString& model,
                                           const int polynomialDegree,
                                           const int harmonicCount,
                                           const double angularFrequency,
                                           const bool exponentialWithOffset,
                                           const double rangeMin,
                                           const double rangeMax)
    {
        QVariantMap info{};
        const auto found = m_lod.constFind(seriesKeyText);
        if (found == m_lod.constEnd() || found->values == nullptr)
        {
            info[QStringLiteral("ok")] = false;
            info[QStringLiteral("message")] = QStringLiteral("请先在图表上方选择一条有效序列");
            m_fitInfo = info;
            emit fitChanged();
            return info;
        }

        const SampleRange range = sampleRangeFor(m_result.time, rangeMin, rangeMax);
        const std::size_t count = range.end - range.begin;
        if (count < 3)
        {
            info[QStringLiteral("ok")] = false;
            info[QStringLiteral("message")] =
                QStringLiteral("所选时间区间内的样本不足（至少需要 3 个点），请放宽拟合区间");
            m_fitInfo = info;
            emit fitChanged();
            return info;
        }

        const std::vector<double> fitX{
            m_result.time.begin() + static_cast<std::ptrdiff_t>(range.begin),
            m_result.time.begin() + static_cast<std::ptrdiff_t>(range.end)};
        const std::vector<double> fitY{found->values + range.begin, found->values + range.end};

        FitRequest request{};
        if (model == QLatin1String("polynomial"))
        {
            request.model = FitModel::Polynomial;
        }
        else if (model == QLatin1String("trigonometric"))
        {
            request.model = FitModel::Trigonometric;
        }
        else if (model == QLatin1String("exponential"))
        {
            request.model = FitModel::Exponential;
        }
        else if (model == QLatin1String("logarithmic"))
        {
            request.model = FitModel::Logarithmic;
        }
        else
        {
            request.model = FitModel::Linear;
        }
        request.polynomialDegree = polynomialDegree;
        request.harmonicCount = harmonicCount;
        request.angularFrequency = angularFrequency;
        request.exponentialWithOffset = exponentialWithOffset;

        const FitResult fitResult = CurveFitter::fit(fitX.data(), fitY.data(), count, request);
        if (!fitResult.ok)
        {
            info[QStringLiteral("ok")] = false;
            info[QStringLiteral("message")] = fitResult.message;
            m_fitInfo = info;
            emit fitChanged();
            return info;
        }

        info[QStringLiteral("ok")] = true;
        info[QStringLiteral("model")] = QString::fromUtf8(CurveFitter::modelKey(fitResult.model));
        info[QStringLiteral("modelName")] = CurveFitter::modelDisplayName(fitResult.model);
        info[QStringLiteral("equation")] = fitResult.equation;
        info[QStringLiteral("rSquared")] = fitResult.rSquared;
        info[QStringLiteral("adjustedRSquared")] = fitResult.adjustedRSquared;
        info[QStringLiteral("rmse")] = fitResult.rmse;
        info[QStringLiteral("mae")] = fitResult.mae;
        info[QStringLiteral("maxAbsResidual")] = fitResult.maxAbsResidual;
        info[QStringLiteral("angularFrequency")] = fitResult.angularFrequency;
        info[QStringLiteral("sampleCount")] = fitResult.sampleCount;
        info[QStringLiteral("message")] = fitResult.message;
        info[QStringLiteral("rangeMin")] = fitX.front();
        info[QStringLiteral("rangeMax")] = fitX.back();

        QVariantList coefficients{};
        for (qsizetype index = 0; index < fitResult.coefficients.size(); ++index)
        {
            QVariantMap item{};
            item[QStringLiteral("name")] = fitResult.coefficientNames.value(index);
            item[QStringLiteral("value")] = fitResult.coefficients[index];
            coefficients.append(item);
        }
        info[QStringLiteral("coefficients")] = coefficients;

        // 拟合曲线只画在拟合区间内：多项式等模型外推到区间外没有物理意义
        const QVector<double> fitted = CurveFitter::evaluate(fitResult, fitX.data(), count);
        if (fitted.size() == static_cast<qsizetype>(count))
        {
            setOverlay(fitX,
                       std::vector<double>(fitted.begin(), fitted.end()),
                       QStringLiteral("拟合曲线"));
        }
        else
        {
            emit overlayChanged();
        }

        m_fitInfo = info;
        // 新的拟合让旧的误差对比失效，避免界面上两套曲线混在一起
        m_comparisonInfo = QVariantMap{};
        emit fitChanged();
        emit comparisonChanged();
        return info;
    }

    QVariantMap AnalysisSession::compareSeries(const QString& seriesKeyText,
                                               const QString& expression,
                                               const QString& variableName,
                                               const QVariantMap& variables,
                                               const double rangeMin,
                                               const double rangeMax)
    {
        QVariantMap info{};
        const auto found = m_lod.constFind(seriesKeyText);
        if (found == m_lod.constEnd() || found->values == nullptr)
        {
            info[QStringLiteral("ok")] = false;
            info[QStringLiteral("message")] = QStringLiteral("请先在图表上方选择一条有效序列");
            m_comparisonInfo = info;
            emit comparisonChanged();
            return info;
        }

        QMap<QString, double> constants{};
        for (auto it = variables.constBegin(); it != variables.constEnd(); ++it)
        {
            bool ok = false;
            const double value = it.value().toDouble(&ok);
            if (ok)
            {
                constants.insert(it.key(), value);
            }
        }

        const QString name = variableName.trimmed().isEmpty() ? QStringLiteral("t")
                                                             : variableName.trimmed();
        const SampleRange range = sampleRangeFor(m_result.time, rangeMin, rangeMax);
        const std::size_t count = range.end - range.begin;
        if (count < 2)
        {
            info[QStringLiteral("ok")] = false;
            info[QStringLiteral("message")] =
                QStringLiteral("所选时间区间内的样本不足（至少需要 2 个点）");
            m_comparisonInfo = info;
            emit comparisonChanged();
            return info;
        }

        const ComparisonResult comparison =
            ExpressionEvaluator::compare(m_result.time.data() + range.begin,
                                         found->values + range.begin,
                                         count,
                                         expression,
                                         name,
                                         constants);
        if (!comparison.ok)
        {
            info[QStringLiteral("ok")] = false;
            info[QStringLiteral("message")] = comparison.message;
            m_comparisonInfo = info;
            emit comparisonChanged();
            return info;
        }

        info[QStringLiteral("ok")] = true;
        info[QStringLiteral("sampleCount")] = comparison.sampleCount;
        info[QStringLiteral("rSquared")] = comparison.rSquared;
        info[QStringLiteral("rmse")] = comparison.rmse;
        info[QStringLiteral("mae")] = comparison.mae;
        info[QStringLiteral("maxAbsError")] = comparison.maxAbsError;
        info[QStringLiteral("nrmse")] = comparison.nrmse;
        info[QStringLiteral("meanRelativeError")] = comparison.meanRelativeError;
        info[QStringLiteral("maxRelativeError")] = comparison.maxRelativeError;
        info[QStringLiteral("expression")] = expression;
        info[QStringLiteral("variable")] = name;
        info[QStringLiteral("message")] = QString{};
        info[QStringLiteral("rangeMin")] = m_result.time[range.begin];
        info[QStringLiteral("rangeMax")] = m_result.time[range.end - 1];

        // 理论曲线在全时间轴上都成立，因此画满整条横轴；误差指标只统计所选区间
        QVector<double> fullPredicted{};
        QString overlayError{};
        if (ExpressionEvaluator::evaluateOnGrid(m_result.time.data(),
                                                m_result.time.size(),
                                                expression,
                                                name,
                                                constants,
                                                &fullPredicted,
                                                &overlayError))
        {
            setOverlay(m_result.time,
                       std::vector<double>(fullPredicted.begin(), fullPredicted.end()),
                       QStringLiteral("理论曲线"));
        }
        else
        {
            emit overlayChanged();
        }
        m_comparisonInfo = info;
        m_fitInfo = QVariantMap{};
        emit comparisonChanged();
        emit fitChanged();
        return info;
    }
}
