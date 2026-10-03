/**
 * @file        epsx_video_encoder.cpp
 * @brief       epsx → 视频导出任务的实现。
 * @details     导出采用「编码器拉取」的驱动方式：VideoFrameWriter 就绪时发出 frameRequested()，
 *              本类在主线程完成「源帧状态套用 → 离屏渲染 → GPU 回读 → 组装视频帧」，
 *              再把帧投递到编码线程。这样既不会在主线程堆积帧缓冲，也不会因为编码器忙而丢帧。
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

#include "epsx_video_encoder.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoFrameFormat>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include "async_logger.h"
#include "offscreen_scene_renderer.h"
#include "scene_manager.h"
#include "video_frame_writer.h"

namespace egret::video
{
    namespace
    {
        /** 每帧的字节数换算系数（RGBA8888：4 字节/像素）。 */
        constexpr int BYTES_PER_PIXEL{4};

        /** 纳秒与秒的换算常量。 */
        constexpr double NANOSECONDS_PER_SECOND{1.0e9};

        /**
         * @brief 微秒与秒的换算常量。
         * @remark QVideoFrame 的显示时间戳单位是**微秒**（Qt 文档明确说明），
         *         不是纳秒；用错单位会让输出视频的时长放大一千倍。
         */
        constexpr double MICROSECONDS_PER_SECOND{1.0e6};
    }

    EpsxVideoEncoder::EpsxVideoEncoder(QObject* parent) :
        QObject(parent)
    {
    }

    EpsxVideoEncoder::~EpsxVideoEncoder()
    {
        stopWorkerThread();
    }

    bool EpsxVideoEncoder::isRunning() const
    {
        return m_renderer != nullptr && !m_finished;
    }

    const EpsxVideoEncoder::Report& EpsxVideoEncoder::report() const
    {
        return m_report;
    }

    bool EpsxVideoEncoder::start(const Options& options, QString* errorMessage)
    {
        const auto fail = [errorMessage](const QString& message)
        {
            if (errorMessage != nullptr) {
                *errorMessage = message;
            }
            return false;
        };

        if (m_renderer != nullptr) {
            return fail(QStringLiteral("导出任务已经启动，不能重复调用 start()"));
        }

        m_options = options;
        // 统一转成绝对路径：QMediaRecorder 会把相对路径解析到系统「视频」目录，
        // 而不是进程当前工作目录，容易写出用户意想不到的位置。
        m_options.outputPath = QFileInfo{options.outputPath}.absoluteFilePath();
        m_finished = false;
        m_finishing = false;
        m_cancelled = false;
        m_renderedFrames = 0;
        m_report = Report{};

        // ---- 输出目录准备 ----
        const QString targetPath{m_options.previewPngPath.isEmpty() ? m_options.outputPath
                                                                    : m_options.previewPngPath};
        const QFileInfo outputInfo{targetPath};
        const QString outputDirectory{outputInfo.absolutePath()};
        if (!QFileInfo::exists(outputDirectory) && !QDir{}.mkpath(outputDirectory)) {
            return fail(QStringLiteral("无法创建输出目录：%1").arg(outputDirectory));
        }

        // ---- 编码能力探测：宁可现在失败，也不要录到一半才失败 ----
        const QMediaFormat::FileFormat fileFormat{toFileFormat(options.container)};
        const QMediaFormat::VideoCodec requestedCodec{toVideoCodec(options.codec)};
        const QList<QMediaFormat::VideoCodec> supportedCodecs{VideoFrameWriter::supportedVideoCodecs(fileFormat)};
        if (!supportedCodecs.contains(requestedCodec)) {
            QStringList availableNames{};
            for (const QMediaFormat::VideoCodec codec : supportedCodecs) {
                availableNames.append(QMediaFormat::videoCodecName(codec));
            }
            return fail(QStringLiteral("当前媒体后端不支持 %1 + %2；可用组合：%3")
                            .arg(containerName(m_options.container),
                                 codecName(m_options.codec),
                                 availableNames.isEmpty() ? QStringLiteral("（无）")
                                                          : availableNames.join(QStringLiteral(", "))));
        }

        // ---- 打开 epsx 并重建场景（独立实例，不影响正在播放的界面）----
        m_scene = std::make_unique<egret::SceneManagerViewModel>(nullptr, false);
        if (!m_scene->openSimulationFile(options.inputPath)) {
            const QString reason{m_scene->simulationError()};
            m_scene.reset();
            return fail(QStringLiteral("打开仿真文件失败：%1")
                            .arg(reason.isEmpty() ? options.inputPath : reason));
        }

        const int sourceFrameCount{m_scene->simulationFrameCount()};
        m_sampleIntervalSeconds = m_scene->simulationSampleIntervalMs() / 1000.0;
        if (sourceFrameCount <= 0 || m_sampleIntervalSeconds <= 0.0) {
            m_scene.reset();
            return fail(QStringLiteral("仿真文件没有可用帧或采样间隔非法"));
        }

        // ---- 时间轴：视频时长与 epsx 播放时长一致，帧率只决定采样密度 ----
        const double sourceDurationSeconds{m_scene->simulationDurationSeconds()};
        m_frameRate = options.frameRate > 0.0 ? options.frameRate : 1.0 / m_sampleIntervalSeconds;
        m_totalFrames = static_cast<quint64>(
            std::max(1LL, std::llround(sourceDurationSeconds * m_frameRate)));

        m_report.outputPath = m_options.outputPath;
        m_report.sourceFrames = static_cast<quint64>(sourceFrameCount);
        m_report.durationSeconds = sourceDurationSeconds;
        m_report.frameRate = m_frameRate;
        m_report.resolution = QSize{options.width, options.height};

        // ---- 离屏渲染器 ----
        OffscreenSceneRenderer::Settings renderSettings{};
        renderSettings.resolution = m_report.resolution;
        renderSettings.camera = resolveCameraPose(options.camera);
        renderSettings.showGrid = options.showGrid;

        m_renderer = std::make_unique<OffscreenSceneRenderer>(m_scene.get());
        if (!m_renderer->initialize(renderSettings, errorMessage)) {
            m_renderer.reset();
            m_scene.reset();
            return false;
        }

        // ---- 预览模式：只输出首帧 PNG，不进入视频编码流程 ----
        if (!m_options.previewPngPath.isEmpty()) {
            m_clock.start();

            QString previewError{};
            const QImage previewImage{m_renderer->renderFrame(&previewError)};
            if (previewImage.isNull()) {
                return fail(QStringLiteral("渲染首帧失败：%1").arg(previewError));
            }
            if (!previewImage.save(m_options.previewPngPath)) {
                return fail(QStringLiteral("保存预览图片失败：%1").arg(m_options.previewPngPath));
            }

            m_report.previewOnly = true;
            m_report.outputPath = m_options.previewPngPath;
            m_report.outputFrames = 1;
            m_report.wallClockSeconds = static_cast<double>(m_clock.nsecsElapsed()) / NANOSECONDS_PER_SECOND;

            LOG_INFO("已保存首帧预览：" + m_options.previewPngPath.toStdString());
            QTimer::singleShot(0, this,
                               [this]
                               {
                                   if (m_finished) {
                                       return;
                                   }
                                   m_finished = true;
                                   emit finished(true, QString{});
                               });
            return true;
        }

        // ---- 编码线程 ----
        m_writerThread = std::make_unique<QThread>();
        m_writerThread->setObjectName(QStringLiteral("egret-video-encoder"));

        // VideoFrameWriter 内部持有 QMediaRecorder 等带线程亲和性的 Qt 对象，
        // 必须在工作线程内创建与销毁：这里把所有权交给线程结束前的 deleteLater()。
        std::unique_ptr<VideoFrameWriter> writerOwner{std::make_unique<VideoFrameWriter>()};
        m_writer = writerOwner.release();
        m_writer->moveToThread(m_writerThread.get());

        VideoFrameWriter::Settings writerSettings{};
        writerSettings.outputPath = m_options.outputPath;
        writerSettings.resolution = m_report.resolution;
        writerSettings.frameRate = m_frameRate;
        writerSettings.fileFormat = fileFormat;
        writerSettings.videoCodec = requestedCodec;
        writerSettings.quality = toQuality(options.quality);

        connect(m_writer, &VideoFrameWriter::frameRequested, this, &EpsxVideoEncoder::handleFrameRequest);
        connect(m_writer, &VideoFrameWriter::finished, this, &EpsxVideoEncoder::handleWriterFinished);
        connect(m_writer, &VideoFrameWriter::finished, m_writerThread.get(), &QThread::quit, Qt::DirectConnection);
        connect(m_writer, &VideoFrameWriter::finished, m_writer, &QObject::deleteLater);
        connect(m_writerThread.get(), &QThread::started, m_writer,
                [writer = m_writer, writerSettings]
                {
                    writer->start(writerSettings);
                });

        // 兜底：编码线程若在没有发出 finished 的情况下结束，也要给调用方一个明确结果。
        connect(m_writerThread.get(), &QThread::finished, this,
                [this]
                {
                    if (m_finished) {
                        return;
                    }
                    m_writer = nullptr;
                    m_finished = true;
                    LOG_ERROR_LITERAL("编码线程意外结束，导出未正常收尾");
                    emit finished(false, QStringLiteral("编码线程意外结束，导出未正常收尾"));
                });

        m_clock.start();
        m_writerThread->start();
        return true;
    }

    void EpsxVideoEncoder::cancel()
    {
        if (m_finished || m_finishing) {
            return;
        }
        m_cancelled = true;
        LOG_INFO_LITERAL("收到取消请求：已写入的帧将保留，文件会正常收尾");
    }

    void EpsxVideoEncoder::handleFrameRequest()
    {
        if (m_writer == nullptr || m_finished || m_finishing || m_renderer == nullptr || m_scene == nullptr) {
            return;
        }

        // 全部帧写完（或被取消）后，再收到请求就发送流结束标志。
        if (m_cancelled || m_renderedFrames >= m_totalFrames) {
            m_finishing = true;
            QMetaObject::invokeMethod(m_writer, [writer = m_writer]{ writer->finish(); }, Qt::QueuedConnection);
            return;
        }

        // 时间映射：与播放器 SceneManagerViewModel::advancePlayback() 使用同一条 floor 规则，
        // 因此「mp4 的画面序列」与「同参数下播放器看到的画面序列」一致。
        const double timeSeconds{static_cast<double>(m_renderedFrames) / m_frameRate};
        const int sourceFrame{
            std::clamp(static_cast<int>(std::floor(timeSeconds / m_sampleIntervalSeconds)),
                       0,
                       std::max(0, m_scene->simulationFrameCount() - 1))};
        m_scene->setPlaybackFrame(sourceFrame);

        QString renderError{};
        const QImage image{m_renderer->renderFrame(&renderError)};
        if (image.isNull()) {
            failWith(renderError.isEmpty() ? QStringLiteral("离屏渲染失败") : renderError);
            return;
        }

        if (!dispatchFrame(image)) {
            return;
        }

        ++m_renderedFrames;
        m_report.outputFrames = m_renderedFrames;
        emit progress(m_renderedFrames, m_totalFrames);
    }

    bool EpsxVideoEncoder::dispatchFrame(const QImage& image)
    {
        QVideoFrame frame{m_renderer->frameFormat()};
        frame.setStreamFrameRate(m_frameRate);
        if (!frame.map(QVideoFrame::WriteOnly)) {
            failWith(QStringLiteral("无法映射视频帧缓冲（内存不足？）"));
            return false;
        }

        const int rowBytes{image.width() * BYTES_PER_PIXEL};
        const int destinationStride{frame.bytesPerLine(0)};
        uchar* const destination{frame.bits(0)};
        for (int row = 0; row < image.height(); ++row) {
            std::memcpy(destination + static_cast<qsizetype>(row) * destinationStride,
                        image.constScanLine(row),
                        static_cast<std::size_t>(rowBytes));
        }
        frame.unmap();

        // 显式设置时间戳：即使编码器内部按帧率推算，也不会因为逐帧耗时抖动而累积偏移。
        const double frameIndex{static_cast<double>(m_renderedFrames)};
        frame.setStartTime(static_cast<qint64>(frameIndex * MICROSECONDS_PER_SECOND / m_frameRate));
        frame.setEndTime(static_cast<qint64>((frameIndex + 1.0) * MICROSECONDS_PER_SECOND / m_frameRate));

        QMetaObject::invokeMethod(m_writer,
                                  [writer = m_writer, frame]
                                  {
                                      writer->writeFrame(frame);
                                  },
                                  Qt::QueuedConnection);
        return true;
    }

    void EpsxVideoEncoder::handleWriterFinished(const bool success, const QString& message)
    {
        if (m_finished) {
            return;
        }

        stopWorkerThread();

        m_report.outputFrames = m_renderedFrames;
        m_report.canceled = m_cancelled;
        m_report.wallClockSeconds = static_cast<double>(m_clock.nsecsElapsed()) / NANOSECONDS_PER_SECOND;

        const QFileInfo outputInfo{m_options.outputPath};
        m_report.fileBytes = outputInfo.exists() ? static_cast<quint64>(outputInfo.size()) : 0ULL;

        m_finished = true;

        QString finalMessage{message};
        if (success && m_report.canceled) {
            finalMessage = QStringLiteral("导出已取消（已写入的帧已正常收尾）");
        }
        emit finished(success, finalMessage);
    }

    void EpsxVideoEncoder::failWith(const QString& message)
    {
        if (m_finishing || m_finished) {
            return;
        }

        m_finishing = true;
        LOG_ERROR(message.toStdString());

        if (m_writer == nullptr) {
            handleWriterFinished(false, message);
            return;
        }

        // 让编码器自行中止并收尾，随后由 handleWriterFinished() 统一汇总结果；
        // 若编码器没有回应，则由看门狗保证调用方仍能收到结果。
        QMetaObject::invokeMethod(m_writer,
                                  [writer = m_writer, message]
                                  {
                                      writer->abort(message);
                                  },
                                  Qt::QueuedConnection);
        QTimer::singleShot(10000, this,
                           [this]
                           {
                               if (!m_finished) {
                                   LOG_ERROR_LITERAL("等待编码器收尾超时，强制结束导出");
                                   handleWriterFinished(false, QStringLiteral("等待编码器收尾超时"));
                               }
                           });
    }

    void EpsxVideoEncoder::stopWorkerThread()
    {
        if (m_writerThread != nullptr) {
            if (m_writerThread->isRunning()) {
                m_writerThread->quit();
                m_writerThread->wait();
            }
            m_writerThread.reset();
        }

        // 编码器对象由工作线程结束前的 deleteLater() 释放，这里只清空指针。
        m_writer = nullptr;
    }
}
