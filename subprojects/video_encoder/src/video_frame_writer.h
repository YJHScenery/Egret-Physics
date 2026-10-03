/**
 * @file        video_frame_writer.h
 * @brief       逐帧视频编码器：把 QImage 序列写成 MP4/MOV 文件。
 * @details     VideoFrameWriter 封装 Qt Multimedia 的「自定义视频帧输入」链路：
 *
 *              QVideoFrameInput（帧输入） → QMediaCaptureSession（捕获会话） → QMediaRecorder（编码）
 *
 *              设计要点：
 *
 *              1. 采用「拉模式」：编码器空闲时发出 frameRequested()，调用方此时才渲染并送入下一帧，
 *                 因此不存在内部队列积压，也不需要丢弃帧；
 *              2. 结束流时发送一个空的 QVideoFrame（Qt 规定的流结束标志），并依赖 FFmpeg 后端的
 *                 autoStop 让录制器自动收尾，保证 MP4 的 moov 索引被正确写入；
 *              3. 本类需要在**工作线程**中创建并使用（对象 moveToThread 到该线程，线程提供事件循环），
 *                 所有跨线程调用均由调用方通过队列连接完成。
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

#ifndef EGRET_VIDEO_ENCODER_VIDEO_FRAME_WRITER_H
#define EGRET_VIDEO_ENCODER_VIDEO_FRAME_WRITER_H

#include <QList>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QObject>
#include <QSize>
#include <QString>
#include <QVideoFrame>
#include <QVideoFrameFormat>

class QMediaCaptureSession;
class QVideoFrameInput;

namespace egret::video
{
    /**
     * @brief       把渲染出来的视频帧编码为媒体文件。
     * @invariant   start() 成功返回后，frameRequested() 与 finished() 至多各产生一组；
     *              在 finished() 之后不得再调用 writeFrame()。
     */
    class VideoFrameWriter final : public QObject
    {
        Q_OBJECT

    public:
        /** 一次编码任务的参数。 */
        struct Settings
        {
            /** 输出文件路径（本地文件系统路径）。 */
            QString outputPath{};

            /** 视频分辨率（像素）。 */
            QSize resolution{1920, 1080};

            /** 视频帧率。 */
            double frameRate{60.0};

            /** 容器格式。 */
            QMediaFormat::FileFormat fileFormat{QMediaFormat::MPEG4};

            /** 视频编码。 */
            QMediaFormat::VideoCodec videoCodec{QMediaFormat::VideoCodec::H264};

            /** 质量档。 */
            QMediaRecorder::Quality quality{QMediaRecorder::HighQuality};
        };

        /** 默认构造（对象必须在即将使用它的线程中创建）。 */
        explicit VideoFrameWriter(QObject* parent = nullptr);

        /** 默认析构。 */
        ~VideoFrameWriter() override;

        /**
         * @brief  探测指定容器下运行期可用的视频编码器。
         * @param  fileFormat 目标容器格式。
         * @return 可用编码器列表；空列表表示当前后端不支持任何视频编码。
         * @remark 内部通过 QMediaFormat::supportedVideoCodecs(Encode) 查询真实后端能力，
         *         因此 H.265 在 Windows 上会被自动剔除、缺少硬件编码器时也会如实反映。
         */
        [[nodiscard]] static QList<QMediaFormat::VideoCodec> supportedVideoCodecs(QMediaFormat::FileFormat fileFormat);

        /**
         * @brief 开始录制。
         * @param settings 编码参数。
         * @remark 必须在对象所属线程中调用；重复调用会被忽略。
         */
        void start(const Settings& settings);

        /** 最近一次失败原因；无错误时为空串。 */
        [[nodiscard]] QString errorString() const;

    public slots:
        /**
         * @brief 送入一帧画面。
         * @param frame 待编码帧，时间戳由调用方设置。
         */
        void writeFrame(const QVideoFrame& frame);

        /**
         * @brief 结束视频流并收尾文件。
         * @remark 会先发送流结束标志，再等待录制器自行停止；必要时在超时后强制停止。
         */
        void finish();

        /**
         * @brief 立即中止录制。
         * @param reason 中止原因，会随 finished(false, reason) 一起发出。
         */
        void abort(const QString& reason);

    signals:
        /** 录制已启动，可以开始送帧。 */
        void started();

        /** 编码器已就绪，请求下一帧（拉模式）。 */
        void frameRequested();

        /**
         * @brief 录制结束（正常收尾或失败）。
         * @param success 成功为 true。
         * @param message 失败原因；成功时为空串。
         */
        void finished(bool success, const QString& message);

    private:
        /**
         * @brief 记录失败并发出 finished(false, ...)。
         * @param message 失败原因。
         */
        void reportFailure(const QString& message);

        /** 录制会话（Qt 父子机制持有）。 */
        QMediaCaptureSession* m_session{nullptr};

        /** 自定义帧输入（Qt 父子机制持有）。 */
        QVideoFrameInput* m_input{nullptr};

        /** 录制器（Qt 父子机制持有）。 */
        QMediaRecorder* m_recorder{nullptr};

        /** 送入帧的格式（与渲染分辨率一致）。 */
        QVideoFrameFormat m_frameFormat{};

        /** 是否已经进入收尾流程。 */
        bool m_finishing{false};

        /** 是否已经发出 finished 信号。 */
        bool m_reported{false};

        /** 是否发生过失败。 */
        bool m_failed{false};

        /** 最近一次失败原因。 */
        QString m_errorMessage{};
    };
}

#endif // EGRET_VIDEO_ENCODER_VIDEO_FRAME_WRITER_H
