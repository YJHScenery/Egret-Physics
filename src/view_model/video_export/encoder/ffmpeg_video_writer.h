/**
 * @file        ffmpeg_video_writer.h
 * @brief       基于自带 FFmpeg 9.0.2 的视频编码器（支持 NVENC 硬件编码）。
 * @details     与 VideoFrameWriter（Qt Multimedia 后端）保持相同的外部契约：
 *
 *              - `start()` 打开编码器与输出文件；
 *              - `writeFrame()` 送入一帧（内部排队、异步编码，通过 frameRequested() 反压）；
 *              - `finish()` 冲掉编码器并写完文件索引；
 *              - `abort()` 立即收尾。
 *
 *              编码链路：RGBA 回读帧 → swscale 转 NV12 → libavcodec 编码（默认 h264_nvenc）
 *              → libavformat 封装 MP4。与 Qt Multimedia 后端的区别在于编码器由本项目的
 *              FFmpeg 副本提供，因而可以使用 NVENC/AMF/QSV 等硬件编码器。
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

#ifndef EGRET_VIDEO_ENCODER_FFMPEG_VIDEO_WRITER_H
#define EGRET_VIDEO_ENCODER_FFMPEG_VIDEO_WRITER_H

#include <QList>
#include <QObject>
#include <QQueue>
#include <QSize>
#include <QString>
#include <QVideoFrame>

#include <cstdint>

struct AVCodecContext;
struct AVCodec;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
struct AVStream;
struct SwsContext;

namespace egret::video
{
    /**
     * @brief       使用 FFmpeg 直接编码视频文件。
     * @invariant   start() 成功返回后，finished() 至多发出一次；
     *              在 finished() 之后不得再调用 writeFrame()。
     */
    class FfmpegVideoWriter final : public QObject
    {
        Q_OBJECT

    public:
        /** 编码配置。 */
        struct Settings
        {
            /** 输出文件路径（本地路径）。 */
            QString outputPath{};

            /** 视频分辨率（必须是偶数）。 */
            QSize resolution{1920, 1080};

            /** 输出帧率。 */
            double frameRate{60.0};

            /** FFmpeg 编码器名，例如 h264_nvenc / libx264 / h264_mf。 */
            QString encoderName{QStringLiteral("h264_nvenc")};

            /** 目标码率（kbps）；0 表示按分辨率与帧率自动推算。 */
            int bitRateKbps{0};

            /** 自动码率的倍数（由质量档映射而来）。 */
            double bitRateScale{1.0};

            /** 编码器预设（如 NVENC 的 p1~p7、x264 的 veryfast）；空表示用默认值。 */
            QString preset{};

            /** 编码队列容量（帧），用于在渲染与编码之间形成流水线。 */
            int queueCapacity{4};
        };

        /** 默认构造（对象必须在即将使用它的线程中创建）。 */
        explicit FfmpegVideoWriter(QObject* parent = nullptr);

        /** 析构：释放所有 FFmpeg 资源。 */
        ~FfmpegVideoWriter() override;

        /**
         * @brief  列出本机可用的候选编码器（按优先级）。
         * @return 编码器名列表，例如 [h264_nvenc, libx264, h264_mf, h264]。
         * @remark 只做名称探测，不做实际初始化；实际可用性由 start() 决定。
         */
        [[nodiscard]] static QList<QString> candidateEncoders();

        /**
         * @brief  打开编码器与输出文件。
         * @param  settings 编码配置。
         * @param  errorMessage 失败原因输出，可为 nullptr。
         * @return 成功返回 true。
         * @remark 必须在对象所属线程中调用；重复调用会被忽略。
         */
        [[nodiscard]] bool start(const Settings& settings, QString* errorMessage);

        /** 实际生效的编码器名（start 成功后有效）。 */
        [[nodiscard]] QString usedEncoderName() const;

        /** 已成功编码的帧数。 */
        [[nodiscard]] std::uint64_t encodedFrameCount() const;

        /** 是否处于可写状态。 */
        [[nodiscard]] bool isRunning() const;

    public slots:
        /**
         * @brief 送入一帧画面。
         * @param frame 待编码帧（RGBA8888，时间戳由本类按帧序号推算）。
         */
        void writeFrame(const QVideoFrame& frame);

        /** 结束视频流并写完文件索引。 */
        void finish();

        /**
         * @brief 立即中止并收尾。
         * @param reason 中止原因。
         */
        void abort(const QString& reason);

    signals:
        /** 编码器已就绪，请求下一帧（拉模式反压）。 */
        void frameRequested();

        /**
         * @brief 编码结束。
         * @param success 成功为 true。
         * @param message 失败原因；成功时为空串。
         */
        void finished(bool success, const QString& message);

    private:
        /**
         * @brief 查找并配置 libavcodec 编码器（此时还不打开编码器）。
         * @param errorMessage 失败原因输出，可为 nullptr。
         * @return 成功返回 true。
         * @remark 必须先创建输出容器，才能知道是否需要 AV_CODEC_FLAG_GLOBAL_HEADER，
         *         因此真正的 avcodec_open2() 放在 openOutput() 中执行。
         */
        [[nodiscard]] bool openEncoder(QString* errorMessage);

        /**
         * @brief 创建输出容器、打开编码器、关联流参数并写入文件头。
         * @param errorMessage 失败原因输出，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool openOutput(QString* errorMessage);

        /**
         * @brief 准备 AVFrame 与 RGBA→YUV 转换器。
         * @param errorMessage 失败原因输出，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool prepareFrame(QString* errorMessage);

        /** 排队等待编码的帧如果为空则安排一次异步编码。 */
        void scheduleEncode();

        /** 把队列中的帧依次编码（在所属线程内执行）。 */
        void encodePendingFrames();

        /**
         * @brief 编码单帧。
         * @param frame 待编码帧。
         * @param errorMessage 失败原因输出，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool encodeOne(const QVideoFrame& frame, QString* errorMessage);

        /**
         * @brief 取出编码器内部已产出的码流并写入容器。
         * @param errorMessage 失败原因输出，可为 nullptr。
         * @return 成功返回 true。
         */
        bool drainPackets(QString* errorMessage);

        /** 收尾：冲编码器、写文件尾、释放资源。 */
        void closeOutput();

        /** 释放全部 FFmpeg 资源。 */
        void release();

        /**
         * @brief  把 FFmpeg 错误码转成可读文本。
         * @param  errorCode FFmpeg 返回码。
         * @return 中文前缀 + FFmpeg 原始描述。
         */
        [[nodiscard]] static QString errorText(int errorCode);

        /**
         * @brief 记录失败并发出 finished(false, ...)。
         * @param message 失败原因。
         */
        void reportFailure(const QString& message);

        /** 编码配置。 */
        Settings m_settings{};

        /** 实际使用的编码器名。 */
        QString m_usedEncoderName{};

        /** 待编码帧队列。 */
        QQueue<QVideoFrame> m_pendingFrames{};

        /** 编码器上下文。 */
        AVCodecContext* m_codecContext{nullptr};

        /** 选中的编码器实现。 */
        const AVCodec* m_codec{nullptr};

        /** 输出容器上下文。 */
        AVFormatContext* m_formatContext{nullptr};

        /** 输出流。 */
        AVStream* m_stream{nullptr};

        /** 编码用 AVFrame（目标像素格式）。 */
        AVFrame* m_frame{nullptr};

        /** 码流包。 */
        AVPacket* m_packet{nullptr};

        /** RGBA → NV12/YUV420P 转换器。 */
        SwsContext* m_swsContext{nullptr};

        /** 已编码帧数。 */
        std::uint64_t m_encodedFrames{0};

        /** 是否已写入文件头。 */
        bool m_headerWritten{false};

        /** 是否处于可写状态。 */
        bool m_running{false};

        /** 是否正在收尾。 */
        bool m_finishing{false};

        /** 是否已经发出 finished 信号。 */
        bool m_reported{false};

        /** 是否发生过失败。 */
        bool m_failed{false};

        /** 是否已经安排异步编码。 */
        bool m_encodeScheduled{false};
    };
}

#endif // EGRET_VIDEO_ENCODER_FFMPEG_VIDEO_WRITER_H
