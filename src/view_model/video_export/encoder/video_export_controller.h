/**
 * @file video_export_controller.h
 * @brief 播放器视频导出控制器。
 */
#pragma once
#include <QObject>
#include <QVariantMap>
#include <memory>
#include "epsx_video_encoder.h"

namespace egret::video {
/** @brief 向界面提供异步视频导出、进度和取消操作。 */
class VideoExportController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    /** @brief 创建导出控制器。 @param parent 父对象。 */
    explicit VideoExportController(QObject* parent = nullptr);
    /** @brief 返回是否正在导出。 */
    bool running() const { return m_running; }
    /** @brief 返回导出进度。 */
    double progress() const { return m_progress; }
    /** @brief 返回最近的导出结果。 */
    QString message() const { return m_message; }
    /** @brief 以播放器相机和输出参数启动导出。 @param input 输入文件。 @param output 输出文件 URL。 @param width 宽度。 @param height 高度。 @param fps 帧率，零表示跟随采样率。 @param camera 相机状态。 @param grid 是否显示网格。 @return 启动是否成功。 */
    Q_INVOKABLE bool start(const QString& input, const QUrl& output, int width, int height, int fps, const QVariantMap& camera, bool grid);
    /** @brief 请求取消导出，已生成的视频会正常收尾。 */
    Q_INVOKABLE void cancel();
signals:
    /** @brief 状态变化。 */
    void changed();
    /** @brief 导出结束。 @param success 是否成功。 @param message 结果说明。 */
    void completed(bool success, const QString& message);
private:
    std::unique_ptr<EpsxVideoEncoder> m_encoder;
    bool m_running{false};
    double m_progress{0};
    QString m_message;
};
}
