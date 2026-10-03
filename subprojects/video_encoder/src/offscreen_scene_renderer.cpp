/**
 * @file        offscreen_scene_renderer.cpp
 * @brief       离屏 3D 场景渲染器实现。
 * @details     渲染循环严格遵循 Qt 官方 RHI 离屏渲染示例的顺序：
 *              polishItems() → beginFrame() → sync() → render() → 回读 → endFrame()。
 *              离屏帧不做流水线，因此 endFrame() 返回后回读数据已经就绪，可以直接构造 QImage。
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

#include "offscreen_scene_renderer.h"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QUrl>
#include <QVariant>

#include <rhi/qrhi.h>

#include <memory>

#include "resource_helper.h"
#include "scene_manager.h"

namespace egret::video
{
    namespace
    {
        /** 离屏场景的 QML 资源路径。 */
        constexpr auto SCENE_QML_PATH{"qrc:/video_encoder/qml/VideoExportScene.qml"};

        /** 初始化时预热渲染的帧数：首帧常因延迟创建而缺少内容。 */
        constexpr int WARMUP_FRAME_COUNT{2};

        /**
         * @brief  汇总 QML 组件的错误信息。
         * @param  component 出错的组件。
         * @return 每行一个错误的文本。
         */
        QString collectComponentErrors(const QQmlComponent& component)
        {
            QStringList messages{};
            for (const QQmlError& error : component.errors()) {
                messages.append(error.toString());
            }
            return messages.join(QLatin1Char('\n'));
        }
    }

    OffscreenSceneRenderer::OffscreenSceneRenderer(egret::SceneManagerViewModel* scene, QObject* parent) :
        QObject(parent),
        m_scene(scene)
    {
    }

    OffscreenSceneRenderer::~OffscreenSceneRenderer()
    {
        // 显式按「图形资源 → 窗口（含 QML 场景）→ 引擎」的顺序释放：
        // QRhi 资源必须在渲染控制器销毁前释放，QML 场景必须在引擎销毁前释放。
        m_renderPassDescriptor.reset();
        m_renderTarget.reset();
        m_depthStencilBuffer.reset();
        m_colorTexture.reset();
        m_window.reset();
        m_component.reset();
        m_renderControl.reset();
        m_engine.reset();
        m_resourceHelper.reset();
    }

    bool OffscreenSceneRenderer::isInitialized() const
    {
        return m_initialized;
    }

    QSize OffscreenSceneRenderer::resolution() const
    {
        return m_resolution;
    }

    QVideoFrameFormat OffscreenSceneRenderer::frameFormat() const
    {
        return QVideoFrameFormat{m_resolution, QVideoFrameFormat::Format_RGBA8888};
    }

    bool OffscreenSceneRenderer::initialize(const Settings& settings, QString* errorMessage)
    {
        if (m_initialized) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("离屏渲染器已经初始化，不能重复调用");
            }
            return false;
        }
        if (m_scene == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("离屏渲染器缺少场景数据来源");
            }
            return false;
        }
        if (settings.resolution.width() < 2 || settings.resolution.height() < 2) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("离屏渲染分辨率非法：%1x%2")
                                    .arg(settings.resolution.width())
                                    .arg(settings.resolution.height());
            }
            return false;
        }

        m_resolution = settings.resolution;
        m_camera = settings.camera;

        // 导出场景复用播放器视口组件，因此需要与主程序一致的上下文属性。
        m_resourceHelper = std::make_unique<egret::ResourceHelper>();
        m_engine = std::make_unique<QQmlEngine>();
        m_engine->rootContext()->setContextProperty(QStringLiteral("sceneController"), m_scene);
        m_engine->rootContext()->setContextProperty(QStringLiteral("resourceHelper"), m_resourceHelper.get());

        m_renderControl = std::make_unique<QQuickRenderControl>();
        m_window = std::make_unique<QQuickWindow>(m_renderControl.get());
        m_window->setGeometry(0, 0, m_resolution.width(), m_resolution.height());
        m_window->contentItem()->setSize(QSizeF{m_resolution});

        m_component = std::make_unique<QQmlComponent>(m_engine.get(), QUrl{QString::fromUtf8(SCENE_QML_PATH)});
        if (m_component->isError()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("加载离屏导出场景失败：\n%1").arg(collectComponentErrors(*m_component));
            }
            return false;
        }

        std::unique_ptr<QObject> rootOwner{m_component->create()};
        if (m_component->isError() || rootOwner == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("创建离屏导出场景失败：\n%1").arg(collectComponentErrors(*m_component));
            }
            return false;
        }

        m_rootItem = qobject_cast<QQuickItem*>(rootOwner.get());
        if (m_rootItem == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("离屏导出场景的根对象必须是 QQuickItem");
            }
            return false;
        }

        // 所有权转交离屏窗口的内容项：窗口析构时场景随之一并销毁。
        rootOwner.release();
        m_rootItem->setParentItem(m_window->contentItem());
        m_rootItem->setWidth(m_resolution.width());
        m_rootItem->setHeight(m_resolution.height());
        m_rootItem->setProperty("showGrid", settings.showGrid);
        applyCameraToScene(m_camera);

        if (!m_renderControl->initialize()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("离屏渲染初始化失败：无法创建图形设备（RHI）");
            }
            return false;
        }
        if (m_renderControl->rhi() == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("离屏渲染初始化失败：渲染控制器没有返回 QRhi");
            }
            return false;
        }
        if (!createGraphicsResources(errorMessage)) {
            return false;
        }

        m_window->setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(m_renderTarget.get()));
        m_initialized = true;

        // 预热：让 Repeater3D 完成首次委托创建，并提前验证「渲染 + 回读」整条通路。
        for (int index = 0; index < WARMUP_FRAME_COUNT; ++index) {
            QString warmupError{};
            const QImage frame{renderFrame(&warmupError)};
            if (frame.isNull()) {
                m_initialized = false;
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("离屏渲染预热失败：%1").arg(warmupError);
                }
                return false;
            }
        }
        return true;
    }

    void OffscreenSceneRenderer::setCamera(const CameraPose& camera)
    {
        m_camera = camera;
        applyCameraToScene(camera);
    }

    QImage OffscreenSceneRenderer::renderFrame(QString* errorMessage)
    {
        if (!m_initialized) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("离屏渲染器尚未初始化");
            }
            return {};
        }

        m_renderControl->polishItems();
        m_renderControl->beginFrame();
        m_renderControl->sync();
        m_renderControl->render();

        QRhi* const rhi{m_renderControl->rhi()};
        QRhiReadbackResult readback{};
        QRhiResourceUpdateBatch* const batch{rhi->nextResourceUpdateBatch()};
        batch->readBackTexture(m_colorTexture.get(), &readback);
        m_renderControl->commandBuffer()->resourceUpdate(batch);
        m_renderControl->endFrame();

        if (readback.data.isEmpty() || readback.pixelSize.height() <= 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("离屏渲染回读失败：没有拿到像素数据");
            }
            return {};
        }

        // 离屏帧不做流水线，endFrame() 返回时回读已经完成；这里构造的 QImage 只是包装，
        // 因此 flipped()/copy() 会生成独立副本，避免引用已释放的回读缓冲。
        const QImage wrapper{reinterpret_cast<const uchar*>(readback.data.constData()),
                             readback.pixelSize.width(),
                             readback.pixelSize.height(),
                             static_cast<qsizetype>(readback.data.size() / readback.pixelSize.height()),
                             QImage::Format_RGBA8888_Premultiplied};
        return rhi->isYUpInFramebuffer() ? wrapper.flipped() : wrapper.copy();
    }

    bool OffscreenSceneRenderer::createGraphicsResources(QString* errorMessage)
    {
        QRhi* const rhi{m_renderControl->rhi()};

        // 主通道不做 MSAA：抗锯齿由 Quick3D 自身的离屏缓冲按 SceneEnvironment 设置完成。
        constexpr int SAMPLE_COUNT{1};

        m_colorTexture.reset(rhi->newTexture(QRhiTexture::RGBA8,
                                             m_resolution,
                                             SAMPLE_COUNT,
                                             QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        if (!m_colorTexture->create()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("创建离屏颜色纹理失败（%1x%2）")
                                    .arg(m_resolution.width())
                                    .arg(m_resolution.height());
            }
            return false;
        }

        m_depthStencilBuffer.reset(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, m_resolution, SAMPLE_COUNT));
        if (!m_depthStencilBuffer->create()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("创建离屏深度模板缓冲失败");
            }
            return false;
        }

        QRhiTextureRenderTargetDescription description{QRhiColorAttachment{m_colorTexture.get()}};
        description.setDepthStencilBuffer(m_depthStencilBuffer.get());
        m_renderTarget.reset(rhi->newTextureRenderTarget(description));
        m_renderPassDescriptor.reset(m_renderTarget->newCompatibleRenderPassDescriptor());
        m_renderTarget->setRenderPassDescriptor(m_renderPassDescriptor.get());
        if (!m_renderTarget->create()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("创建离屏渲染目标失败");
            }
            return false;
        }
        return true;
    }

    void OffscreenSceneRenderer::applyCameraToScene(const CameraPose& camera)
    {
        if (m_rootItem == nullptr) {
            return;
        }

        m_rootItem->setProperty("cameraPosition", QVariant::fromValue(camera.position));
        m_rootItem->setProperty("cameraTarget", QVariant::fromValue(camera.target));
        m_rootItem->setProperty("cameraFieldOfView", camera.fieldOfView);
        m_rootItem->setProperty("cameraClipNear", camera.clipNear);
        m_rootItem->setProperty("cameraClipFar", camera.clipFar);
        QMetaObject::invokeMethod(m_rootItem, "applyCamera");
    }
}
