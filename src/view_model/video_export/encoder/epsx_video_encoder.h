/**
 * @file        epsx_video_encoder.h
 * @brief       epsx → 视频的导出任务：串联场景重建、离屏渲染与逐帧编码。
 * @details     一次导出的完整链路：
 *
 *              1. 用独立的 SceneManagerViewModel 打开 epsx 文件（与播放器同一条数据链路）；
 *              2. 建立离屏渲染器（分辨率、相机、网格开关来自命令行参数）；
 *              3. 在工作线程中创建 VideoFrameWriter，采用「拉模式」驱动整个导出过程：
 *                 编码器请求下一帧 → 主线程套用源帧状态并渲染 → 回读 → 送入编码线程；
 *              4. 输出帧数、时间轴与 epsx 播放完全一致（时长 = 帧数 × 采样间隔），
 *                 源帧号按 floor(时刻 / 采样间隔) 选取，与播放器 advancePlayback() 同一规则。
 *
 * @author      jehor
 * @date        2026-10-03
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     VideoEncoder
 * @defgroup    VideoEncoder (EPSX 视频导出)
 */

#ifndef EGRET_VIDEO_ENCODER_EPSX_VIDEO_ENCODER_H
#define EGRET_VIDEO_ENCODER_EPSX_VIDEO_ENCODER_H

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QSize>
#include <QString>
#include <QVideoFrame>

#include <memory>
#include <vector>

#include "video_encoder_options.h"

class QThread;

namespace egret
{
    class SceneManagerViewModel;
}

namespace egret::video
{
    class FfmpegVideoWriter;
    class OffscreenSceneRenderer;
    class RenderLane;
    class VideoFrameWriter;

    /**
     * @brief       一次 epsx 视频导出的执行器。
     * @invariant   start() 成功返回后，一定会收到一次 finished() 信号；
     *              期间可以通过 cancel() 请求提前结束（文件仍会正常收尾）。
     */
    class EpsxVideoEncoder final : public QObject
    {
        Q_OBJECT

    public:
        /** 导出结果统计。 */
        struct Report
        {
            /** 输出文件路径。 */
            QString outputPath{};

            /** 源 epsx 的帧数。 */
            quint64 sourceFrames{0};

            /** 实际写入视频的帧数。 */
            quint64 outputFrames{0};

            /** 视频时长（秒），等于「源帧数 × 采样间隔」。 */
            double durationSeconds{0.0};

            /** 输出帧率。 */
            double frameRate{0.0};

            /** 输出分辨率。 */
            QSize resolution{};

            /** 渲染与编码消耗的真实时间（秒）。 */
            double wallClockSeconds{0.0};

            /** 输出文件大小（字节）。 */
            quint64 fileBytes{0};

            /** 是否因调用 cancel() 而提前结束。 */
            bool canceled{false};

            /** 是否只是首帧预览（未进行视频编码）。 */
            bool previewOnly{false};
        };

        /** 默认构造。 */
        explicit EpsxVideoEncoder(QObject* parent = nullptr);

        /** 析构：确保工作线程已经安全退出。 */
        ~EpsxVideoEncoder() override;

        /**
         * @brief  校验参数、打开 epsx、建立离屏场景并启动编码线程。
         * @param  options 导出参数。
         * @param  errorMessage 失败原因输出，可为 nullptr。
         * @return 成功启动返回 true；此时导出在事件循环中异步推进。
         */
        [[nodiscard]] bool start(const Options& options, QString* errorMessage);

        /**
         * @brief 请求取消导出。
         * @remark 当前帧结束后停止，已写入的帧仍然有效，文件会正常收尾。
         */
        void cancel();

        /** 是否正在导出。 */
        [[nodiscard]] bool isRunning() const;

        /** 最近一次导出的结果统计。 */
        [[nodiscard]] const Report& report() const;

        /**
         * @brief  生成逐阶段耗时统计文本。
         * @return 多行中文统计文本，用于定位「渲染慢」还是「编码慢」。
         */
        [[nodiscard]] QString timingSummary() const;

    signals:
        /**
         * @brief 导出进度。
         * @param renderedFrames 已渲染并送入编码器的帧数。
         * @param totalFrames 计划输出的总帧数。
         */
        void progress(quint64 renderedFrames, quint64 totalFrames);

        /**
         * @brief 导出结束。
         * @param success 成功为 true。
         * @param message 失败原因或取消说明；成功时为空串。
         */
        void finished(bool success, const QString& message);

    private:
        /** 响应编码线程的「请求下一帧」。 */
        void handleFrameRequest();

        /**
         * @brief  推进流水线：补满空闲通道，并在编码器需要时按序送帧。
         * @remark 帧的送出顺序严格等于输出帧号顺序，与通道完成顺序无关。
         */
        void pumpPipeline();

        /** 把后续帧分派给空闲通道。 */
        void scheduleFrames();

        /** 若有序帧已就绪且编码器在等待，则送出该帧。 */
        void sendNextReadyFrame();

        /** 某个通道完成一帧。 */
        void handleLaneFrame(quint64 outputIndex, const QVideoFrame& frame);

        /** 某个通道渲染失败。 */
        void handleLaneFailure(quint64 outputIndex, const QString& message);

        /** 停止并回收所有渲染通道。 */
        void stopLanes();

        /**
         * @brief  按配置创建并行渲染通道（含各自线程）。
         * @param  camera 相机姿态。
         * @param  errorMessage 失败原因输出，可为 nullptr。
         * @return 至少创建出一个通道时返回 true。
         * @remark 某个通道初始化失败时会退化为更少通道继续运行。
         */
        [[nodiscard]] bool createLanes(const CameraPose& camera, QString* errorMessage);

        /** 判断是否可以进入收尾流程。 */
        void checkCompletion();

        /** 当前是否有可用的编码器对象。 */
        [[nodiscard]] bool hasWriter() const;

        /** 把一帧交给当前编码后端。 */
        void pushFrameToWriter(const QVideoFrame& frame);

        /** 请求当前编码后端结束流并收尾。 */
        void requestWriterFinish();

        /** 请求当前编码后端中止并收尾。 */
        void requestWriterAbort(const QString& message);

        /**
         * @brief  由输出帧号推算源 epsx 帧号。
         * @param  outputIndex 输出帧序号。
         * @return 源帧号（与播放器 advancePlayback() 相同的 floor 规则）。
         */
        [[nodiscard]] int sourceFrameForOutputIndex(quint64 outputIndex) const;

        /** 编码线程结束后的汇总。 */
        void handleWriterFinished(bool success, const QString& message);

        /** 停止并回收编码线程。 */
        void stopWorkerThread();

        /** 渲染失败等致命错误的统一处理。 */
        void failWith(const QString& message);

        /** 导出参数。 */
        Options m_options{};

        /** 导出结果。 */
        Report m_report{};

        /**
         * @brief 仅用于读取 epsx 元数据（帧数、采样间隔）的临时场景控制器。
         * @remark 通道建立后即释放，不参与渲染。
         */
        std::unique_ptr<egret::SceneManagerViewModel> m_metadataScene{};

        /**
         * @brief 并行渲染通道。
         * @remark 通道对象由通道线程在退出前通过 deleteLater() 释放，这里只保存指针。
         */
        std::vector<RenderLane*> m_lanes{};

        /**
         * @brief 运行在调用线程（GUI 线程）中的通道。
         * @remark 单通道时使用：Qt 的 QML 场景对象必须在 GUI 线程创建，
         *         在工作线程创建 QQuickWindow 会触发线程亲和性告警并显著变慢。
         */
        std::vector<std::unique_ptr<RenderLane>> m_mainThreadLanes{};

        /** 各通道所属线程。 */
        std::vector<std::unique_ptr<QThread>> m_laneThreads{};

        /** 各通道当前是否在渲染。 */
        std::vector<bool> m_laneBusy{};

        /** 各通道正在渲染的输出帧号。 */
        std::vector<quint64> m_laneFrameIndex{};

        /** 预览模式使用的单通道（运行在主线程）。 */
        std::unique_ptr<RenderLane> m_previewLane{};

        /** 已渲染完成、等待按序送出的帧。 */
        QHash<quint64, QVideoFrame> m_readyFrames{};

        /** 下一个待分派的输出帧号。 */
        quint64 m_nextToSchedule{0};

        /** 下一个待送出的输出帧号。 */
        quint64 m_nextToSend{0};

        /** 正在渲染的通道数量。 */
        int m_busyLaneCount{0};

        /** 编码器是否正在等待下一帧。 */
        bool m_writerWantsFrame{false};

        /** 实际启用的通道数量。 */
        int m_laneCount{1};

        /** 收尾前汇总的通道耗时（毫秒），用于在通道销毁后仍能输出统计。 */
        double m_laneApplyMs{0.0};
        double m_laneSyncMs{0.0};
        double m_laneRenderMs{0.0};
        double m_laneReadbackMs{0.0};
        double m_laneImageMs{0.0};

        /** 实际使用的通道数量（通道销毁后仍保留，供统计输出）。 */
        int m_laneCountUsed{0};

        /** 上一帧投递完成到编码器再次请求之间的计时器（编码器周转时间）。 */
        QElapsedTimer m_encoderTurnaroundTimer{};

        /** 编码器周转累计耗时（毫秒）。 */
        double m_encoderTurnaroundMs{0.0};

        /**
         * @brief 编码器对象。
         * @remark 它属于编码工作线程，退出流程由线程结束时触发的 deleteLater() 负责释放，
         *         因此这里只保存指针，不参与智能指针管理。
         */
        VideoFrameWriter* m_writer{nullptr};

        /**
         * @brief FFmpeg 编码器对象。
         * @remark 与 m_writer 一样属于编码线程，由线程结束时的 deleteLater() 释放。
         */
        FfmpegVideoWriter* m_ffmpegWriter{nullptr};

        /** 编码工作线程。 */
        std::unique_ptr<QThread> m_writerThread{};

        /** 计时器（统计真实耗时）。 */
        QElapsedTimer m_clock{};

        /** 计划输出的总帧数。 */
        quint64 m_totalFrames{0};

        /** 已渲染并送出的帧数。 */
        quint64 m_renderedFrames{0};

        /** 实际使用的输出帧率。 */
        double m_frameRate{0.0};

        /** 源 epsx 的采样间隔（秒）。 */
        double m_sampleIntervalSeconds{0.0};

        /** 是否已请求取消。 */
        bool m_cancelled{false};

        /** 是否已经进入收尾流程。 */
        bool m_finishing{false};

        /** 是否已经发出 finished 信号。 */
        bool m_finished{false};
    };
}

#endif // EGRET_VIDEO_ENCODER_EPSX_VIDEO_ENCODER_H
