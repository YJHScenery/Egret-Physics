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
#include <QObject>
#include <QSize>
#include <QString>

#include <memory>

#include "video_encoder_options.h"

class QImage;
class QThread;

namespace egret
{
    class SceneManagerViewModel;
}

namespace egret::video
{
    class OffscreenSceneRenderer;
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
        /** 响应编码线程的「请求下一帧」：套用源帧状态 → 渲染 → 回读 → 送帧。 */
        void handleFrameRequest();

        /** 编码线程结束后的汇总。 */
        void handleWriterFinished(bool success, const QString& message);

        /** 停止并回收编码线程。 */
        void stopWorkerThread();

        /** 渲染失败等致命错误的统一处理。 */
        void failWith(const QString& message);

        /**
         * @brief 把渲染结果组装成带时间戳的 QVideoFrame 并投递到编码线程。
         * @param image 渲染结果（RGBA8888）。
         * @return 成功投递返回 true；失败时会自行触发失败收尾。
         */
        [[nodiscard]] bool dispatchFrame(const QImage& image);

        /** 导出参数。 */
        Options m_options{};

        /** 导出结果。 */
        Report m_report{};

        /** 导出专用场景控制器（与播放器互不影响）。 */
        std::unique_ptr<egret::SceneManagerViewModel> m_scene{};

        /** 离屏渲染器。 */
        std::unique_ptr<OffscreenSceneRenderer> m_renderer{};

        /**
         * @brief 编码器对象。
         * @remark 它属于编码工作线程，退出流程由线程结束时触发的 deleteLater() 负责释放，
         *         因此这里只保存指针，不参与智能指针管理。
         */
        VideoFrameWriter* m_writer{nullptr};

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
