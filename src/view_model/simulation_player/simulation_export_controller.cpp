/**
 * @file        simulation_export_controller.cpp
 * @brief       离线仿真导出 QML 门面的实现文件。
 * @details     求解任务通过 QtConcurrent::run 提交到全局线程池；进度回调运行在
 *              工作线程，因此统一用 QMetaObject::invokeMethod + 队列连接回到
 *              UI 线程后再修改状态、发送信号。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 * @defgroup    Offline (离线求解)
 */

#include "simulation_export_controller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QtConcurrent>

#include <atomic>
#include <memory>
#include <utility>

#include "eps_format.h"
#include "offline_simulation_runner.h"

namespace egret
{
    SimulationExportController::SimulationExportController(QObject* parent)
        : QObject(parent)
    {
    }

    SimulationExportController::~SimulationExportController() = default;

    bool SimulationExportController::isRunning() const
    {
        return m_running;
    }

    double SimulationExportController::progress() const
    {
        return m_progress;
    }

    QString SimulationExportController::statusMessage() const
    {
        return m_statusMessage;
    }

    QString SimulationExportController::errorMessage() const
    {
        return m_errorMessage;
    }

    QString SimulationExportController::outputFilePath() const
    {
        return m_outputFilePath;
    }

    int SimulationExportController::frameCount() const
    {
        return m_frameCount;
    }

    double SimulationExportController::simulatedSeconds() const
    {
        return m_simulatedSeconds;
    }

    double SimulationExportController::fileSizeMb() const
    {
        return m_fileSizeMb;
    }

    double SimulationExportController::wallClockSeconds() const
    {
        return m_wallClockSeconds;
    }

    void SimulationExportController::setError(const QString& message)
    {
        m_errorMessage = message;
        emit errorMessageChanged();
    }

    void SimulationExportController::setRunning(const bool running)
    {
        if (m_running == running) {
            return;
        }
        m_running = running;
        emit runningChanged();
    }

    bool SimulationExportController::start(const QString& sceneJson,
                                           const double durationSeconds,
                                           const double solverStepMs,
                                           const double sampleIntervalMs)
    {
        if (m_running) {
            setError(QStringLiteral("已有离线求解任务正在进行"));
            return false;
        }
        if (sceneJson.trimmed().isEmpty()) {
            setError(QStringLiteral("当前没有可求解的场景，请先用「文件 → 打开」加载场景（.json）"));
            return false;
        }
        if (durationSeconds <= 0.0) {
            setError(QStringLiteral("仿真时长必须大于 0"));
            return false;
        }
        if (solverStepMs <= 0.0) {
            setError(QStringLiteral("求解步长必须大于 0"));
            return false;
        }

        const std::uint64_t solverStepNs{eps::secondsToNanoseconds(solverStepMs / 1000.0)};
        const std::uint64_t sampleIntervalNs{eps::secondsToNanoseconds(sampleIntervalMs / 1000.0)};

        if (!eps::isAllowedSampleInterval(sampleIntervalNs)) {
            setError(QStringLiteral("采样间隔只能是 1 / 2 / 5 / 10 / 20 ms"));
            return false;
        }
        if (!eps::decimationFor(solverStepNs, sampleIntervalNs).has_value()) {
            setError(QStringLiteral("采样间隔必须是求解步长的整数倍"));
            return false;
        }

        const QString outputPath{OfflineSimulationRunner::defaultSimulationFilePath()};
        const QString outputDir{QFileInfo(outputPath).absolutePath()};
        if (!QDir().mkpath(outputDir)) {
            setError(QStringLiteral("无法创建仿真缓存目录：%1").arg(outputDir));
            return false;
        }

        OfflineSimulationRunner::Request request{};
        request.sceneJson = sceneJson;
        request.outputPath = outputPath;
        request.durationSeconds = durationSeconds;
        request.solverStepNs = solverStepNs;
        request.sampleIntervalNs = sampleIntervalNs;

        // 进度回调在工作线程触发：只在整数百分比变化时投递一次事件，避免事件风暴。
        const QPointer<SimulationExportController> self{this};
        const auto lastPercent = std::make_shared<std::atomic<int>>(-1);
        request.progressCallback =
            [self, lastPercent](const std::uint64_t completedSteps, const std::uint64_t totalSteps)
            {
                if (self.isNull() || totalSteps == 0) {
                    return;
                }

                const int percent{static_cast<int>(completedSteps * 100ull / totalSteps)};
                int expected{lastPercent->load()};
                if (percent == expected || !lastPercent->compare_exchange_strong(expected, percent)) {
                    return;
                }

                const double ratio{static_cast<double>(completedSteps) / static_cast<double>(totalSteps)};
                QMetaObject::invokeMethod(
                    self.data(),
                    [self, ratio]()
                    {
                        if (self.isNull()) {
                            return;
                        }
                        self->m_progress = ratio;
                        emit self->progressChanged();
                    },
                    Qt::QueuedConnection);
            };

        m_progress = 0.0;
        m_errorMessage.clear();
        m_statusMessage = QStringLiteral("正在离线求解…");
        m_outputFilePath = outputPath;
        m_simulatedSeconds = durationSeconds;

        emit progressChanged();
        emit errorMessageChanged();
        emit statusMessageChanged();
        emit outputFilePathChanged();

        setRunning(true);

        std::ignore = QtConcurrent::run(
            [self, request]()
            {
                OfflineSimulationRunner::Report report{};
                QString errorMessage{};
                const bool success{OfflineSimulationRunner::run(request, &report, &errorMessage)};

                if (self.isNull()) {
                    return;
                }
                QMetaObject::invokeMethod(
                    self.data(),
                    [self, success, report, errorMessage]()
                    {
                        if (self.isNull()) {
                            return;
                        }
                        self->handleFinished(success, report, errorMessage);
                    },
                    Qt::QueuedConnection);
            });

        return true;
    }

    void SimulationExportController::handleFinished(const bool success,
                                                    const OfflineSimulationRunner::Report& report,
                                                    const QString& errorMessage)
    {
        setRunning(false);

        if (!success) {
            m_statusMessage = QStringLiteral("离线求解失败");
            emit statusMessageChanged();
            setError(errorMessage.isEmpty() ? QStringLiteral("离线求解失败") : errorMessage);
            emit finished(false);
            return;
        }

        m_progress = 1.0;
        m_frameCount = static_cast<int>(report.frameCount);
        m_fileSizeMb = static_cast<double>(report.fileBytes) / (1024.0 * 1024.0);
        m_wallClockSeconds = report.wallClockSeconds;
        m_statusMessage = QStringLiteral("求解完成：%1 帧，%2 MB，耗时 %3 s")
                              .arg(report.frameCount)
                              .arg(QString::number(m_fileSizeMb, 'f', 2))
                              .arg(QString::number(m_wallClockSeconds, 'f', 2));

        emit progressChanged();
        emit statusMessageChanged();
        emit resultChanged();
        emit finished(true);
    }

    bool SimulationExportController::saveAs(const QString& targetPath)
    {
        if (targetPath.trimmed().isEmpty()) {
            setError(QStringLiteral("保存路径为空"));
            return false;
        }
        if (m_outputFilePath.isEmpty() || !QFile::exists(m_outputFilePath)) {
            setError(QStringLiteral("没有可保存的仿真文件，请先执行离线仿真"));
            return false;
        }
        if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
            setError(QStringLiteral("目标文件已存在且无法覆盖：%1").arg(targetPath));
            return false;
        }
        if (!QFile::copy(m_outputFilePath, targetPath)) {
            setError(QStringLiteral("复制仿真文件失败：%1").arg(targetPath));
            return false;
        }

        m_statusMessage = QStringLiteral("已保存到 %1").arg(targetPath);
        emit statusMessageChanged();
        return true;
    }

    bool SimulationExportController::removeCacheFile()
    {
        if (m_outputFilePath.isEmpty() || !QFile::exists(m_outputFilePath)) {
            setError(QStringLiteral("缓存文件不存在"));
            return false;
        }
        if (!QFile::remove(m_outputFilePath)) {
            setError(QStringLiteral("删除缓存文件失败：%1").arg(m_outputFilePath));
            return false;
        }

        m_statusMessage = QStringLiteral("已删除缓存文件");
        emit statusMessageChanged();
        return true;
    }

    QString SimulationExportController::defaultFilePath() const
    {
        return OfflineSimulationRunner::defaultSimulationFilePath();
    }
}
