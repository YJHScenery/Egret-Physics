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
#include <QAbstractVideoBuffer>
#include <QElapsedTimer>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QUrl>
#include <QVariant>

#include <rhi/qrhi.h>

#include <cstring>
#include <memory>
#include <vector>

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

        /**
         * @brief       直接包住 GPU 回读缓冲的只读视频缓冲。
         * @details     回读数据由 QByteArray 持有（隐式共享 + 引用计数），QVideoFrame 在
         *              编码线程读取期间会一直持有一份拷贝，因此整条链路不需要复制整帧像素。
         *              缓冲只允许只读映射：像素由渲染器产生，编码器只读取。
         */
        class ReadbackVideoBuffer final : public QAbstractVideoBuffer
        {
        public:
            /**
             * @brief  构造只读缓冲。
             * @param  data 回读得到的像素数据（所有权转移）。
             * @param  bytesPerLine 每行字节数。
             * @param  format 像素格式描述。
             */
            ReadbackVideoBuffer(QByteArray data, const int bytesPerLine, const QVideoFrameFormat& format) :
                m_data(std::move(data)),
                m_bytesPerLine(bytesPerLine),
                m_format(format)
            {
            }

            /**
             * @brief  映射缓冲供读取。
             * @param  mode 映射模式。
             * @return 单平面映射数据；写模式返回空映射。
             */
            [[nodiscard]] MapData map(const QVideoFrame::MapMode mode) override
            {
                MapData mapData{};
                if (mode != QVideoFrame::ReadOnly) {
                    return mapData;
                }

                mapData.planeCount = 1;
                mapData.data[0] = reinterpret_cast<uchar*>(m_data.data());
                mapData.bytesPerLine[0] = m_bytesPerLine;
                mapData.dataSize[0] = static_cast<int>(m_data.size());
                return mapData;
            }

            /** 解除映射（只读缓冲无需额外处理）。 */
            void unmap() override
            {
            }

            /** 返回像素格式。 */
            [[nodiscard]] QVideoFrameFormat format() const override
            {
                return m_format;
            }

        private:
            /** 像素数据。 */
            QByteArray m_data;

            /** 每行字节数。 */
            int m_bytesPerLine{0};

            /** 像素格式描述。 */
            QVideoFrameFormat m_format{};
        };

        /**
         * @brief  原地上下翻转图像行序。
         * @param  pixels 像素缓冲（按行存放）。
         * @param  height 图像高度。
         * @param  bytesPerLine 每行字节数。
         * @remark 只交换行，不额外分配整帧内存，可省下约一半内存带宽。
         */
        void flipRowsInPlace(QByteArray& pixels, const int height, const int bytesPerLine)
        {
            if (height < 2 || bytesPerLine <= 0) {
                return;
            }

            uchar* const data{reinterpret_cast<uchar*>(pixels.data())};
            std::vector<uchar> scratch(static_cast<std::size_t>(bytesPerLine));
            for (int row = 0; row < height / 2; ++row) {
                uchar* const topRow{data + static_cast<qsizetype>(row) * bytesPerLine};
                uchar* const bottomRow{data + static_cast<qsizetype>(height - 1 - row) * bytesPerLine};
                std::memcpy(scratch.data(), topRow, static_cast<std::size_t>(bytesPerLine));
                std::memcpy(topRow, bottomRow, static_cast<std::size_t>(bytesPerLine));
                std::memcpy(bottomRow, scratch.data(), static_cast<std::size_t>(bytesPerLine));
            }
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

    const OffscreenSceneRenderer::Timings& OffscreenSceneRenderer::timings() const
    {
        return m_timings;
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
            const QVideoFrame frame{renderFrame(&warmupError)};
            if (!frame.isValid()) {
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

    QVideoFrame OffscreenSceneRenderer::renderFrame(QString* errorMessage)
    {
        if (!m_initialized) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("离屏渲染器尚未初始化");
            }
            return {};
        }

        QElapsedTimer stageTimer{};
        stageTimer.start();
        m_renderControl->polishItems();
        m_renderControl->beginFrame();
        m_renderControl->sync();
        m_timings.polishAndSyncMs += static_cast<double>(stageTimer.nsecsElapsed()) / 1.0e6;

        stageTimer.restart();
        m_renderControl->render();
        m_timings.renderMs += static_cast<double>(stageTimer.nsecsElapsed()) / 1.0e6;

        stageTimer.restart();
        QRhi* const rhi{m_renderControl->rhi()};
        QRhiReadbackResult readback{};
        QRhiResourceUpdateBatch* const batch{rhi->nextResourceUpdateBatch()};
        batch->readBackTexture(m_colorTexture.get(), &readback);
        m_renderControl->commandBuffer()->resourceUpdate(batch);
        m_renderControl->endFrame();
        m_timings.readbackMs += static_cast<double>(stageTimer.nsecsElapsed()) / 1.0e6;

        if (readback.data.isEmpty() || readback.pixelSize.height() <= 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("离屏渲染回读失败：没有拿到像素数据");
            }
            return {};
        }

        // 离屏帧不做流水线，endFrame() 返回时回读已经完成；这里直接接管回读缓冲，
        // 只按需做一次原地行交换，避免「整帧翻转 + 再拷贝进 QVideoFrame」两次 8 MB 级拷贝。
        const QSize pixelSize{readback.pixelSize};
        const int bytesPerLine{static_cast<int>(readback.data.size() / pixelSize.height())};
        QByteArray pixels{std::move(readback.data)};
        if (rhi->isYUpInFramebuffer()) {
            flipRowsInPlace(pixels, pixelSize.height(), bytesPerLine);
        }

        const QVideoFrameFormat format{pixelSize, QVideoFrameFormat::Format_RGBA8888};
        std::unique_ptr<QAbstractVideoBuffer> buffer{
            std::make_unique<ReadbackVideoBuffer>(std::move(pixels), bytesPerLine, format)};
        QVideoFrame frame{buffer.release(), format};

        m_timings.imageMs += static_cast<double>(stageTimer.nsecsElapsed()) / 1.0e6;
        ++m_timings.frameCount;
        return frame;
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
