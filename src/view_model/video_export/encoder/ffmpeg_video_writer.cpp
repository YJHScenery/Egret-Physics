/**
 * @file        ffmpeg_video_writer.cpp
 * @brief       FFmpeg 编码器实现（libavcodec + libavformat + libswscale）。
 * @details     关键设计：
 *
 *              1. 编码在对象所属线程内异步推进：writeFrame() 只入队并立即返回，
 *                 真正的编码由 `QTimer::singleShot(0, ...)` 排队执行，从而让
 *                 「渲染下一帧」与「编码当前帧」真正并行；
 *              2. 通过 frameRequested() 做反压：队列低于容量才请求新帧，避免无限堆积内存；
 *              3. 时间戳直接用输出帧序号（time_base = 1/(fps×1000)），不做累加，
 *                 因此不会因为逐帧耗时抖动产生漂移；
 *              4. 结束时先冲空编码器（送入 nullptr），再写文件尾，保证 MP4 索引完整。
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

#include "ffmpeg_video_writer.h"

#include <QFileInfo>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <tuple>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include "async_logger.h"

namespace egret::video
{
    namespace
    {
        /** 自动码率系数：每像素每帧的比特数（1080p60 约 10 Mbps）。 */
        constexpr double BITS_PER_PIXEL_PER_FRAME{0.08};

        /** 自动码率下限（kbps）。 */
        constexpr int MIN_BITRATE_KBPS{2000};

        /** 自动码率上限（kbps）。 */
        constexpr int MAX_BITRATE_KBPS{60000};

        /** 关键帧间隔（秒）。 */
        constexpr double KEYFRAME_INTERVAL_SECONDS{2.0};

        /**
         * @brief  把浮点帧率转成 AVRational。
         * @param  frameRate 帧率（fps）。
         * @return 最简分数表示（例如 30 → 30/1，29.97 → 2997/100）。
         * @remark 时间基取该分数的倒数：一帧恰好占用 time_base 的一个单位，
         *         因此帧时间戳可以直接写帧序号，不会积累舍入误差。
         */
        AVRational frameRateToRational(const double frameRate)
        {
            const int scaled{std::max(1, static_cast<int>(std::lround(frameRate * 1000.0)))};
            AVRational result{scaled, 1000};
            av_reduce(&result.num, &result.den, scaled, 1000, 1000000);
            return result;
        }
    }

    FfmpegVideoWriter::FfmpegVideoWriter(QObject* parent) :
        QObject(parent)
    {
    }

    FfmpegVideoWriter::~FfmpegVideoWriter()
    {
        release();
    }

    QList<QString> FfmpegVideoWriter::candidateEncoders()
    {
        QList<QString> names{};
        for (const char* const name : {"h264_nvenc", "h264_amf", "h264_qsv", "h264_mf", "libx264", "h264"}) {
            if (avcodec_find_encoder_by_name(name) != nullptr) {
                names.append(QString::fromLatin1(name));
            }
        }
        return names;
    }

    QString FfmpegVideoWriter::usedEncoderName() const
    {
        return m_usedEncoderName;
    }

    std::uint64_t FfmpegVideoWriter::encodedFrameCount() const
    {
        return m_encodedFrames;
    }

    bool FfmpegVideoWriter::isRunning() const
    {
        return m_running;
    }

    QString FfmpegVideoWriter::errorText(const int errorCode)
    {
        char buffer[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(errorCode, buffer, sizeof(buffer));
        return QStringLiteral("FFmpeg 错误 %1：%2").arg(errorCode).arg(QString::fromUtf8(buffer));
    }

    bool FfmpegVideoWriter::start(const Settings& settings, QString* errorMessage)
    {
        if (m_running || m_reported) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("编码器已经启动");
            }
            return false;
        }

        m_settings = settings;
        m_settings.queueCapacity = std::max(1, m_settings.queueCapacity);
        m_failed = false;
        m_finishing = false;
        m_reported = false;
        m_encodedFrames = 0;
        m_pendingFrames.clear();

        if (!openEncoder(errorMessage)) {
            release();
            return false;
        }
        if (!openOutput(errorMessage)) {
            release();
            return false;
        }
        if (!prepareFrame(errorMessage)) {
            release();
            return false;
        }

        m_running = true;
        LOG_INFO("视频编码器：" + m_usedEncoderName.toStdString());
        emit frameRequested();
        return true;
    }

    bool FfmpegVideoWriter::openEncoder(QString* errorMessage)
    {
        // 只输出错误级别日志，避免 FFmpeg 的提示刷屏。
        av_log_set_level(AV_LOG_ERROR);

        QList<QString> candidates{};
        if (!m_settings.encoderName.isEmpty()) {
            candidates.append(m_settings.encoderName);
        }
        for (const QString& name : candidateEncoders()) {
            if (!candidates.contains(name)) {
                candidates.append(name);
            }
        }

        QString lastError{};
        for (const QString& name : candidates) {
            const AVCodec* const codec{avcodec_find_encoder_by_name(name.toUtf8().constData())};
            if (codec == nullptr) {
                continue;
            }
            m_codec = codec;
            m_usedEncoderName = name;
            return true;
        }

        if (errorMessage != nullptr) {
            *errorMessage = lastError.isEmpty()
                                ? QStringLiteral("找不到可用的 H.264 编码器（检查 dependency/bin/ffmpeg 是否部署）")
                                : lastError;
        }
        return false;
    }

    bool FfmpegVideoWriter::openOutput(QString* errorMessage)
    {
        const QByteArray outputPath{QFileInfo{m_settings.outputPath}.absoluteFilePath().toUtf8()};

        int result{avformat_alloc_output_context2(&m_formatContext, nullptr, nullptr, outputPath.constData())};
        if (result < 0 || m_formatContext == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("无法创建输出容器：%1").arg(errorText(result));
            }
            return false;
        }

        m_stream = avformat_new_stream(m_formatContext, nullptr);
        if (m_stream == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("无法创建输出流");
            }
            return false;
        }

        // FFmpeg 9 已移除 AVCodec::pix_fmts，因此按硬件/软件编码器常见的像素格式顺序尝试，
        // 每次尝试都用全新的上下文（avcodec_open2 失败后不能复用同一个上下文）。
        const AVRational frameRate{frameRateToRational(m_settings.frameRate)};
        int bitRateKbps{m_settings.bitRateKbps};
        if (bitRateKbps <= 0) {
            const double bitsPerSecond{static_cast<double>(m_settings.resolution.width())
                                       * static_cast<double>(m_settings.resolution.height())
                                       * m_settings.frameRate * BITS_PER_PIXEL_PER_FRAME
                                       * std::max(0.05, m_settings.bitRateScale)};
            bitRateKbps = std::clamp(static_cast<int>(std::lround(bitsPerSecond / 1000.0)),
                                     MIN_BITRATE_KBPS,
                                     MAX_BITRATE_KBPS);
        }

        QString preset{m_settings.preset};
        if (preset.isEmpty()) {
            if (m_usedEncoderName.contains(QStringLiteral("nvenc"))) {
                preset = QStringLiteral("p5");
            }
            else if (m_usedEncoderName.contains(QStringLiteral("x264"))) {
                preset = QStringLiteral("veryfast");
            }
        }

        QList<AVPixelFormat> pixelFormats{};
        if (m_usedEncoderName.contains(QStringLiteral("nvenc"))
            || m_usedEncoderName.contains(QStringLiteral("qsv"))
            || m_usedEncoderName.contains(QStringLiteral("amf"))
            || m_usedEncoderName.contains(QStringLiteral("mf"))) {
            pixelFormats = {AV_PIX_FMT_NV12, AV_PIX_FMT_YUV420P};
        }
        else {
            pixelFormats = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NV12};
        }

        result = AVERROR_ENCODER_NOT_FOUND;
        for (const AVPixelFormat pixelFormat : pixelFormats) {
            AVCodecContext* context{avcodec_alloc_context3(m_codec)};
            if (context == nullptr) {
                continue;
            }

            context->width = m_settings.resolution.width();
            context->height = m_settings.resolution.height();
            context->framerate = frameRate;
            context->time_base = AVRational{frameRate.den, frameRate.num};   // 一帧一个时间单位
            context->pix_fmt = pixelFormat;
            context->gop_size = std::max(1, static_cast<int>(
                                                std::lround(m_settings.frameRate * KEYFRAME_INTERVAL_SECONDS)));
            context->bit_rate = static_cast<int64_t>(bitRateKbps) * 1000;
            context->max_b_frames = 0;
            if (!preset.isEmpty()) {
                av_opt_set(context->priv_data, "preset", preset.toUtf8().constData(), 0);
            }
            if ((m_formatContext->oformat->flags & AVFMT_GLOBALHEADER) != 0) {
                context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

            result = avcodec_open2(context, m_codec, nullptr);
            if (result >= 0) {
                m_codecContext = context;
                break;
            }
            avcodec_free_context(&context);
        }

        if (m_codecContext == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("打开编码器 %1 失败：%2")
                                    .arg(m_usedEncoderName, errorText(result));
            }
            return false;
        }

        result = avcodec_parameters_from_context(m_stream->codecpar, m_codecContext);
        if (result < 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("导出编码参数失败：%1").arg(errorText(result));
            }
            return false;
        }
        m_stream->time_base = m_codecContext->time_base;

        if ((m_formatContext->oformat->flags & AVFMT_NOFILE) == 0) {
            result = avio_open(&m_formatContext->pb, outputPath.constData(), AVIO_FLAG_WRITE);
            if (result < 0) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("无法打开输出文件 %1：%2")
                                        .arg(m_settings.outputPath, errorText(result));
                }
                return false;
            }
        }

        result = avformat_write_header(m_formatContext, nullptr);
        if (result < 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("写入文件头失败：%1").arg(errorText(result));
            }
            return false;
        }
        m_headerWritten = true;
        return true;
    }

    bool FfmpegVideoWriter::prepareFrame(QString* errorMessage)
    {
        m_frame = av_frame_alloc();
        m_packet = av_packet_alloc();
        if (m_frame == nullptr || m_packet == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("分配 AVFrame / AVPacket 失败");
            }
            return false;
        }

        m_frame->format = m_codecContext->pix_fmt;
        m_frame->width = m_codecContext->width;
        m_frame->height = m_codecContext->height;
        const int bufferResult{av_frame_get_buffer(m_frame, 32)};
        if (bufferResult < 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("分配编码帧缓冲失败：%1").arg(errorText(bufferResult));
            }
            return false;
        }

        m_swsContext = sws_getContext(m_codecContext->width,
                                      m_codecContext->height,
                                      AV_PIX_FMT_RGBA,
                                      m_codecContext->width,
                                      m_codecContext->height,
                                      m_codecContext->pix_fmt,
                                      SWS_FAST_BILINEAR,
                                      nullptr,
                                      nullptr,
                                      nullptr);
        if (m_swsContext == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("创建像素格式转换器失败（RGBA → YUV）");
            }
            return false;
        }
        return true;
    }

    void FfmpegVideoWriter::writeFrame(const QVideoFrame& frame)
    {
        if (!m_running || m_finishing || m_reported || !frame.isValid()) {
            return;
        }

        m_pendingFrames.enqueue(frame);
        scheduleEncode();

        // 队列未满就继续要帧，让渲染线程保持满载。
        if (m_pendingFrames.size() < m_settings.queueCapacity) {
            emit frameRequested();
        }
    }

    void FfmpegVideoWriter::finish()
    {
        if (!m_running || m_finishing || m_reported) {
            return;
        }
        m_finishing = true;
        scheduleEncode();   // 排队中的帧先编完，队列排空后自动收尾
    }

    void FfmpegVideoWriter::abort(const QString& reason)
    {
        if (m_reported) {
            return;
        }
        m_pendingFrames.clear();
        reportFailure(reason.isEmpty() ? QStringLiteral("导出已中止") : reason);
    }

    void FfmpegVideoWriter::scheduleEncode()
    {
        if (m_encodeScheduled || m_reported) {
            return;
        }
        m_encodeScheduled = true;
        QTimer::singleShot(0, this,
                           [this]
                           {
                               m_encodeScheduled = false;
                               encodePendingFrames();
                           });
    }

    void FfmpegVideoWriter::encodePendingFrames()
    {
        if (m_reported || !m_running) {
            return;
        }

        while (!m_pendingFrames.isEmpty() && !m_reported) {
            const QVideoFrame frame{m_pendingFrames.dequeue()};
            QString error{};
            if (!encodeOne(frame, &error)) {
                reportFailure(error);
                return;
            }
            ++m_encodedFrames;
        }

        if (m_finishing) {
            closeOutput();
            return;
        }
        if (m_pendingFrames.size() < m_settings.queueCapacity) {
            emit frameRequested();
        }
    }

    bool FfmpegVideoWriter::encodeOne(const QVideoFrame& frame, QString* errorMessage)
    {
        QVideoFrame mapped{frame};   // 隐式共享副本：映射只影响本副本
        if (!mapped.map(QVideoFrame::ReadOnly)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("无法映射视频帧缓冲");
            }
            return false;
        }

        const uint8_t* sourceData[4]{mapped.bits(0), nullptr, nullptr, nullptr};
        const int sourceStride[4]{mapped.bytesPerLine(0), 0, 0, 0};
        sws_scale(m_swsContext,
                  sourceData,
                  sourceStride,
                  0,
                  m_codecContext->height,
                  m_frame->data,
                  m_frame->linesize);
        mapped.unmap();

        m_frame->pts = static_cast<int64_t>(m_encodedFrames);

        const int sendResult{avcodec_send_frame(m_codecContext, m_frame)};
        if (sendResult < 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("提交编码帧失败：%1").arg(errorText(sendResult));
            }
            return false;
        }
        return drainPackets(errorMessage);
    }

    bool FfmpegVideoWriter::drainPackets(QString* errorMessage)
    {
        while (true) {
            const int receiveResult{avcodec_receive_packet(m_codecContext, m_packet)};
            if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) {
                return true;
            }
            if (receiveResult < 0) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("取出编码码流失败：%1").arg(errorText(receiveResult));
                }
                return false;
            }

            av_packet_rescale_ts(m_packet, m_codecContext->time_base, m_stream->time_base);
            m_packet->stream_index = m_stream->index;
            const int writeResult{av_interleaved_write_frame(m_formatContext, m_packet)};
            av_packet_unref(m_packet);
            if (writeResult < 0) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("写入码流失败：%1").arg(errorText(writeResult));
                }
                return false;
            }
        }
    }

    void FfmpegVideoWriter::closeOutput()
    {
        QString error{};
        if (m_codecContext != nullptr && m_headerWritten) {
            const int flushResult{avcodec_send_frame(m_codecContext, nullptr)};
            if (flushResult >= 0) {
                const bool drained{drainPackets(&error)};
                std::ignore = drained;
            }
            if (m_formatContext != nullptr) {
                av_write_trailer(m_formatContext);
            }
        }

        const bool success{!m_failed};
        release();
        if (!m_reported) {
            m_reported = true;
            emit finished(success, success ? QString{} : error);
        }
    }

    void FfmpegVideoWriter::reportFailure(const QString& message)
    {
        if (m_reported) {
            return;
        }
        m_failed = true;
        LOG_ERROR(message.toStdString());

        // 尽量正常收尾，已写入的帧仍然可以播放。
        if (m_codecContext != nullptr && m_headerWritten) {
            QString ignored{};
            const int flushResult{avcodec_send_frame(m_codecContext, nullptr)};
            if (flushResult >= 0) {
                const bool drained{drainPackets(&ignored)};
                std::ignore = drained;
            }
            if (m_formatContext != nullptr) {
                av_write_trailer(m_formatContext);
            }
        }

        release();
        m_reported = true;
        emit finished(false, message);
    }

    void FfmpegVideoWriter::release()
    {
        if (m_swsContext != nullptr) {
            sws_freeContext(m_swsContext);
            m_swsContext = nullptr;
        }
        if (m_frame != nullptr) {
            av_frame_free(&m_frame);
        }
        if (m_packet != nullptr) {
            av_packet_free(&m_packet);
        }
        if (m_codecContext != nullptr) {
            avcodec_free_context(&m_codecContext);
        }
        if (m_formatContext != nullptr) {
            if (m_formatContext->pb != nullptr) {
                avio_closep(&m_formatContext->pb);
            }
            avformat_free_context(m_formatContext);
            m_formatContext = nullptr;
        }

        m_stream = nullptr;
        m_headerWritten = false;
        m_running = false;
        m_finishing = false;
        m_codec = nullptr;
        m_pendingFrames.clear();
    }
}
