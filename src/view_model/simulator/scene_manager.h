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

    public:
        /** 默认构造。 */
        explicit SceneManagerViewModel(QObject* parent = nullptr);

        /** 默认析构。 */
        ~SceneManagerViewModel() override = default;

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
        double m_fixedStepSeconds{1.0 / 120.0};

        /**
         * @brief 时间缩放因子。
         * @details 1.0 = 实时，>1.0 = 加速，<1.0 = 慢动作。
         *          物理积分始终以 m_fixedStepSeconds 求解，缩放仅影响累加器。
         */
        double m_timeScale{1.0};

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
