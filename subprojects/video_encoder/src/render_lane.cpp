/**
 * @file        render_lane.cpp
 * @brief       渲染通道实现。
 * @details     通道内的所有 Qt 对象（QML 引擎、离屏窗口、渲染控制器、场景控制器）
 *              都在通道线程中创建与销毁，因此线程之间没有共享状态；唯一跨线程的
 *              数据是渲染完成的 QVideoFrame，它通过队列信号传递（引用计数保证缓冲存活）。
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

#include "render_lane.h"

#include <QElapsedTimer>

#include <algorithm>
#include <cmath>
#include <memory>

#include "async_logger.h"
#include "scene_manager.h"

namespace egret::video
{
    namespace
    {
        /** 微秒与秒的换算常量（QVideoFrame 的显示时间戳单位是微秒）。 */
        constexpr double MICROSECONDS_PER_SECOND{1.0e6};
    }

    RenderLane::RenderLane(QObject* parent) :
        QObject(parent)
    {
    }

    RenderLane::~RenderLane() = default;

    double RenderLane::applyMs() const
    {
        return m_applyMs;
    }

    const OffscreenSceneRenderer::Timings& RenderLane::renderTimings() const
    {
        static const OffscreenSceneRenderer::Timings emptyTimings{};
        return m_renderer != nullptr ? m_renderer->timings() : emptyTimings;
    }

    bool RenderLane::prepare(const Settings& settings, QString* errorMessage)
    {
        m_settings = settings;

        // 每个通道独立打开 epsx：SceneManagerViewModel 不是线程安全的，
        // 让每个通道拥有自己的读取器与物理世界，避免任何跨线程访问。
        m_scene = std::make_unique<egret::SceneManagerViewModel>(nullptr, false);
        if (!m_scene->openSimulationFile(settings.epsxPath)) {
            const QString reason{m_scene->simulationError()};
            m_scene.reset();
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("打开仿真文件失败：%1")
                                    .arg(reason.isEmpty() ? settings.epsxPath : reason);
            }
            return false;
        }

        OffscreenSceneRenderer::Settings renderSettings{};
        renderSettings.resolution = settings.resolution;
        renderSettings.camera = settings.camera;
        renderSettings.showGrid = settings.showGrid;

        m_renderer = std::make_unique<OffscreenSceneRenderer>(m_scene.get());
        if (!m_renderer->initialize(renderSettings, errorMessage)) {
            m_renderer.reset();
            m_scene.reset();
            return false;
        }
        return true;
    }

    QVideoFrame RenderLane::renderFrame(const quint64 outputIndex, const int sourceFrame, QString* errorMessage)
    {
        if (m_scene == nullptr || m_renderer == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("渲染通道尚未初始化");
            }
            return {};
        }

        QElapsedTimer stageTimer{};
        stageTimer.start();
        m_scene->setPlaybackFrame(sourceFrame);
        m_applyMs += static_cast<double>(stageTimer.nsecsElapsed()) / 1.0e6;

        QVideoFrame frame{m_renderer->renderFrame(errorMessage)};
        if (!frame.isValid()) {
            return {};
        }

        frame.setStreamFrameRate(m_settings.frameRate);
        const double index{static_cast<double>(outputIndex)};
        frame.setStartTime(static_cast<qint64>(index * MICROSECONDS_PER_SECOND / m_settings.frameRate));
        frame.setEndTime(static_cast<qint64>((index + 1.0) * MICROSECONDS_PER_SECOND / m_settings.frameRate));
        return frame;
    }
}
