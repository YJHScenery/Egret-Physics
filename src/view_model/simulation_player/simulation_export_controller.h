/**
 * @file        simulation_export_controller.h
 * @brief       离线仿真导出的 QML 门面头文件。
 * @details     SimulationExportController 把「离线求解」从 QML 中隔离开来，负责：
 *
 *              1. 校验表单参数（求解步长 / 采样间隔 / 仿真时长）；
 *              2. 把 OfflineSimulationRunner::run() 提交到线程池执行，避免阻塞 UI；
 *              3. 把求解进度回调转换为 Qt 信号，供进度条绑定；
 *              4. 管理缓存文件路径，并提供另存为与删除。
 *
 *              该类只负责调度与状态呈现，不含任何物理逻辑；求解本身仍由
 *              OfflineSimulationRunner 完成，保证界面路径与命令行路径结果一致。
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

#ifndef EGRET_PHYSICS_SIMULATION_EXPORT_CONTROLLER_H
#define EGRET_PHYSICS_SIMULATION_EXPORT_CONTROLLER_H

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

#include "offline_simulation_runner.h"

namespace egret
{
    /**
     * @brief       离线仿真导出的 QML 门面对象。
     * @details     在 main.cpp 中以上下文属性 simulationExporter 注册给 QML；
     *              求解任务在线程池中执行，进度与结果通过信号回到 UI 线程。
     *
     * @invariant   running 为 true 时 start() 直接失败，不会并发启动第二个求解任务
     * @remark      求解过程的中间结果全部写入缓存文件，另存为只是复制缓存文件
     */
    class SimulationExportController final : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

        Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

        Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

        Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

        Q_PROPERTY(QString outputFilePath READ outputFilePath NOTIFY outputFilePathChanged)

        Q_PROPERTY(int frameCount READ frameCount NOTIFY resultChanged)

        Q_PROPERTY(double simulatedSeconds READ simulatedSeconds NOTIFY resultChanged)

        Q_PROPERTY(double fileSizeMb READ fileSizeMb NOTIFY resultChanged)

        Q_PROPERTY(double wallClockSeconds READ wallClockSeconds NOTIFY resultChanged)

        Q_PROPERTY(bool cancelRequested READ isCancelRequested NOTIFY cancelRequestedChanged)

    public:
        /** 默认构造。 */
        explicit SimulationExportController(QObject* parent = nullptr);

        /** 默认析构。 */
        ~SimulationExportController() override;

        /** 是否正在求解。 */
        [[nodiscard]] bool isRunning() const;

        /** 求解进度比例，取值 0 ~ 1。 */
        [[nodiscard]] double progress() const;

        /** 面向用户的状态描述。 */
        [[nodiscard]] QString statusMessage() const;

        /** 最近一次失败的原因；无错误时为空串。 */
        [[nodiscard]] QString errorMessage() const;

        /** 缓存文件路径。 */
        [[nodiscard]] QString outputFilePath() const;

        /** 最近一次求解写入的帧数。 */
        [[nodiscard]] int frameCount() const;

        /** 最近一次请求的仿真时长（秒）。 */
        [[nodiscard]] double simulatedSeconds() const;

        /** 最近一次输出文件大小（MB）。 */
        [[nodiscard]] double fileSizeMb() const;

        /** 最近一次求解与写盘消耗的真实时间（秒）。 */
        [[nodiscard]] double wallClockSeconds() const;

        /** 是否已经发出取消请求（求解线程会在下一个检查点结束）。 */
        [[nodiscard]] bool isCancelRequested() const;

        /**
         * @brief  启动一次离线求解。
         * @param  sceneJson 场景 JSON 文本（取自 SceneManagerViewModel::currentSceneJson()）。
         * @param  durationSeconds 仿真总时长（秒），必须大于 0。
         * @param  solverStepMs 求解推进步长（毫秒），必须大于 0。
         * @param  sampleIntervalMs 采样间隔（毫秒），只允许 1 / 2 / 5 / 10 / 20。
         * @return 已提交任务返回 true；参数非法或已有任务在运行时返回 false。
         */
        Q_INVOKABLE [[nodiscard]] bool start(const QString& sceneJson,
                                             double durationSeconds,
                                             double solverStepMs,
                                             double sampleIntervalMs);

        /**
         * @brief  把缓存文件复制到目标路径。
         * @param  targetPath 目标文件路径（通常来自保存对话框）。
         * @return 成功返回 true。
         */
        Q_INVOKABLE [[nodiscard]] bool saveAs(const QString& targetPath);

        /**
         * @brief  删除缓存文件。
         * @return 成功返回 true。
         */
        Q_INVOKABLE [[nodiscard]] bool removeCacheFile();

        /**
         * @brief  请求取消当前离线求解。
         * @details 求解线程每 100 步检查一次；收到取消后仍会正常收尾（写索引块与尾块），
         *          因此已求解出的帧可以照常播放，状态栏会显示「已取消」。
         *          没有任务在跑时调用无副作用。
         */
        Q_INVOKABLE void cancel();

        /** 返回默认缓存文件路径（不创建文件）。 */
        Q_INVOKABLE [[nodiscard]] QString defaultFilePath() const;

    signals:
        /** 运行状态变化。 */
        void runningChanged();

        /** 求解进度变化。 */
        void progressChanged();

        /** 状态描述变化。 */
        void statusMessageChanged();

        /** 错误描述变化。 */
        void errorMessageChanged();

        /** 缓存文件路径变化。 */
        void outputFilePathChanged();

        /** 求解结果指标变化。 */
        void resultChanged();

        /** 取消请求状态变化。 */
        void cancelRequestedChanged();

        /**
         * @brief 一次求解结束。
         * @param success 成功为 true。
         */
        void finished(bool success);

    private:
        /**
         * @brief 记录错误并通知 QML。
         * @param message 错误描述。
         */
        void setError(const QString& message);

        /**
         * @brief 更新运行状态并发送信号。
         * @param running 目标状态。
         */
        void setRunning(bool running);

        /**
         * @brief 求解结束时在 UI 线程汇总结果。
         * @param success 求解是否成功。
         * @param report 求解报告。
         * @param errorMessage 失败原因。
         */
        void handleFinished(bool success, const OfflineSimulationRunner::Report& report, const QString& errorMessage);

        /** 是否正在求解。 */
        bool m_running{false};

        /**
         * @brief 取消标志。
         * @details 由 UI 线程置位、求解线程读取，因此用 shared_ptr<atomic> 持有：
         *          即便控制器先于任务销毁，工作线程也不会访问悬空对象。
         */
        std::shared_ptr<std::atomic<bool>> m_cancelFlag{std::make_shared<std::atomic<bool>>(false)};

        /** 求解进度比例。 */
        double m_progress{0.0};

        /** 状态描述。 */
        QString m_statusMessage{};

        /** 错误描述。 */
        QString m_errorMessage{};

        /** 缓存文件路径。 */
        QString m_outputFilePath{};

        /** 最近一次写入的帧数。 */
        int m_frameCount{0};

        /** 最近一次请求的仿真时长（秒）。 */
        double m_simulatedSeconds{0.0};

        /** 最近一次输出文件大小（MB）。 */
        double m_fileSizeMb{0.0};

        /** 最近一次求解耗时（秒）。 */
        double m_wallClockSeconds{0.0};
    };
}

#endif // EGRET_PHYSICS_SIMULATION_EXPORT_CONTROLLER_H
