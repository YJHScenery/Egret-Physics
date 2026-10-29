/**
 * @file video_export_controller.cpp
 * @brief 实现播放器的视频导出控制器。
 */
#include "video_export_controller.h"
#include <QFileInfo>
#include <QUrl>

namespace egret::video {
VideoExportController::VideoExportController(QObject* parent) : QObject(parent) {}

bool VideoExportController::start(const QString& input, const QUrl& output, int width, int height, int fps, const QVariantMap& camera, bool grid)
{
    if (m_running) return false;
    const QString path = output.toLocalFile();
    if (input.isEmpty() || path.isEmpty() || width <= 0 || height <= 0 || width % 2 || height % 2 || fps < 0 || QFileInfo(input).absoluteFilePath().compare(QFileInfo(path).absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        m_message = QStringLiteral("请选择有效的输入和输出文件，并使用正偶数分辨率。");
        emit changed();
        return false;
    }
    Options options;
    options.inputPath = input;
    options.outputPath = path;
    options.width = width;
    options.height = height;
    options.frameRate = fps;
    options.showGrid = grid;
    options.camera.hasExplicitPose = true;
    options.camera.position = QVector3D(camera.value("positionX").toFloat(), camera.value("positionY").toFloat(), camera.value("positionZ").toFloat());
    options.camera.target = QVector3D(camera.value("targetX").toFloat(), camera.value("targetY").toFloat(), camera.value("targetZ").toFloat());
    options.camera.fieldOfView = camera.value("fovY").toFloat();
    options.camera.clipNear = camera.value("nearClip").toFloat();
    options.camera.clipFar = camera.value("farClip").toFloat();
    m_encoder = std::make_unique<EpsxVideoEncoder>();
    connect(m_encoder.get(), &EpsxVideoEncoder::progress, this, [this](quint64 done, quint64 total) {
        m_progress = total ? static_cast<double>(done) / static_cast<double>(total) : 0;
        emit changed();
    });
    connect(m_encoder.get(), &EpsxVideoEncoder::finished, this, [this](bool success, const QString& message) {
        m_running = false;
        m_message = m_encoder->report().canceled ? QStringLiteral("导出已取消，已写入的部分已保存。") :
            (success ? QStringLiteral("视频导出完成：%1").arg(m_encoder->report().outputPath) : QStringLiteral("导出失败：%1").arg(message));
        emit changed();
        emit completed(success, m_message);
    });
    m_progress = 0;
    m_message.clear();
    QString error;
    if (!m_encoder->start(options, &error)) {
        m_message = QStringLiteral("启动导出失败：%1").arg(error);
        emit changed();
        return false;
    }
    m_running = true;
    emit changed();
    return true;
}

void VideoExportController::cancel() { if (m_running) m_encoder->cancel(); }
}
