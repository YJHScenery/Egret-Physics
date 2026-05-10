//
// 由 GitHub Copilot 于 2026/4/23 创建。
//

#ifndef EGRET_PHYSICS_SCENE_MANAGER_VIEWMODEL_H
#define EGRET_PHYSICS_SCENE_MANAGER_VIEWMODEL_H

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <memory>

#include "scene_body_model.h"
#include "world_scene_manager.h"

namespace egret
{
    class WorldSceneManager;

    /**
     * @brief Qt / ViewModel 层的场景门面对象。
     *
     * 该对象直接暴露给 QML，负责：
     * 1. 作为 UI 与模型层之间的唯一交互入口。
     * 2. 使用 QTimer 驱动模型层 tick，并处理固定步长累积。
     * 3. 将模型层输出的轻量化渲染快照转换为 QML 可绑定模型。
     */
    class SceneManagerViewModel final : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

        Q_PROPERTY(double simTime READ simTime NOTIFY simTimeChanged)

        Q_PROPERTY(quint64 stepCount READ stepCount NOTIFY stepCountChanged)

        Q_PROPERTY(int entityCount READ entityCount NOTIFY entityCountChanged)

        Q_PROPERTY(double fps READ fps NOTIFY fpsChanged)

        Q_PROPERTY(double fixedStepMs READ fixedStepMs NOTIFY fixedStepMsChanged)

        Q_PROPERTY(SceneBodyModel* bodyModel READ bodyModel CONSTANT)

    public:
        /** 默认构造。 */
        explicit SceneManagerViewModel(QObject* parent = nullptr);

        /** 默认析构。 */
        ~SceneManagerViewModel() override = default;

        /** 当前是否运行。 */
        [[nodiscard]] bool isRunning() const;

        /** 当前仿真时间。 */
        [[nodiscard]] double simTime() const;

        /** 当前步数。 */
        [[nodiscard]] quint64 stepCount() const;

        /** 当前实体数量。 */
        [[nodiscard]] int entityCount() const;

        /** 当前帧率。 */
        [[nodiscard]] double fps() const;

        /** 固定步长，单位毫秒。 */
        [[nodiscard]] double fixedStepMs() const;

        /** QML 直接使用的实体列表模型。 */
        [[nodiscard]] SceneBodyModel* bodyModel();

        /** 开始运行。 */
        Q_INVOKABLE void play();

        /** 暂停运行。 */
        Q_INVOKABLE void pause();

        /** 切换运行状态。 */
        Q_INVOKABLE void toggleRunning();

        /** 执行一次固定步长求解。 */
        Q_INVOKABLE void stepOnce();

        /** 重置场景。 */
        Q_INVOKABLE void reset();

        /** 生成一个默认球体。 */
        Q_INVOKABLE void spawnSphere();

        /** 生成一个默认盒体。 */
        Q_INVOKABLE void spawnBox();

    signals:
        /** 运行状态变化。 */
        void runningChanged();

        /** 仿真时间变化。 */
        void simTimeChanged();

        /** 步数变化。 */
        void stepCountChanged();

        /** 实体数量变化。 */
        void entityCountChanged();

        /** 帧率变化。 */
        void fpsChanged();

        /** 固定步长变化。 */
        void fixedStepMsChanged();

        /** 一帧求解完成。 */
        void frameAdvanced();

        /** 场景被重置。 */
        void sceneReset();

        /** 实体集合发生变化。 */
        void entityChanged();

    private slots:
        /** 定时器回调，用于驱动固定步长累积。 */
        void onFrameTick();

    private:
        /**
         * @brief 重新构建演示场景。
         *
         * 这里放置第一阶段最小闭环所需的示例数据：
         * 1. 一个重力场。
         * 2. 一个静态地面盒。
         * 3. 若干可碰撞球体。
         */
        void rebuildDemoWorld();

        /**
         * @brief 将模型层世界快照刷新到 Qt 列表模型。
         */
        void refreshBodyModel();

        /**
         * @brief 执行单次固定步长推进的内部函数。
         * @param emitFrameSignal 是否在完成后发出 frameAdvanced 信号。
         */
        void advanceFixedStep(bool emitFrameSignal);

        /**
         * @brief 更新帧率统计。
         * @param realFrameSeconds 实际帧耗时。
         */
        void updateFps(double realFrameSeconds);

        /**
         * @brief 设置运行状态并同步信号。
         * @param running 目标状态。
         */
        void setRunning(bool running);

        /** 当前运行状态。 */
        bool m_running{false};

        /** 当前仿真时间缓存。 */
        double m_simTime{0.0};

        /** 当前步数缓存。 */
        quint64 m_stepCount{0};

        /** 当前实体数量缓存。 */
        int m_entityCount{0};

        /** 当前帧率缓存。 */
        double m_fps{0.0};

        /** 固定步长，单位秒。 */
        double m_fixedStepSeconds{1.0 / 120.0};

        /** 每帧最多补偿的固定步数，避免“死亡螺旋”。 */
        int m_maxSubSteps{5};

        /** 固定步长累加器。 */
        double m_accumulator{0.0};

        /** 帧计时器，用于驱动实时步进。 */
        QTimer m_timer;

        /** 统计真实帧时间。 */
        QElapsedTimer m_frameClock;

        /** QML 可直接使用的实体模型。 */
        SceneBodyModel m_bodyModel;

        /** 模型层世界管理器。 */
        std::unique_ptr<WorldSceneManager> m_world;
    };
}

#endif // EGRET_PHYSICS_SCENE_MANAGER_VIEWMODEL_H
