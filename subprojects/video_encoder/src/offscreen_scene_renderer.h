/**
 * @file        offscreen_scene_renderer.h
 * @brief       基于 QQuickRenderControl 的离屏 3D 场景渲染器。
 * @details     渲染器复用播放器的同一个 QML 视口组件（SimulationViewport），因此
 *              灯光、材质、网格、相机映射与播放器完全一致；区别只在于：
 *
 *              1. 场景挂在一个**不会显示**的 QQuickWindow 上，渲染由本类手动驱动；
 *              2. 渲染目标是自建的 QRhi 纹理（不是屏幕），分辨率由调用方指定，
 *                 因此导出 4K 也不受窗口尺寸限制；
 *              3. 每帧渲染完成后通过 QRhi 回读（readBackTexture）得到 QImage。
 *
 *              线程约束：QQuickRenderControl 未调用 prepareThread() 时，sync() 与 render()
 *              必须在创建它的线程执行，因此本类当前运行在主线程；后续接入 UI 时可把
 *              sync/render 迁移到导出线程（见视频导出设计文档）。
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

#ifndef EGRET_VIDEO_ENCODER_OFFSCREEN_SCENE_RENDERER_H
#define EGRET_VIDEO_ENCODER_OFFSCREEN_SCENE_RENDERER_H

#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QVideoFrameFormat>

#include <memory>

#include "video_encoder_options.h"

class QQmlComponent;
class QQmlEngine;
class QQuickItem;
class QQuickRenderControl;
class QQuickWindow;
class QRhiRenderBuffer;
class QRhiRenderPassDescriptor;
class QRhiTexture;
class QRhiTextureRenderTarget;

namespace egret
{
    class ResourceHelper;
    class SceneManagerViewModel;
}

namespace egret::video
{
    /**
     * @brief       把 epsx 场景渲染成逐帧图像（离屏、可在任意分辨率下工作）。
     * @invariant   initialize() 成功返回后，renderFrame() 可以按任意顺序重复调用；
     *              每帧渲染结果都是「当前 SceneManagerViewModel 状态」的完整快照。
     */
    class OffscreenSceneRenderer final : public QObject
    {
        Q_OBJECT

    public:
        /** 渲染器配置。 */
        struct Settings
        {
            /** 输出分辨率（像素）。 */
            QSize resolution{1920, 1080};

            /** 初始相机姿态。 */
            CameraPose camera{};

            /** 是否渲染网格与坐标轴。 */
            bool showGrid{true};
        };

        /**
         * @brief  构造渲染器。
         * @param  scene 场景数据来源（epsx 的加载与逐帧状态套用由它负责）。
         * @param  parent Qt 父对象。
         */
        explicit OffscreenSceneRenderer(egret::SceneManagerViewModel* scene, QObject* parent = nullptr);

        /** 析构：按「图形资源 → 窗口 → 引擎」的顺序释放。 */
        ~OffscreenSceneRenderer() override;

        /**
         * @brief  建立 QML 场景并初始化离屏渲染资源。
         * @param  settings 渲染配置。
         * @param  errorMessage 失败原因输出，可为 nullptr。
         * @return 成功返回 true。
         * @remark 初始化末尾会预热渲染若干帧并丢弃结果，用于尽早暴露驱动/QML 错误。
         */
        [[nodiscard]] bool initialize(const Settings& settings, QString* errorMessage);

        /** 是否初始化完成。 */
        [[nodiscard]] bool isInitialized() const;

        /** 当前渲染分辨率。 */
        [[nodiscard]] QSize resolution() const;

        /** 送入帧时应使用的像素格式（与渲染目标一致）。 */
        [[nodiscard]] QVideoFrameFormat frameFormat() const;

        /**
         * @brief 更新相机姿态（下一帧生效）。
         * @param camera 相机姿态（物理坐标系）。
         */
        void setCamera(const CameraPose& camera);

        /**
         * @brief  渲染一帧并回读为图像。
         * @param  errorMessage 失败原因输出，可为 nullptr。
         * @return 渲染结果；失败时返回空图像。
         */
        [[nodiscard]] QImage renderFrame(QString* errorMessage);

    private:
        /**
         * @brief  创建 QRhi 颜色纹理、深度缓冲与渲染目标。
         * @param  errorMessage 失败原因输出，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] bool createGraphicsResources(QString* errorMessage);

        /** 把相机姿态写入 QML 场景并立即应用。 */
        void applyCameraToScene(const CameraPose& camera);

        /** 场景数据来源（不拥有所有权）。 */
        egret::SceneManagerViewModel* m_scene{nullptr};

        /**
         * @brief 导出场景里 BodyDelegate 需要的资源解析器。
         * @remark 放在引擎之前声明，保证它在 QML 引擎（及其对象树）之后析构。
         */
        std::unique_ptr<egret::ResourceHelper> m_resourceHelper{};

        /** QML 引擎（独立于主界面引擎）。 */
        std::unique_ptr<QQmlEngine> m_engine{};

        /** 离屏场景组件。 */
        std::unique_ptr<QQmlComponent> m_component{};

        /** 渲染控制器。 */
        std::unique_ptr<QQuickRenderControl> m_renderControl{};

        /** 离屏窗口（从不显示，只承载场景）。 */
        std::unique_ptr<QQuickWindow> m_window{};

        /** 场景根对象（由离屏窗口持有）。 */
        QQuickItem* m_rootItem{nullptr};

        /** 颜色渲染目标纹理。 */
        std::unique_ptr<QRhiTexture> m_colorTexture{};

        /** 深度模板缓冲。 */
        std::unique_ptr<QRhiRenderBuffer> m_depthStencilBuffer{};

        /** 离屏渲染目标。 */
        std::unique_ptr<QRhiTextureRenderTarget> m_renderTarget{};

        /** 与渲染目标匹配的渲染通道描述。 */
        std::unique_ptr<QRhiRenderPassDescriptor> m_renderPassDescriptor{};

        /** 当前渲染分辨率。 */
        QSize m_resolution{};

        /** 当前相机姿态。 */
        CameraPose m_camera{};

        /** 是否初始化完成。 */
        bool m_initialized{false};
    };
}

#endif // EGRET_VIDEO_ENCODER_OFFSCREEN_SCENE_RENDERER_H
