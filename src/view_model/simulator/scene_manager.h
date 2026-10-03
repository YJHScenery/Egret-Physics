/**
 * @file        scene_manager.h
 * @brief       场景管理器视图模型头文件，提供场景交互的 QML 接口。
 * @details     定义 SceneManagerViewModel 类，作为 UI 与模型层之间的交互入口，
 *              使用 QTimer 驱动物理模拟，并处理固定步长累积。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-04
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_SCENE_MANAGER_VIEWMODEL_H
#define EGRET_PHYSICS_SCENE_MANAGER_VIEWMODEL_H

#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QVector3D>

#include <Eigen/Dense>

#include <memory>

#include "sleep_config.h"
#include "visual_item/scene_body_model.h"
#include "visual_item/scene_constraint_model.h"
#include "visual_item/scene_convex_mesh_model.h"
#include "visual_item/scene_curve_model.h"
#include "world_scene_manager.h"
#include <QQuaternion>

class QFile;

namespace egret
{
    class WorldSceneManager;

    namespace epsx
    {
        class EpsxReader;
    }

    struct FieldRecord;

    struct ConstraintRecord;

    struct BodyRecord;

    struct SceneRecord;

    // ========================================================================
    //  追踪数据结构
    // ========================================================================

    /** 每个仿真步的追踪快照。 */
    struct TrackingSnapshot
    {
        double simTime{0.0};
        std::array<double, 3> position{0.0, 0.0, 0.0};
        std::array<double, 3> velocity{0.0, 0.0, 0.0};
        std::array<double, 3> angularVelocity{0.0, 0.0, 0.0};
        double kineticEnergy{0.0};
    };

    /**
     * @brief 单个实体的追踪缓冲区。
     * @details 在 refreshBodyModel() 时自动追加快照，超限时丢弃旧数据。
     */
    struct TrackingBuffer
    {
        bool active{false};

        /** 暂停采集：为 true 时 refreshBodyModel 跳过追加快照。 */
        bool paused{false};

        /** 位标志：bit0=位置, bit1=速度, bit2=角速度, bit3=动能 */
        std::uint32_t flags{0};

        std::deque<TrackingSnapshot> snapshots;

        /** 导出互斥锁：0=空闲，1=正在导出。防止并发写同一文件。 */
        QAtomicInt exporting{0};

    };

    /**
     * @brief       Qt / ViewModel 层的场景门面对象。
     * @details     SceneManagerViewModel 是 Qt / ViewModel 层的场景门面对象。
     *              该对象直接暴露给 QML，负责：
     *              1. 作为 UI 与模型层之间的唯一交互入口。
     *              2. 使用 QTimer 驱动模型层 tick，并处理固定步长累积。
     *              3. 将模型层输出的轻量化渲染快照转换为 QML 可绑定模型。
     *              采用门面模式，封装模型层的复杂性。
     *              支持 QML 属性绑定和信号通知。
     *
     * @invariant   running 属性表示仿真是否运行
     * @invariant   simTime 属性表示当前仿真时间
     * @invariant   stepCount 属性表示当前步数
     * @remark      SceneManagerViewModel 是 QObject，支持 QML 属性绑定
     * @see         WorldSceneManager, SceneBodyModel, QTimer
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

        Q_PROPERTY(double timeScale READ timeScale NOTIFY timeScaleChanged)

        Q_PROPERTY(SceneBodyModel *bodyModel READ bodyModel CONSTANT)

        Q_PROPERTY(SceneConstraintModel *constraintModel READ constraintModel CONSTANT)

        Q_PROPERTY(SceneCurveModel *curveModel READ curveModel CONSTANT)

        Q_PROPERTY(SceneConvexMeshModel *convexMeshModel READ convexMeshModel CONSTANT)

        Q_PROPERTY(bool simulationLoaded READ isSimulationLoaded NOTIFY simulationChanged)

        Q_PROPERTY(QString simulationFile READ simulationFile NOTIFY simulationChanged)

        Q_PROPERTY(QString simulationError READ simulationError NOTIFY simulationChanged)

        Q_PROPERTY(int simulationFrameCount READ simulationFrameCount NOTIFY simulationChanged)

        Q_PROPERTY(double simulationDurationSeconds READ simulationDurationSeconds NOTIFY simulationChanged)

        Q_PROPERTY(double simulationSampleIntervalMs READ simulationSampleIntervalMs NOTIFY simulationChanged)

        Q_PROPERTY(int playbackFrame READ playbackFrame NOTIFY playbackStateChanged)

        Q_PROPERTY(double playbackTimeSeconds READ playbackTimeSeconds NOTIFY playbackStateChanged)

        Q_PROPERTY(bool playbackPlaying READ isPlaybackPlaying NOTIFY playbackStateChanged)

        Q_PROPERTY(double playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)

    public:
        /**
         * @brief 默认构造。
         * @param parent Qt 父对象。
         * @param buildDemoScene 是否在构造时建立内置示例场景。离线仿真工作区传入
         *        false：它需要一个「空世界」起点，未加载场景时只渲染坐标系与网格，
         *        也避免再依赖 `../tools/icosahedron.obj` 等外部资源。
         */
        explicit SceneManagerViewModel(QObject* parent = nullptr, bool buildDemoScene = true);

        /**
         * @brief 默认析构。
         * @details 定义放在 .cpp 中：本类持有 epsx::EpsxReader 的 unique_ptr，
         *          此处只有前置声明，需要完整类型才能实例化删除器。
         */
        ~SceneManagerViewModel() override;

        // ------------------------------------------------------------------
        //  离线仿真文件播放（与实时求解互斥：只要加载了文件就进入播放模式）
        // ------------------------------------------------------------------

        /** 是否已加载仿真文件。 */
        [[nodiscard]] bool isSimulationLoaded() const;

        /** 当前打开的仿真文件路径。 */
        [[nodiscard]] QString simulationFile() const;

        /** 最近一次加载失败的原因；无错误时为空串。 */
        [[nodiscard]] QString simulationError() const;

        /** 仿真文件的总帧数。 */
        [[nodiscard]] int simulationFrameCount() const;

        /** 仿真的总时长（秒）。 */
        [[nodiscard]] double simulationDurationSeconds() const;

        /** 文件采样间隔（毫秒）。 */
        [[nodiscard]] double simulationSampleIntervalMs() const;

        /** 当前播放到的帧号。 */
        [[nodiscard]] int playbackFrame() const;

        /** 当前播放时间（秒）。 */
        [[nodiscard]] double playbackTimeSeconds() const;

        /** 是否正在播放。 */
        [[nodiscard]] bool isPlaybackPlaying() const;

        /** 播放倍速。 */
        [[nodiscard]] double playbackRate() const;

        /** 设置播放倍速。 */
        void setPlaybackRate(double rate);

        /**
         * @brief 打开仿真文件并重建场景。
         * @param fileName 文件路径。
         * @return 成功返回 true；失败时可通过 simulationError 读取原因。
         */
        Q_INVOKABLE bool openSimulationFile(const QString& fileName);

        /** 打开默认缓存位置的仿真文件。 */
        Q_INVOKABLE bool openDefaultSimulationFile();

        /** 关闭当前仿真文件并回到实时求解模式。 */
        Q_INVOKABLE void closeSimulationFile();

        /** 跳到指定帧。 */
        Q_INVOKABLE void setPlaybackFrame(int frame);

        /** 跳到指定播放时间（秒）。 */
        Q_INVOKABLE void setPlaybackTime(double seconds);

        /** 相对当前帧前后跳若干帧。 */
        Q_INVOKABLE void stepPlaybackFrame(int delta);

        /** 开始播放。 */
        Q_INVOKABLE void playSimulation();

        /** 暂停播放。 */
        Q_INVOKABLE void pauseSimulation();

        /** 在播放与暂停之间切换。 */
        Q_INVOKABLE void toggleSimulationPlayback();

        // 从 json 创建 PhysicalEntity 等实例以供仿真器使用
        static SceneRecord createSceneFromJsonString(const QString& jsonString);

        static SceneRecord createSceneFromJsonFile(const QString& fileName);

        /** 初始化 solver，配置策略器 */
        void initialize();

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

        /** 时间缩放因子（1.0=实时，3.0=3倍速，10.0=10倍速）。 */
        [[nodiscard]] double timeScale() const;

        /**
         * @brief 设置积分步长。
         * @param ms 步长，单位毫秒（必须 > 0）。
         */
        Q_INVOKABLE void setFixedStepMs(double ms);

        /**
         * @brief 设置时间缩放因子。
         * @param scale 缩放因子，1.0 为实时，3.0 为 3 倍速，0.1 为 1/10 慢动作。
         * @details 物理积分始终以 fixedStepMs 为步长求解，缩放仅影响累加器的推进速率。
         */
        Q_INVOKABLE void setTimeScale(double scale);

        /**
         * @brief 设置指定实体的追踪属性。
         * @param bodyId  实体 ID 字符串。
         * @param trackPosition   是否追踪位置。
         * @param trackVelocity   是否追踪速度。
         * @param trackAngularVelocity  是否追踪角速度。
         * @param trackKineticEnergy  是否追踪动能。
         * @details 至少一项为 true 时激活追踪；全部为 false 时停用。
         */
        Q_INVOKABLE void setBodyTracking(const QString &bodyId,
                                         bool trackPosition,
                                         bool trackVelocity,
                                         bool trackAngularVelocity,
                                         bool trackKineticEnergy);

        /** 获取指定实体的快照数量。 */
        Q_INVOKABLE [[nodiscard]] int getSnapshotCount(const QString &bodyId) const;

        /**
         * @brief 将指定实体的追踪数据导出为 Excel (.xlsx) 文件。
         * @param bodyId  实体 ID 字符串。
         * @param filePath  导出文件路径。
         * @return 成功返回 true。
         */
        Q_INVOKABLE [[nodiscard]] bool saveTrackingXlsx(const QString &bodyId,
                                          const QString &filePath) const;

        /**
         * @brief 确保指定实体的追踪已激活（幂等）。
         * @details 若缓冲区未激活，自动开启全部 4 项追踪属性。
         *          已激活则保持现有 flags 不变。
         */
        Q_INVOKABLE void ensureBodyTracking(const QString &bodyId);

        /**
         * @brief 异步导出追踪数据到 xlsx（线程池）。
         * @details 拷贝当前快照后通过 QtConcurrent::run 写入文件。
         *          快照数为 0 时也创建仅含表头的文件。
         */
        Q_INVOKABLE void exportTrackingAsync(const QString &bodyId,
                                             const QString &filePath);

        /**
         * @brief 开始录制：激活追踪并每 2 秒自动刷新 xlsx。
         * @details 调用后数据持续积累到同一文件，无需再次手动导出。
         *          同一 body 重复调用会更新路径。
         */
        Q_INVOKABLE void startRecording(const QString &bodyId,
                                        const QString &filePath);

        /** 暂停采集（文件仍会定时刷新，但新快照不再追加）。 */
        Q_INVOKABLE void pauseRecording(const QString &bodyId);

        /** 恢复采集。 */
        Q_INVOKABLE void resumeRecording(const QString &bodyId);

        /**
         * @brief 停止录制：最终刷新 → 清空快照 → 关闭文件。
         * @details 调用后缓冲区被清空，追踪停用，保存按钮恢复可用。
         */
        Q_INVOKABLE void stopRecording(const QString &bodyId);

        /** QML 直接使用的实体列表模型。 */
        [[nodiscard]] SceneBodyModel* bodyModel();

        /** QML 直接使用的约束列表模型。 */
        [[nodiscard]] SceneConstraintModel* constraintModel();


        /** QML 直接使用的曲线列表模型。 */
        [[nodiscard]] SceneCurveModel* curveModel();

        /** QML 直接使用的凸体网格列表模型。 */
        [[nodiscard]] SceneConvexMeshModel* convexMeshModel();

        /** 获取当前的积分策略器名称 */
        [[nodiscard]] Q_INVOKABLE QString getIntegratorName() const;

        /** 获取广相位碰撞检测策略器名称 */
        [[nodiscard]] Q_INVOKABLE QString getBroadStrategyName() const;

        /** 获取当前休眠策略器名称（未启用时返回空字符串）。 */
        [[nodiscard]] Q_INVOKABLE QString getSleepStrategyName() const;

        /**
         * @brief 启用或禁用休眠策略。
         * @param enabled true 表示启用休眠机制。
         */
        Q_INVOKABLE void setSleepEnabled(bool enabled);

        /**
         * @brief 设置解冻受力比例。
         * @param ratio 解冻比例，例如 0.01 表示受力变化超过基线 1% 时解冻。
         */
        Q_INVOKABLE void setWakeUpForceRatio(double ratio);

        /**
         * @brief 设置冻结指标类型。
         * @param type 指标类型：0=速度平方，1=动能，2=动量。
         */
        Q_INVOKABLE void setSleepMetricType(int type);

        /**
         * @brief 设置冻结阈值。
         * @param threshold 冻结阈值，单位取决于当前指标类型。
         */
        Q_INVOKABLE void setSleepThreshold(double threshold);

        /**
         * @brief 设置连续冻结所需渲染帧数。
         * @param frameCount 连续通过的渲染帧数阈值。
         */
        Q_INVOKABLE void setSleepFrozenFrameCount(int frameCount);

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

        /** 从 JSON 文件中加载新的场景 */
        Q_INVOKABLE void loadSceneFronJsonFile(const QString& fileName);

        Q_INVOKABLE void loadSceneFromJsonString(const QString& jsonString);

        /**
         * @brief 返回最近一次成功加载的场景 JSON 文本。
         * @details 供离线求解器复用同一份场景数据；从未加载过场景时返回空串。
         */
        Q_INVOKABLE [[nodiscard]] QString currentSceneJson() const;

        // /**
        //  * @brief 将屏幕坐标映射到世界 XY 平面（z 固定）上的点。
        //  * @param screenX 屏幕像素 X。
        //  * @param screenY 屏幕像素 Y。
        //  * @param viewportWidth 视口宽度。
        //  * @param viewportHeight 视口高度。
        //  * @param cameraState 相机参数字典。
        //  * @param planeZ 目标平面 z 值。
        //  * @return 包含 ok/x/y/z/error 的映射结果。
        //  */
        // [[deprecated]] [[nodiscard]] Q_INVOKABLE  static QVariantMap mapScreenToWorldOnPlane(double screenX,
        //                                                                      double screenY,
        //                                                                      double viewportWidth,
        //                                                                      double viewportHeight,
        //                                                                      const QVariantMap& cameraState,
        //                                                                      double planeZ);

        [[nodiscard]] Q_INVOKABLE static QVector3D buildRenderPosition(const QVector3D& position);

        [[nodiscard]] Q_INVOKABLE static QQuaternion buildRenderRotation(const QQuaternion& rotation);

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

        /** 时间缩放因子变化。 */
        void timeScaleChanged();

        /** 仿真文件或元信息变化。 */
        void simulationChanged();

        /** 播放进度或播放状态变化。 */
        void playbackStateChanged();

        /** 播放倍速变化。 */
        void playbackRateChanged();

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

        void build3BodyModel() const;

        /**
         * @brief 将模型层世界快照刷新到 Qt 列表模型。
         */
        void refreshBodyModel();


        /**
         * @brief 将模型层约束快照刷新到 Qt 列表模型。
         */
        void refreshConstraintModel();

        /**
         * @brief 将模型层曲线快照刷新到 Qt 列表模型。
         */
        void refreshCurveModel();

        /**
         * @brief 将模型层凸体网格快照刷新到 Qt 列表模型。
         */
        void refreshConvexMeshModel();

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

        /** 从相机状态构建拾取射线。 */
        [[nodiscard]] static bool buildPickRay(double screenX,
                                        double screenY,
                                        double viewportWidth,
                                        double viewportHeight,
                                        const QVariantMap& cameraState,
                                        Eigen::Vector3d* origin,
                                        Eigen::Vector3d* direction,
                                        QString* error = nullptr) ;

        /** 射线与 z=planeZ 平面的交点。 */
        [[nodiscard]] static bool intersectRayWithPlaneZ(const Eigen::Vector3d& rayOrigin,
                                                         const Eigen::Vector3d& rayDirection,
                                                         double planeZ,
                                                         Eigen::Vector3d* hitPoint,
                                                         QString* error = nullptr);

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
        double m_fixedStepSeconds{1.0 / 1000.0};

        /** 休眠策略的运行时配置（ViewModel 层持有，用于 QML 参数同步）。 */
        SleepConfig m_sleepConfig{};

        /**
         * @brief 时间缩放因子。
         * @details 1.0 = 实时，>1.0 = 加速，<1.0 = 慢动作。
         *          物理积分始终以 m_fixedStepSeconds 求解，缩放仅影响累加器。
         */
        double m_timeScale{1.0};

        /**
         * @brief 每个定时器周期最多补偿的固定步数，避免“死亡螺旋”。
         * @details 本值决定仿真时间能否跟上真实时间，关系为
         *          每秒可推进的仿真时间 = (1000 / 定时器周期 ms) × m_maxSubSteps × m_fixedStepSeconds。
         *          定时器周期 16 ms、步长 1 ms 时，至少需要 16 步才能达到 1 倍实时；
         *          取 32 即 2 倍余量，卡顿之后仍能追回落后的仿真时间。
         * @remark 修改 m_fixedStepSeconds 或 m_timer 的周期后，必须同步复核本值。
         */
        int m_maxSubSteps{32};

        /** 固定步长累加器。 */
        double m_accumulator{0.0};

        /**
         * @brief 播放推进：按真实时间推进播放时钟并切换到对应帧。
         * @param realFrameSeconds 本渲染帧的真实耗时（秒）。
         */
        void advancePlayback(double realFrameSeconds);

        /**
         * @brief  按需启动 / 停止 16 ms 帧定时器。
         * @details 实时求解与离线播放共用同一个定时器：只要「实时仿真在跑」或
         *          「播放处于播放中」，定时器就必须运行；两者都停止时立即停表。
         *          启动定时器时同步重置帧计时时钟，避免把等待时间算进第一帧。
         */
        void syncFrameTimer();

        /**
         * @brief 把指定帧的数据套用到世界的实体上，并刷新 Qt 模型。
         * @param frame 帧号。
         * @return 成功返回 true。
         */
        bool applySimulationFrame(int frame);

        /** 离线仿真文件读取器；为空表示当前处于实时求解模式。 */
        std::unique_ptr<epsx::EpsxReader> m_simulationReader{};

        /** 当前打开的仿真文件路径。 */
        QString m_simulationFile{};

        /** 最近一次加载失败的原因。 */
        QString m_simulationError{};

        /** 最近一次成功加载的场景 JSON 文本（供离线求解复用）。 */
        QString m_currentSceneJson{};

        /** 当前播放帧号。 */
        int m_playbackFrame{0};

        /** 当前播放时间（秒）。 */
        double m_playbackTime{0.0};

        /** 播放倍速。 */
        double m_playbackRate{1.0};

        /** 是否正在播放。 */
        bool m_playbackPlaying{false};

        /** 帧计时器，用于驱动实时步进。 */
        QTimer m_timer;

        /** 统计真实帧时间。 */
        QElapsedTimer m_frameClock;

        /** QML 可直接使用的实体模型。 */
        SceneBodyModel m_bodyModel;

        /** QML 可直接使用的约束模型。 */
        SceneConstraintModel m_constraintModel;

        /** QML 可直接使用的曲线模型。 */
        SceneCurveModel m_curveModel;

        /** QML 可直接使用的凸体网格模型。 */
        SceneConvexMeshModel m_convexMeshModel;

        /** 追踪数据缓冲区，按实体 ID 索引。 */
        QMap<std::uint64_t, TrackingBuffer> m_trackingBuffers;

        /** 录制中的导出路径，按实体 ID 索引。有路径 = 正在录制。 */
        QMap<std::uint64_t, QString> m_recordingPaths;

        /** 录制中保持打开的文件句柄（锁定文件，禁止外部写入）。 */
        QMap<std::uint64_t, std::shared_ptr<QFile>> m_recordingFiles;

        /** 定时刷新录制文件（每 2 秒触发一次）。 */
        QTimer m_flushTimer;

        /** 将所有录制中的缓冲区导出到各自文件。 */
        void flushRecordings();

        /** 模型层世界管理器。 */
        std::unique_ptr<WorldSceneManager> m_world;
    };
}

#endif // EGRET_PHYSICS_SCENE_MANAGER_VIEWMODEL_H
