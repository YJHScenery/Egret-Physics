/**
 * @file        analysis_session.h
 * @brief       数据分析工作区的 QML 门面对象。
 * @details     AnalysisSession 把「读文件 → 选实体与通道 → 抽帧 → 导出 → 取绘图点」
 *              封装成 QML 可以直接调用的接口，并把耗时操作（抽取、导出）投到线程池，
 *              通过 busy / progress / statusMessage 暴露状态。
 *
 *              键的编码约定（QML 与 C++ 之间用它传递实体与序列）：
 *              - 实体键：`<kind>:<id>`，例如 `body:12`、`constraint:3`；
 *              - 序列键：`<实体键>/<通道键>`，例如 `body:12/speed`。
 *
 *              数据所有权：抽取结果保存在本对象内，LOD 摘要只引用其内部数组，
 *              因此序列数据在下次抽取或关闭文件前一直有效。
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

#ifndef EGRET_PHYSICS_ANALYSIS_SESSION_H
#define EGRET_PHYSICS_ANALYSIS_SESSION_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <memory>
#include <optional>

#include "analysis_types.h"
#include "epsx_channel_catalog.h"
#include "series_lod.h"

namespace egret::analysis
{
    /**
     * @brief       数据分析工作区的门面对象。
     * @details     在 main.cpp 中以上下文属性 analysisSession 注册给 QML。
     *
     * @invariant   busy 为 true 时不会启动第二个抽取或导出任务
     */
    class AnalysisSession final : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)

        Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

        Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

        Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

        Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY catalogChanged)

        Q_PROPERTY(QVariantList fileInfo READ fileInfo NOTIFY catalogChanged)

        Q_PROPERTY(QVariantList entities READ entities NOTIFY catalogChanged)

        Q_PROPERTY(QVariantList channels READ channels NOTIFY catalogChanged)

        Q_PROPERTY(QVariantList seriesSummary READ seriesSummary NOTIFY resultChanged)

        Q_PROPERTY(int sampleCount READ sampleCount NOTIFY resultChanged)

        Q_PROPERTY(double sampleIntervalMs READ sampleIntervalMs NOTIFY resultChanged)

        Q_PROPERTY(QVariantMap fitInfo READ fitInfo NOTIFY fitChanged)

        Q_PROPERTY(QVariantMap comparisonInfo READ comparisonInfo NOTIFY comparisonChanged)

        Q_PROPERTY(QVariantList overlayPoints READ overlayPoints NOTIFY overlayChanged)

    public:
        /** 默认构造。 */
        explicit AnalysisSession(QObject* parent = nullptr);

        /** 默认析构。 */
        ~AnalysisSession() override;

        /** 是否正在执行抽取或导出。 */
        [[nodiscard]] bool isBusy() const;

        /** 当前任务进度比例 0 ~ 1。 */
        [[nodiscard]] double progress() const;

        /** 面向用户的状态描述。 */
        [[nodiscard]] QString statusMessage() const;

        /** 最近一次失败原因；无错误时为空串。 */
        [[nodiscard]] QString errorMessage() const;

        /** 当前打开的文件路径。 */
        [[nodiscard]] QString sourcePath() const;

        /** 文件级信息表（供界面展示）。 */
        [[nodiscard]] QVariantList fileInfo() const;

        /** 实体列表（供界面勾选）。 */
        [[nodiscard]] QVariantList entities() const;

        /** 通道列表（含所属类别与可用性标记）。 */
        [[nodiscard]] QVariantList channels() const;

        /** 已抽取序列的摘要列表。 */
        [[nodiscard]] QVariantList seriesSummary() const;

        /** 已抽取的样本数。 */
        [[nodiscard]] int sampleCount() const;

        /** 文件的采样间隔（毫秒）。 */
        [[nodiscard]] double sampleIntervalMs() const;

        /** 最近一次曲线拟合的结果摘要。 */
        [[nodiscard]] QVariantMap fitInfo() const;

        /** 最近一次误差对比的结果摘要。 */
        [[nodiscard]] QVariantMap comparisonInfo() const;

        /** 叠加曲线（拟合曲线或理论曲线）的绘图点。 */
        [[nodiscard]] QVariantList overlayPoints() const;

        /**
         * @brief  打开 EPSX 文件并解析实体 / 通道目录（同步，只读场景块与索引）。
         * @param  filePath 文件路径。
         * @return 成功返回 true。
         */
        Q_INVOKABLE [[nodiscard]] bool openFile(const QString& filePath);

        /** 关闭当前文件并清空实体、通道与结果。 */
        Q_INVOKABLE void closeFile();

        /**
         * @brief  在后台线程执行抽取。
         * @param  entityKeys 实体键列表（`<kind>:<id>`）。
         * @param  channelKeys 通道键列表（ChannelInfo::key）。
         * @param  stride 抽帧比，≥ 1。
         */
        Q_INVOKABLE void extract(const QVariantList& entityKeys,
                                 const QStringList& channelKeys,
                                 int stride);

        /** 请求取消当前任务。 */
        Q_INVOKABLE void cancel();

        /**
         * @brief  把当前抽取结果导出为 xlsx 或 CSV。
         * @param  filePath 目标路径。
         * @param  format "xlsx" 或 "csv"。
         * @return 参数合法并已提交任务返回 true。
         */
        Q_INVOKABLE [[nodiscard]] bool exportTable(const QString& filePath, const QString& format);

        /**
         * @brief  取某个序列的绘图点集（按视口抽稀）。
         * @param  seriesKey 序列键。
         * @param  xMin 视口左端。
         * @param  xMax 视口右端。
         * @param  maxPoints 期望最大点数。
         * @param  uniform true 时做均匀抽样，false 时做 min/max 抽稀。
         * @return 点集（每个元素为 QPointF）；键无效时返回空。
         */
        Q_INVOKABLE [[nodiscard]] QVariantList seriesPoints(const QString& seriesKey,
                                                            double xMin,
                                                            double xMax,
                                                            int maxPoints,
                                                            bool uniform) const;

        /**
         * @brief  取某个序列的完整数值列（供拟合与误差对比使用，不抽稀）。
         * @param  seriesKey 序列键。
         * @return {xs:[...], ys:[...]}；键无效时返回空映射。
         */
        Q_INVOKABLE [[nodiscard]] QVariantMap seriesValues(const QString& seriesKey) const;

        /**
         * @brief  对某个序列做曲线拟合。
         * @param  seriesKey 序列键。
         * @param  model 模型键（linear / polynomial / trigonometric / exponential / logarithmic）。
         * @param  polynomialDegree 多项式阶数。
         * @param  harmonicCount 三角函数谐波数。
         * @param  angularFrequency 三角函数角频率，≤ 0 表示自动估计。
         * @param  exponentialWithOffset 指数模型是否带偏移项。
         * @param  rangeMin 拟合区间左端（横轴值）；rangeMax ≤ rangeMin 表示全范围。
         * @param  rangeMax 拟合区间右端。
         * @return 拟合摘要（方程、系数、优度指标）；失败时含 message。
         */
        Q_INVOKABLE [[nodiscard]] QVariantMap fitSeries(const QString& seriesKey,
                                                        const QString& model,
                                                        int polynomialDegree,
                                                        int harmonicCount,
                                                        double angularFrequency,
                                                        bool exponentialWithOffset,
                                                        double rangeMin,
                                                        double rangeMax);

        /**
         * @brief  把理论方程与某个序列做误差对比。
         * @param  seriesKey 序列键（提供实测值）。
         * @param  expression 理论方程表达式。
         * @param  variableName 自变量名（对应序列的横轴，即时间）。
         * @param  variables 常量表（名称 → 数值）。
         * @param  rangeMin 统计区间左端；rangeMax ≤ rangeMin 表示全范围。
         * @param  rangeMax 统计区间右端。
         * @return 对比摘要（各误差指标）；失败时含 message。
         */
        Q_INVOKABLE [[nodiscard]] QVariantMap compareSeries(const QString& seriesKey,
                                                            const QString& expression,
                                                            const QString& variableName,
                                                            const QVariantMap& variables,
                                                            double rangeMin,
                                                            double rangeMax);

        /** 清除叠加曲线与拟合 / 对比结果。 */
        Q_INVOKABLE void clearAnalysis();

    signals:
        /** 忙闲状态变化。 */
        void busyChanged();

        /** 进度变化。 */
        void progressChanged();

        /** 状态描述变化。 */
        void statusMessageChanged();

        /** 错误描述变化。 */
        void errorMessageChanged();

        /** 文件目录（实体 / 通道）变化。 */
        void catalogChanged();

        /** 抽取结果变化。 */
        void resultChanged();

        /** 拟合结果变化。 */
        void fitChanged();

        /** 误差对比结果变化。 */
        void comparisonChanged();

        /** 叠加曲线变化。 */
        void overlayChanged();

        /**
         * @brief  一次后台任务结束。
         * @param  success 是否成功。
         */
        void operationFinished(bool success);

    private:
        /** LOD 条目：序列引用与抽稀器。 */
        struct LodEntry
        {
            /** 所属实体。 */
            EntityRef entity{};

            /** 通道。 */
            Channel channel{Channel::TimeSeconds};

            /** 样本值指针（指向 m_result 内部，构建后不再失效）。 */
            const double* values{nullptr};

            /** 抽稀器。 */
            MinMaxLod lod{};
        };

        /**
         * @brief  记录错误并通知界面。
         * @param message 错误描述。
         */
        void setError(const QString& message);

        /**
         * @brief  更新忙闲状态并通知界面。
         * @param busy 目标状态。
         */
        void setBusy(bool busy);

        /**
         * @brief  更新状态描述并通知界面。
         * @param message 状态描述。
         */
        void setStatus(const QString& message);

        /**
         * @brief  在 UI 线程接收抽取结果。
         * @param success 是否成功。
         * @param result 抽取结果。
         * @param errorMessage 失败原因。
         * @param cancelled 是否被取消。
         */
        void applyExtractionResult(bool success,
                                   ExtractionResult result,
                                   const QString& errorMessage,
                                   bool cancelled);

        /** 依据 m_result 重建每个序列的 LOD 与摘要。 */
        void rebuildSeriesIndex();

        /**
         * @brief  设置叠加曲线（内部做均匀抽稀以控制绘图点数）。
         * @param  x 横轴数组。
         * @param  y 纵轴数组。
         * @param  label 图例文本。
         */
        void setOverlay(const std::vector<double>& x,
                        const std::vector<double>& y,
                        const QString& label);

        /**
         * @brief  组装实体键。
         * @param  ref 实体引用。
         * @return `<kind>:<id>`。
         */
        [[nodiscard]] static QString entityKey(const EntityRef& ref);

        /**
         * @brief  解析实体键。
         * @param  key 实体键。
         * @return 解析成功返回值，失败返回 std::nullopt。
         */
        [[nodiscard]] static std::optional<EntityRef> parseEntityKey(const QString& key);

        /**
         * @brief  组装序列键。
         * @param  ref 实体引用。
         * @param  channel 通道。
         * @return `<实体键>/<通道键>`。
         */
        [[nodiscard]] static QString seriesKey(const EntityRef& ref, Channel channel);

        /** 当前打开的文件路径。 */
        QString m_sourcePath{};

        /** 场景目录。 */
        SceneCatalog m_catalog{};

        /** 抽取结果。 */
        ExtractionResult m_result{};

        /** 序列键 → LOD 条目。 */
        QHash<QString, LodEntry> m_lod{};

        /** 序列摘要缓存。 */
        QVariantList m_summary{};

        /** 实体列表缓存。 */
        QVariantList m_entities{};

        /** 通道列表缓存。 */
        QVariantList m_channels{};

        /** 是否正在执行后台任务。 */
        bool m_busy{false};

        /** 当前任务进度比例。 */
        double m_progress{0.0};

        /** 状态描述。 */
        QString m_statusMessage{};

        /** 错误描述。 */
        QString m_errorMessage{};

        /** 拟合结果摘要。 */
        QVariantMap m_fitInfo{};

        /** 误差对比结果摘要。 */
        QVariantMap m_comparisonInfo{};

        /** 叠加曲线横轴。 */
        std::vector<double> m_overlayX{};

        /** 叠加曲线纵轴。 */
        std::vector<double> m_overlayY{};

        /** 叠加曲线图例文本。 */
        QString m_overlayLabel{};

        /** 取消标志（UI 线程置位、工作线程读取）。 */
        std::shared_ptr<std::atomic<bool>> m_cancelFlag{
            std::make_shared<std::atomic<bool>>(false)};
    };
}

#endif // EGRET_PHYSICS_ANALYSIS_SESSION_H
