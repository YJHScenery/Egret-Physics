/**
 * @file        render_lane.h
 * @brief       渲染通道：在独立线程内持有「独立场景 + 独立离屏渲染器」。
 * @details     epsx 的每一帧都是完整快照，帧与帧之间没有依赖，因此可以把不同帧分派到
 *              不同的渲染通道并行渲染。每个通道独占：
 *
 *              1. 自己的 SceneManagerViewModel（独立打开同一个 epsx 文件）；
 *              2. 自己的 QQuickRenderControl + 离屏窗口 + QML 引擎（含独立 QRhi 设备）；
 *              3. 自己的线程与事件循环。
 *
 *              通道之间不共享任何 Qt 对象，因此不需要锁；输出顺序由 EpsxVideoEncoder
 *              的时序归并器保证。
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

#ifndef EGRET_VIDEO_ENCODER_RENDER_LANE_H
#define EGRET_VIDEO_ENCODER_RENDER_LANE_H

#include <QObject>
#include <QSize>
#include <QString>
#include <QVideoFrame>

#include <memory>

#include "offscreen_scene_renderer.h"
#include "video_encoder_options.h"

namespace egret
{
    class SceneManagerViewModel;
}

namespace egret::video
{
    /**
     * @brief       一条独立的渲染流水线。
     * @invariant   prepare() 之后 renderFrame() 可以在本通道线程内任意次调用。
     */
    class RenderLane final : public QObject
    {
        Q_OBJECT

    public:
        /** 通道配置。 */
        struct Settings
        {
            /** 输入 epsx 文件路径。 */
            QString epsxPath{};

            /** 输出分辨率。 */
            QSize resolution{1920, 1080};

            /** 相机姿态（物理坐标系）。 */
            CameraPose camera{};

            /** 是否渲染网格与坐标轴。 */
            bool showGrid{true};

            /** 输出帧率（用于写入帧时间戳）。 */
            double frameRate{60.0};
        };

        /** 默认构造（对象必须在其所属线程中创建与使用）。 */
        explicit RenderLane(QObject* parent = nullptr);

        /** 析构。 */
        ~RenderLane() override;

        /**
         * @brief  加载 epsx、重建场景并初始化离屏渲染器。
         * @param  settings 通道配置。
         * @param  errorMessage 失败原因输出，可为 nullptr。
         * @return 成功返回 true。
         * @remark 必须在通道线程内调用。
         */
        [[nodiscard]] bool prepare(const Settings& settings, QString* errorMessage);

        /**
         * @brief  渲染一个输出帧。
         * @param  outputIndex 输出帧序号（决定时间戳）。
         * @param  sourceFrame 源 epsx 帧号。
         * @param  errorMessage 失败原因输出，可为 nullptr。
         * @return 渲染结果；失败时返回无效帧。
         * @remark 必须在通道线程内调用。
         */
        [[nodiscard]] QVideoFrame renderFrame(quint64 outputIndex, int sourceFrame, QString* errorMessage);

        /** 本通道「套用源帧状态」的累计耗时（毫秒）。 */
        [[nodiscard]] double applyMs() const;

        /** 本通道渲染阶段的累计耗时。 */
        [[nodiscard]] const OffscreenSceneRenderer::Timings& renderTimings() const;

    signals:
        /**
         * @brief 通道初始化完成（在通道线程内发出）。
         * @param success 是否成功。
         * @param message 失败原因；成功时为空串。
         */
        void prepared(bool success, const QString& message);

        /**
         * @brief 一帧渲染完成。
         * @param outputIndex 输出帧序号。
         * @param frame 渲染结果（直接引用回读缓冲）。
         */
        void frameRendered(quint64 outputIndex, const QVideoFrame& frame);

        /**
         * @brief 一帧渲染失败。
         * @param outputIndex 输出帧序号。
         * @param message 失败原因。
         */
        void frameFailed(quint64 outputIndex, const QString& message);

    private:
        /** 通道配置。 */
        Settings m_settings{};

        /** 通道专属场景控制器。 */
        std::unique_ptr<egret::SceneManagerViewModel> m_scene{};

        /** 通道专属离屏渲染器。 */
        std::unique_ptr<OffscreenSceneRenderer> m_renderer{};

        /** 套用源帧状态的累计耗时（毫秒）。 */
        double m_applyMs{0.0};
    };
}

#endif // EGRET_VIDEO_ENCODER_RENDER_LANE_H
