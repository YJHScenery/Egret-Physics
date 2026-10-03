/**
 * @file        video_frame_writer.cpp
 * @brief       逐帧视频编码器实现。
 * @details     录制器、捕获会话与帧输入全部在对象所属线程中创建，因此本类可以直接
 *              moveToThread 到工作线程使用，主线程只需要通过队列连接送帧与收尾。
 *              收尾流程依赖 FFmpeg 后端的 autoStop：送入空帧表示流结束，录制器自行停止
 *              并写完文件索引；若后端没有自动停止，则由定时器兜底强制停止。
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

#include "video_frame_writer.h"

#include <QMediaCaptureSession>
#include <QTimer>
#include <QUrl>
#include <QVideoFrameInput>

#include "async_logger.h"

namespace egret::video
{
    namespace
    {
        /** 等待录制器自动收尾的最长时间（毫秒）。 */
        constexpr int FINALIZE_TIMEOUT_MS{3000};

        /** 发送流结束标志的重试间隔（毫秒）。 */
        constexpr int END_OF_STREAM_RETRY_MS{20};

        /** 发送流结束标志的最大重试次数（约 2 秒）。 */
        constexpr int END_OF_STREAM_MAX_ATTEMPTS{100};

        /** EOS 送出后等待 autoStop 的宽限时间（毫秒）。 */
        constexpr int AUTO_STOP_GRACE_MS{300};
    }

    VideoFrameWriter::VideoFrameWriter(QObject* parent) :
        QObject(parent)
    {
    }

    VideoFrameWriter::~VideoFrameWriter() = default;

    QString VideoFrameWriter::errorString() const
    {
        return m_errorMessage;
    }

    QList<QMediaFormat::VideoCodec> VideoFrameWriter::supportedVideoCodecs(const QMediaFormat::FileFormat fileFormat)
    {
        QMediaFormat format{fileFormat};
        return format.supportedVideoCodecs(QMediaFormat::Encode);
    }

    void VideoFrameWriter::start(const Settings& settings)
    {
        if (m_recorder != nullptr || m_failed) {
            return;
        }

        // 与渲染分辨率严格一致的帧格式：编码器可以据此提前初始化，避免逐帧格式转换。
        m_frameFormat = QVideoFrameFormat{settings.resolution, QVideoFrameFormat::Format_RGBA8888};
        // 明确告知编码器流帧率，否则 FFmpeg 引擎会以 30 fps 作为缺省值并打印告警。
        m_frameFormat.setStreamFrameRate(settings.frameRate);

        // 以下三个对象均为本对象的子对象，由 Qt 父子机制负责释放。
        m_input = new QVideoFrameInput{m_frameFormat, this};
        m_session = new QMediaCaptureSession{this};
        m_recorder = new QMediaRecorder{this};

        m_session->setVideoFrameInput(m_input);
        m_session->setRecorder(m_recorder);

        QMediaFormat mediaFormat{settings.fileFormat};
        mediaFormat.setVideoCodec(settings.videoCodec);

        m_recorder->setMediaFormat(mediaFormat);
        m_recorder->setQuality(settings.quality);
        m_recorder->setVideoFrameRate(settings.frameRate);
        m_recorder->setVideoResolution(settings.resolution);
        m_recorder->setAutoStop(true);
        m_recorder->setOutputLocation(QUrl::fromLocalFile(settings.outputPath));

        connect(m_input, &QVideoFrameInput::readyToSendVideoFrame, this, &VideoFrameWriter::frameRequested);
        connect(m_recorder, &QMediaRecorder::errorOccurred, this,
                [this](const QMediaRecorder::Error, const QString& message)
                {
                    reportFailure(message.isEmpty() ? QStringLiteral("视频编码失败") : message);
                });
        connect(m_recorder, &QMediaRecorder::recorderStateChanged, this,
                [this](const QMediaRecorder::RecorderState state)
                {
                    if (state != QMediaRecorder::StoppedState || !m_finishing || m_reported) {
                        return;
                    }
                    m_reported = true;
                    emit finished(!m_failed, m_failed ? m_errorMessage : QString{});
                });

        emit started();
        m_recorder->record();
    }

    void VideoFrameWriter::writeFrame(const QVideoFrame& frame)
    {
        if (m_input == nullptr || m_failed || m_finishing) {
            return;
        }

        if (!m_input->sendVideoFrame(frame)) {
            // 拉模式下不应发生：只有内部队列满时才会失败。
            LOG_WARN_LITERAL("视频编码器内部队列已满，当前帧被丢弃");
        }
    }

    void VideoFrameWriter::finish()
    {
        if (m_recorder == nullptr || m_finishing) {
            return;
        }
        m_finishing = true;
        trySendEndOfStream(0);
    }

    void VideoFrameWriter::trySendEndOfStream(const int attempt)
    {
        if (m_reported) {
            return;
        }

        // 空帧是 Qt 规定的「流结束」标志；配合 autoStop(true)，录制器会自行收尾。
        if (m_input != nullptr && m_input->sendVideoFrame(QVideoFrame{})) {
            if (m_recorder != nullptr && m_recorder->recorderState() == QMediaRecorder::StoppedState) {
                if (!m_reported) {
                    m_reported = true;
                    emit finished(!m_failed, m_failed ? m_errorMessage : QString{});
                }
                return;
            }
            scheduleFinalizeTimeout();
            return;
        }

        if (attempt >= END_OF_STREAM_MAX_ATTEMPTS) {
            LOG_WARN_LITERAL("多次尝试后仍无法送出流结束标志，直接停止录制器");
            if (m_recorder != nullptr && m_recorder->recorderState() != QMediaRecorder::StoppedState) {
                m_recorder->stop();
            }
            return;
        }

        QTimer::singleShot(END_OF_STREAM_RETRY_MS, this,
                           [this, attempt]
                           {
                               trySendEndOfStream(attempt + 1);
                           });
    }

    void VideoFrameWriter::scheduleFinalizeTimeout()
    {
        QTimer::singleShot(AUTO_STOP_GRACE_MS, this,
                           [this]
                           {
                               if (m_reported) {
                                   return;
                               }
                               if (m_recorder != nullptr
                                   && m_recorder->recorderState() == QMediaRecorder::StoppedState) {
                                   m_reported = true;
                                   emit finished(!m_failed, m_failed ? m_errorMessage : QString{});
                                   return;
                               }
                               LOG_WARN_LITERAL("录制器未在宽限时间内自动停止，改为强制停止");
                               m_recorder->stop();
                           });

        // 二次兜底：即使 stop() 之后状态也没有变化，也不能让调用方无限等待。
        QTimer::singleShot(FINALIZE_TIMEOUT_MS, this,
                           [this]
                           {
                               if (!m_reported) {
                                   m_reported = true;
                                   emit finished(!m_failed, m_failed ? m_errorMessage : QString{});
                               }
                           });
    }

    void VideoFrameWriter::abort(const QString& reason)
    {
        if (m_reported) {
            return;
        }

        m_failed = true;
        m_errorMessage = reason;
        m_reported = true;

        if (m_recorder != nullptr && m_recorder->recorderState() != QMediaRecorder::StoppedState) {
            m_recorder->stop();
        }

        emit finished(false, reason);
    }

    void VideoFrameWriter::reportFailure(const QString& message)
    {
        if (m_reported) {
            return;
        }

        m_failed = true;
        m_errorMessage = message;
        m_reported = true;

        if (m_recorder != nullptr && m_recorder->recorderState() != QMediaRecorder::StoppedState) {
            m_recorder->stop();
        }

        emit finished(false, message);
    }
}
