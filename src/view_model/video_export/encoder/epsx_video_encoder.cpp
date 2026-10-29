/**
 * @file        epsx_video_encoder.cpp
 * @brief       epsx → 视频导出任务的实现（多通道并行渲染 + 单编码器按序写盘）。
 * @details     导出流水线分为三级：
 *
 *              1. 渲染通道（N 个线程）：每个通道独立打开 epsx、独立重建场景、独立离屏渲染，
 *                 通道之间不共享任何 Qt 对象，因此可以无锁并行渲染不同帧；
 *              2. 时序归并（主线程）：把各通道渲染完成的帧按输出帧号排队，严格保证视频帧顺序；
 *              3. 编码线程（1 个线程）：QMediaRecorder 以拉模式消费帧并写入文件。
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
#include <QEventLoop>
#include <QFileInfo>
#include <QImage>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QVideoFrame>

#include <algorithm>
#include <cmath>
#include <memory>

#include "async_logger.h"
#include "ffmpeg_video_writer.h"
#include "offscreen_scene_renderer.h"
#include "render_lane.h"
#include "scene_manager.h"
#include "video_frame_writer.h"

namespace egret::video
{
    namespace
    {
        /** 纳秒与秒的换算常量。 */
        constexpr double NANOSECONDS_PER_SECOND{1.0e9};

        /**
         * @brief  默认渲染通道数。
         * @return 固定为 1。
         * @remark 实测（1080p，RTX 级显卡 + Media Foundation 硬件编码）：单个通道已经接近
         *         编码器上限（约 4.5 ms/帧 ≈ 220 帧/秒），增加通道只会引入 GPU 竞争与
         *         额外的显存/初始化开销，因此默认单通道；渲染成为瓶颈的场景可以用
         *         `--lanes N` 手动开启并行。
         */
        int defaultLaneCount()
        {
            return 1;
        }
    }

    EpsxVideoEncoder::EpsxVideoEncoder(QObject* parent) :
        QObject(parent)
    {
    }

    EpsxVideoEncoder::~EpsxVideoEncoder()
    {
        stopLanes();
        stopWorkerThread();
    }

    bool EpsxVideoEncoder::isRunning() const
    {
        return (!m_lanes.empty() || m_previewLane != nullptr) && !m_finished;
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

        if (!m_lanes.empty() || m_previewLane != nullptr) {
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
        m_readyFrames.clear();
        m_nextToSchedule = 0;
        m_nextToSend = 0;
        m_busyLaneCount = 0;
        m_writerWantsFrame = false;

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
        if (m_options.backend == EncoderBackend::Ffmpeg && options.codec != VideoCodecChoice::H264) {
            return fail(QStringLiteral("FFmpeg 后端目前只支持 H.264；MPEG-4 请改用 --backend qt"));
        }
        if (m_options.backend == EncoderBackend::QtMultimedia) {
            const QList<QMediaFormat::VideoCodec> supportedCodecs{
                VideoFrameWriter::supportedVideoCodecs(fileFormat)};
            if (!supportedCodecs.contains(requestedCodec)) {
                QStringList availableNames{};
                for (const QMediaFormat::VideoCodec codec : supportedCodecs) {
                    availableNames.append(QMediaFormat::videoCodecName(codec));
                }
                return fail(QStringLiteral("当前 Qt 媒体后端不支持 %1 + %2；可用组合：%3")
                                .arg(containerName(m_options.container),
                                     codecName(m_options.codec),
                                     availableNames.isEmpty() ? QStringLiteral("（无）")
                                                              : availableNames.join(QStringLiteral(", "))));
            }
        }
        else {
            const QList<QString> encoders{FfmpegVideoWriter::candidateEncoders()};
            if (encoders.isEmpty()) {
                return fail(QStringLiteral("没有可用的 FFmpeg 视频编码器（检查 dependency/bin/ffmpeg 是否部署）"));
            }
        }

        // ---- 读取 epsx 元数据（临时场景，读完即释放，渲染由各通道自行负责）----
        m_metadataScene = std::make_unique<egret::SceneManagerViewModel>(nullptr, false);
        if (!m_metadataScene->openSimulationFile(options.inputPath)) {
            const QString reason{m_metadataScene->simulationError()};
            m_metadataScene.reset();
            return fail(QStringLiteral("打开仿真文件失败：%1")
                            .arg(reason.isEmpty() ? options.inputPath : reason));
        }

        const int sourceFrameCount{m_metadataScene->simulationFrameCount()};
        m_sampleIntervalSeconds = m_metadataScene->simulationSampleIntervalMs() / 1000.0;
        if (sourceFrameCount <= 0 || m_sampleIntervalSeconds <= 0.0) {
            m_metadataScene.reset();
            return fail(QStringLiteral("仿真文件没有可用帧或采样间隔非法"));
        }

        // ---- 时间轴：视频时长与 epsx 播放时长一致，帧率只决定采样密度 ----
        const double sourceDurationSeconds{m_metadataScene->simulationDurationSeconds()};
        m_frameRate = options.frameRate > 0.0 ? options.frameRate : 1.0 / m_sampleIntervalSeconds;
        m_totalFrames = static_cast<quint64>(
            std::max(1LL, std::llround(sourceDurationSeconds * m_frameRate)));

        m_report.outputPath = m_options.outputPath;
        m_report.sourceFrames = static_cast<quint64>(sourceFrameCount);
        m_report.durationSeconds = sourceDurationSeconds;
        m_report.frameRate = m_frameRate;
        m_report.resolution = QSize{options.width, options.height};

        const CameraPose camera{resolveCameraPose(options.camera)};

        // ---- 预览模式：主线程单通道只渲染首帧，不进入编码流程 ----
        if (!m_options.previewPngPath.isEmpty()) {
            m_previewLane = std::make_unique<RenderLane>();
            RenderLane::Settings previewSettings{};
            previewSettings.epsxPath = options.inputPath;
            previewSettings.resolution = m_report.resolution;
            previewSettings.camera = camera;
            previewSettings.showGrid = options.showGrid;
            previewSettings.frameRate = m_frameRate;
            if (!m_previewLane->prepare(previewSettings, errorMessage)) {
                m_previewLane.reset();
                m_metadataScene.reset();
                return false;
            }

            m_clock.start();
            QString previewError{};
            const QVideoFrame previewFrame{m_previewLane->renderFrame(0, 0, &previewError)};
            if (!previewFrame.isValid()) {
                return fail(QStringLiteral("渲染首帧失败：%1").arg(previewError));
            }
            const QImage previewImage{previewFrame.toImage()};
            if (previewImage.isNull() || !previewImage.save(m_options.previewPngPath)) {
                return fail(QStringLiteral("保存预览图片失败：%1").arg(m_options.previewPngPath));
            }

            m_previewLane.reset();
            m_metadataScene.reset();
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

        // ---- 并行渲染通道 ----
        if (!createLanes(camera, errorMessage)) {
            m_metadataScene.reset();
            return false;
        }
        m_metadataScene.reset();

        // ---- 编码线程 ----
        m_writerThread = std::make_unique<QThread>();
        m_writerThread->setObjectName(QStringLiteral("egret-video-encoder"));

        // 编码器对象带线程亲和性，必须在工作线程内创建与销毁：所有权交给线程结束前的 deleteLater()。
        if (m_options.backend == EncoderBackend::Ffmpeg) {
            std::unique_ptr<FfmpegVideoWriter> writerOwner{std::make_unique<FfmpegVideoWriter>()};
            m_ffmpegWriter = writerOwner.release();
            m_ffmpegWriter->moveToThread(m_writerThread.get());

            FfmpegVideoWriter::Settings writerSettings{};
            writerSettings.outputPath = m_options.outputPath;
            writerSettings.resolution = m_report.resolution;
            writerSettings.frameRate = m_frameRate;
            writerSettings.encoderName = (m_options.encoderName.compare(QStringLiteral("auto"),
                                                                        Qt::CaseInsensitive) == 0)
                                             ? QString{}
                                             : m_options.encoderName;
            writerSettings.preset = m_options.encoderPreset;
            writerSettings.bitRateScale = qualityToBitRateScale(options.quality);

            connect(m_ffmpegWriter, &FfmpegVideoWriter::frameRequested,
                    this, &EpsxVideoEncoder::handleFrameRequest);
            connect(m_ffmpegWriter, &FfmpegVideoWriter::finished,
                    this, &EpsxVideoEncoder::handleWriterFinished);
            connect(m_ffmpegWriter, &FfmpegVideoWriter::finished,
                    m_writerThread.get(), &QThread::quit, Qt::DirectConnection);
            connect(m_ffmpegWriter, &FfmpegVideoWriter::finished,
                    m_ffmpegWriter, &QObject::deleteLater);
            connect(m_writerThread.get(), &QThread::started, m_ffmpegWriter,
                    [writer = m_ffmpegWriter, writerSettings]
                    {
                        QString error{};
                        if (!writer->start(writerSettings, &error)) {
                            // 启动失败也要走统一的结束通道，让调用方拿到明确原因。
                            emit writer->finished(false,
                                                  error.isEmpty() ? QStringLiteral("编码器启动失败") : error);
                        }
                    });
        }
        else {
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
            connect(m_writer, &VideoFrameWriter::finished, m_writerThread.get(), &QThread::quit,
                    Qt::DirectConnection);
            connect(m_writer, &VideoFrameWriter::finished, m_writer, &QObject::deleteLater);
            connect(m_writerThread.get(), &QThread::started, m_writer,
                    [writer = m_writer, writerSettings]
                    {
                        writer->start(writerSettings);
                    });
        }

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

        LOG_INFO("并行渲染通道数量：" + std::to_string(m_laneCount));
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
        if (!hasWriter() || m_finished || m_finishing) {
            return;
        }

        // 上一帧送进编码器 → 编码器再次请求，这段时间反映编码器（含像素转换）的真实速度。
        if (m_encoderTurnaroundTimer.isValid()) {
            m_encoderTurnaroundMs += static_cast<double>(m_encoderTurnaroundTimer.nsecsElapsed()) / 1.0e6;
            m_encoderTurnaroundTimer.invalidate();
        }

        m_writerWantsFrame = true;
        pumpPipeline();
    }

    void EpsxVideoEncoder::pumpPipeline()
    {
        if (m_finished || m_finishing) {
            return;
        }
        scheduleFrames();
        sendNextReadyFrame();
        checkCompletion();
    }

    void EpsxVideoEncoder::scheduleFrames()
    {
        if (m_cancelled) {
            return;
        }

        for (int lane = 0; lane < m_laneCount; ++lane) {
            const auto position{static_cast<std::size_t>(lane)};
            if (m_laneBusy.at(position)) {
                continue;
            }
            if (m_nextToSchedule >= m_totalFrames) {
                return;
            }

            const quint64 outputIndex{m_nextToSchedule++};
            m_laneBusy.at(position) = true;
            m_laneFrameIndex.at(position) = outputIndex;
            ++m_busyLaneCount;

            RenderLane* const lanePointer{m_lanes.at(position)};
            const int sourceFrame{sourceFrameForOutputIndex(outputIndex)};

            // 主线程通道：直接渲染，再用队列调用回到归并逻辑，避免递归加深调用栈。
            if (position >= m_laneThreads.size() || m_laneThreads.at(position) == nullptr) {
                QString error{};
                const QVideoFrame frame{lanePointer->renderFrame(outputIndex, sourceFrame, &error)};
                QMetaObject::invokeMethod(
                    this,
                    [this, outputIndex, frame, error]
                    {
                        if (frame.isValid()) {
                            handleLaneFrame(outputIndex, frame);
                        }
                        else {
                            handleLaneFailure(outputIndex, error);
                        }
                    },
                    Qt::QueuedConnection);
                continue;
            }

            QMetaObject::invokeMethod(lanePointer,
                                      [lanePointer, outputIndex, sourceFrame]
                                      {
                                          QString error{};
                                          const QVideoFrame frame{
                                              lanePointer->renderFrame(outputIndex, sourceFrame, &error)};
                                          if (frame.isValid()) {
                                              emit lanePointer->frameRendered(outputIndex, frame);
                                          }
                                          else {
                                              emit lanePointer->frameFailed(outputIndex, error);
                                          }
                                      },
                                      Qt::QueuedConnection);
        }
    }

    void EpsxVideoEncoder::sendNextReadyFrame()
    {
        if (!m_writerWantsFrame || !hasWriter()) {
            return;
        }

        const auto ready{m_readyFrames.find(m_nextToSend)};
        if (ready == m_readyFrames.end()) {
            return;
        }

        m_writerWantsFrame = false;
        QVideoFrame frame{ready.value()};
        m_readyFrames.erase(ready);
        ++m_nextToSend;
        ++m_renderedFrames;
        m_report.outputFrames = m_renderedFrames;

        pushFrameToWriter(frame);
        m_encoderTurnaroundTimer.restart();
        emit progress(m_renderedFrames, m_totalFrames);
    }

    void EpsxVideoEncoder::handleLaneFrame(const quint64 outputIndex, const QVideoFrame& frame)
    {
        if (m_finished) {
            return;
        }

        for (int lane = 0; lane < m_laneCount; ++lane) {
            const auto position{static_cast<std::size_t>(lane)};
            if (m_laneBusy.at(position) && m_laneFrameIndex.at(position) == outputIndex) {
                m_laneBusy.at(position) = false;
                --m_busyLaneCount;
                break;
            }
        }

        m_readyFrames.insert(outputIndex, frame);
        pumpPipeline();
    }

    void EpsxVideoEncoder::handleLaneFailure(const quint64 outputIndex, const QString& message)
    {
        if (m_finished) {
            return;
        }
        failWith(QStringLiteral("第 %1 帧渲染失败：%2")
                     .arg(outputIndex)
                     .arg(message.isEmpty() ? QStringLiteral("未知原因") : message));
    }

    void EpsxVideoEncoder::checkCompletion()
    {
        if (m_finished || m_finishing || !hasWriter()) {
            return;
        }

        const bool allSent{m_nextToSend >= m_totalFrames};
        const bool pipelineDrained{m_busyLaneCount == 0
                                   && !m_readyFrames.contains(m_nextToSend)
                                   && m_nextToSend >= m_nextToSchedule};
        if (!allSent && !(m_cancelled && pipelineDrained)) {
            return;
        }

        m_finishing = true;
        requestWriterFinish();
    }

    int EpsxVideoEncoder::sourceFrameForOutputIndex(const quint64 outputIndex) const
    {
        // 与播放器 SceneManagerViewModel::advancePlayback() 使用同一条 floor 规则，
        // 因此「mp4 的画面序列」与「同参数下播放器看到的画面序列」一致。
        const double timeSeconds{static_cast<double>(outputIndex) / m_frameRate};
        const int sourceFrameCount{
            m_report.sourceFrames > 0 ? static_cast<int>(m_report.sourceFrames) : 1};
        return std::clamp(static_cast<int>(std::floor(timeSeconds / m_sampleIntervalSeconds)),
                          0,
                          sourceFrameCount - 1);
    }

    QString EpsxVideoEncoder::timingSummary() const
    {
        if (m_renderedFrames == 0 || m_laneCountUsed == 0) {
            return QStringLiteral("（没有可统计的帧）");
        }

        const double frames{static_cast<double>(m_renderedFrames)};
        const double totalMs{m_report.wallClockSeconds * 1000.0 / frames};
        const double laneTotalMs{(m_laneApplyMs + m_laneSyncMs + m_laneRenderMs
                                  + m_laneReadbackMs + m_laneImageMs)
                                 / frames};

        return QStringLiteral(
                   "性能统计（共 %1 帧，%2 个并行通道，单位：毫秒/帧）\n"
                   "  套用源帧状态（通道合计）：%3\n"
                   "  同步 + 渲染 + 回读（通道合计）：%4　（同步 %5｜渲染 %6｜回读 %7｜图像 %8）\n"
                   "  编码器周转（投递 → 下次请求）：%9\n"
                   "  其余（调度 + 等待）：%10\n"
                   "  合计：%11　≈ %12 帧/秒\n")
            .arg(m_renderedFrames)
            .arg(m_laneCountUsed)
            .arg(QString::number(m_laneApplyMs / frames, 'f', 2))
            .arg(QString::number((m_laneSyncMs + m_laneRenderMs + m_laneReadbackMs + m_laneImageMs) / frames,
                                 'f', 2))
            .arg(QString::number(m_laneSyncMs / frames, 'f', 2))
            .arg(QString::number(m_laneRenderMs / frames, 'f', 2))
            .arg(QString::number(m_laneReadbackMs / frames, 'f', 2))
            .arg(QString::number(m_laneImageMs / frames, 'f', 2))
            .arg(QString::number(m_encoderTurnaroundMs / frames, 'f', 2))
            .arg(QString::number(std::max(0.0, totalMs - laneTotalMs - m_encoderTurnaroundMs / frames),
                                 'f', 2))
            .arg(QString::number(totalMs, 'f', 2))
            .arg(QString::number(frames / std::max(0.001, m_report.wallClockSeconds), 'f', 1));
    }

    bool EpsxVideoEncoder::createLanes(const CameraPose& camera, QString* errorMessage)
    {
        int requestedLanes{m_options.laneCount > 0 ? m_options.laneCount : defaultLaneCount()};
        requestedLanes = static_cast<int>(
            std::min<quint64>(static_cast<quint64>(std::max(1, requestedLanes)),
                              std::max<quint64>(1, m_totalFrames)));

        RenderLane::Settings laneSettings{};
        laneSettings.epsxPath = m_options.inputPath;
        laneSettings.resolution = m_report.resolution;
        laneSettings.camera = camera;
        laneSettings.showGrid = m_options.showGrid;
        laneSettings.frameRate = m_frameRate;

        QString firstError{};

        for (int index = 0; index < requestedLanes; ++index) {
            // 单通道：直接运行在调用线程（GUI 线程）内 —— Qt 规定 QML 场景对象必须在 GUI 线程创建。
            if (requestedLanes == 1) {
                std::unique_ptr<RenderLane> lane{std::make_unique<RenderLane>()};
                QString laneError{};
                if (!lane->prepare(laneSettings, &laneError)) {
                    firstError = laneError;
                    LOG_WARN("渲染通道初始化失败：" + laneError.toStdString());
                    break;
                }
                m_lanes.push_back(lane.get());
                m_mainThreadLanes.push_back(std::move(lane));
                m_laneBusy.push_back(false);
                m_laneFrameIndex.push_back(0);
                break;
            }

            std::unique_ptr<RenderLane> laneOwner{std::make_unique<RenderLane>()};
            RenderLane* const lane{laneOwner.release()};   // 所有权交给通道线程退出前的 deleteLater()

            auto thread{std::make_unique<QThread>()};
            thread->setObjectName(QStringLiteral("egret-render-lane-%1").arg(index));
            lane->moveToThread(thread.get());

            // 初始化结果需要同步等待：用嵌套事件循环接收通道线程发出的 prepared 信号。
            bool prepared{false};
            QString prepareError{};
            QEventLoop waitLoop{};
            QObject::connect(lane, &RenderLane::prepared, &waitLoop,
                             [&prepared, &prepareError, &waitLoop](const bool success, const QString& message)
                             {
                                 prepared = success;
                                 prepareError = message;
                                 waitLoop.quit();
                             });

            QObject::connect(thread.get(), &QThread::started, lane,
                             [lane, laneSettings]
                             {
                                 QString laneError{};
                                 const bool success{lane->prepare(laneSettings, &laneError)};
                                 emit lane->prepared(success, laneError);
                             });

            connect(lane, &RenderLane::frameRendered, this, &EpsxVideoEncoder::handleLaneFrame);
            connect(lane, &RenderLane::frameFailed, this, &EpsxVideoEncoder::handleLaneFailure);

            thread->start();
            waitLoop.exec();

            if (!prepared) {
                firstError = prepareError;
                LOG_WARN("渲染通道初始化失败，降级为更少通道：" + prepareError.toStdString());

                // 回收这个失败的通道：让它在自己的线程里销毁，再退出线程。
                QMetaObject::invokeMethod(lane,
                                          [lane, threadPointer = thread.get()]
                                          {
                                              lane->deleteLater();
                                              threadPointer->quit();
                                          },
                                          Qt::QueuedConnection);
                thread->wait();
                break;
            }

            m_lanes.push_back(lane);
            m_laneThreads.push_back(std::move(thread));
            m_laneBusy.push_back(false);
            m_laneFrameIndex.push_back(0);
        }

        m_laneCount = static_cast<int>(m_lanes.size());
        if (m_laneCount == 0) {
            if (errorMessage != nullptr) {
                *errorMessage = firstError.isEmpty()
                                    ? QStringLiteral("无法创建任何渲染通道（离屏渲染初始化失败）")
                                    : QStringLiteral("无法创建渲染通道：%1").arg(firstError);
            }
            return false;
        }
        return true;
    }

    void EpsxVideoEncoder::stopLanes()
    {
        for (std::size_t index = 0; index < m_laneThreads.size(); ++index) {
            QThread* const thread{m_laneThreads.at(index).get()};
            RenderLane* const lane{index < m_lanes.size() ? m_lanes.at(index) : nullptr};
            if (thread == nullptr) {
                continue;
            }

            if (thread->isRunning()) {
                // 让通道对象在自己的线程里销毁，避免跨线程析构带线程亲和性的 Qt 对象。
                if (lane != nullptr) {
                    QMetaObject::invokeMethod(lane,
                                              [lane, thread]
                                              {
                                                  lane->deleteLater();
                                                  thread->quit();
                                              },
                                              Qt::QueuedConnection);
                }
                else {
                    thread->quit();
                }
                thread->wait();
            }
        }

        m_laneThreads.clear();
        m_lanes.clear();
        m_mainThreadLanes.clear();
        m_laneBusy.clear();
        m_laneFrameIndex.clear();
        m_busyLaneCount = 0;
        m_laneCount = 0;
        m_previewLane.reset();
    }

    void EpsxVideoEncoder::handleWriterFinished(const bool success, const QString& message)
    {
        if (m_finished) {
            return;
        }

        // 先把各通道的统计汇总下来，再销毁通道（销毁后无法再查询）。
        m_laneCountUsed = m_laneCount;
        for (const RenderLane* const lane : m_lanes) {
            if (lane == nullptr) {
                continue;
            }
            m_laneApplyMs += lane->applyMs();
            const OffscreenSceneRenderer::Timings& timings{lane->renderTimings()};
            m_laneSyncMs += timings.polishAndSyncMs;
            m_laneRenderMs += timings.renderMs;
            m_laneReadbackMs += timings.readbackMs;
            m_laneImageMs += timings.imageMs;
        }

        stopLanes();
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

        if (!hasWriter()) {
            handleWriterFinished(false, message);
            return;
        }

        // 让编码器自行中止并收尾，随后由 handleWriterFinished() 统一汇总结果；
        // 若编码器没有回应，则由看门狗保证调用方仍能收到结果。
        requestWriterAbort(message);
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
        m_ffmpegWriter = nullptr;
    }

    bool EpsxVideoEncoder::hasWriter() const
    {
        return m_writer != nullptr || m_ffmpegWriter != nullptr;
    }

    void EpsxVideoEncoder::pushFrameToWriter(const QVideoFrame& frame)
    {
        if (m_ffmpegWriter != nullptr) {
            QMetaObject::invokeMethod(m_ffmpegWriter,
                                      [writer = m_ffmpegWriter, frame]
                                      {
                                          writer->writeFrame(frame);
                                      },
                                      Qt::QueuedConnection);
            return;
        }
        if (m_writer != nullptr) {
            QMetaObject::invokeMethod(m_writer,
                                      [writer = m_writer, frame]
                                      {
                                          writer->writeFrame(frame);
                                      },
                                      Qt::QueuedConnection);
        }
    }

    void EpsxVideoEncoder::requestWriterFinish()
    {
        if (m_ffmpegWriter != nullptr) {
            QMetaObject::invokeMethod(m_ffmpegWriter,
                                      [writer = m_ffmpegWriter]{ writer->finish(); },
                                      Qt::QueuedConnection);
            return;
        }
        if (m_writer != nullptr) {
            QMetaObject::invokeMethod(m_writer,
                                      [writer = m_writer]{ writer->finish(); },
                                      Qt::QueuedConnection);
        }
    }

    void EpsxVideoEncoder::requestWriterAbort(const QString& message)
    {
        if (m_ffmpegWriter != nullptr) {
            QMetaObject::invokeMethod(m_ffmpegWriter,
                                      [writer = m_ffmpegWriter, message]{ writer->abort(message); },
                                      Qt::QueuedConnection);
            return;
        }
        if (m_writer != nullptr) {
            QMetaObject::invokeMethod(m_writer,
                                      [writer = m_writer, message]{ writer->abort(message); },
                                      Qt::QueuedConnection);
        }
    }
}
