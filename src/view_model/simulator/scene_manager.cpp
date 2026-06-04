/**
 * @file        scene_manager.cpp
 * @brief       场景管理器视图模型实现文件，提供场景交互的 QML 接口。
 * @details     实现 SceneManagerViewModel 类的各项成员函数。
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
#if _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#include "scene_manager.h"
#include <QCryptographicHash>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_set>
#include <QFile>

#include <QMatrix4x4>
#include <QVector3D>

#include "world_scene_manager.h"
#include "broad_phase_strategy/brute_force_broad_phase.h"
#include "broad_phase_strategy/multi_process_brute_force_broad_phase.h"
#include "contact_strategy/frictionless_contact_resolver.h"
#include "integrator_strategy/semi_implicit_euler_integrator.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "air_resistance.h"
#include "shape_box.h"
#include "shape_sphere.h"
#include "shape_cylinder.h"
#include "shape_factory_registry.h"
#include "rigid_body.h"
#include "gravitational_field.h"
#include "async_logger.h"
#include "model_item_data.h"
#include "transform_transformer.h"
#include "magic_enum.hpp"
#include "surface_plane.h"
#include "support_surface.h"
#include "constraints/connecting_rod.h"
#include "surface_bezier.h"
#include "surface_cylindrical.h"
#include "curve_bezier.h"
#include "curve_helix.h"
#include "curve_conic_section.h"
#include "constants.h"

#include "unique_id_generator.h"
#include "contact_strategy/frictional_contact_resolver.h"
#include "integrator_strategy/velocity_verlet_integrator.h"


namespace egret
{
    namespace
    {
        struct CameraState
        {
            QVector3D position{0.0f, -800.0f, 400.0f};
            QVector3D target{0.0f, 0.0f, 0.0f};
            QVector3D up{0.0f, 0.0f, 1.0f};
            double fovY{45.0};
            double nearClip{1.0};
            double farClip{50000.0};
        };

        [[nodiscard]] double readCameraScalar(const QVariantMap& cameraState,
                                              const QString& key,
                                              const double fallback)
        {
            const QVariant value = cameraState.value(key);
            bool ok = false;
            const double converted = value.toDouble(&ok);
            return ok ? converted : fallback;
        }

        [[nodiscard]] CameraState parseCameraState(const QVariantMap& cameraState)
        {
            CameraState parsed{};
            parsed.position.setX(static_cast<float>(readCameraScalar(cameraState, "positionX", parsed.position.x())));
            parsed.position.setY(static_cast<float>(readCameraScalar(cameraState, "positionY", parsed.position.y())));
            parsed.position.setZ(static_cast<float>(readCameraScalar(cameraState, "positionZ", parsed.position.z())));

            parsed.target.setX(static_cast<float>(readCameraScalar(cameraState, "targetX", parsed.target.x())));
            parsed.target.setY(static_cast<float>(readCameraScalar(cameraState, "targetY", parsed.target.y())));
            parsed.target.setZ(static_cast<float>(readCameraScalar(cameraState, "targetZ", parsed.target.z())));

            parsed.up.setX(static_cast<float>(readCameraScalar(cameraState, "upX", parsed.up.x())));
            parsed.up.setY(static_cast<float>(readCameraScalar(cameraState, "upY", parsed.up.y())));
            parsed.up.setZ(static_cast<float>(readCameraScalar(cameraState, "upZ", parsed.up.z())));

            parsed.fovY = readCameraScalar(cameraState, "fovY", parsed.fovY);
            parsed.nearClip = readCameraScalar(cameraState, "nearClip", parsed.nearClip);
            parsed.farClip = readCameraScalar(cameraState, "farClip", parsed.farClip);
            return parsed;
        }
    }

    SceneManagerViewModel::SceneManagerViewModel(QObject* parent) : QObject(parent), m_bodyModel(this)
    {
        m_timer.setInterval(16);
        m_timer.setTimerType(Qt::PreciseTimer);
        connect(&m_timer, &QTimer::timeout, this, &SceneManagerViewModel::onFrameTick);

        initialize();
        rebuildDemoWorld();
        refreshBodyModel();
        refreshConstraintModel();
        refreshSurfaceModel();
        refreshCurveModel();
        m_frameClock.start();
    }

    SceneRecord SceneManagerViewModel::createSceneFromJsonString(const QString& jsonString)
    {
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return {};
        }

        const QJsonObject root = doc.object();
        if (!root.contains("models") || !root.value("models").isArray()) {
            return {};
        }

        const QJsonArray modelsArray = root.value("models").toArray();
        SceneRecord record{};
        record.bodies.reserve(modelsArray.size());

        std::unordered_set<std::uint64_t> usedIds;
        std::uint64_t fallbackId = 1;

        const auto readVector3 = [](const QJsonValue& value, const Eigen::Vector3d& fallback)
        {
            if (value.isArray()) {
                const QJsonArray arr = value.toArray();
                if (arr.size() >= 3) {
                    return Eigen::Vector3d(arr[0].toDouble(), arr[1].toDouble(), arr[2].toDouble());
                }
            }
            if (value.isObject()) {
                const QJsonObject obj = value.toObject();
                if (obj.contains("x") && obj.contains("y") && obj.contains("z")) {
                    return Eigen::Vector3d(obj.value("x").toDouble(), obj.value("y").toDouble(),
                                           obj.value("z").toDouble());
                }
            }
            return fallback;
        };

        const auto readQuaternion = [](const QJsonValue& value, const Eigen::Quaterniond& fallback)
        {
            if (!value.isArray()) {
                return fallback;
            }
            const QJsonArray arr = value.toArray();
            if (arr.size() < 4) {
                return fallback;
            }
            return Eigen::Quaterniond(arr[0].toDouble(), arr[1].toDouble(), arr[2].toDouble(), arr[3].toDouble());
        };

        const auto hashIdString = [](const QString& value)
        {
            const QByteArray hash = QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha1);
            std::uint64_t id = 0;
            for (int i = 0; i < 8 && i < hash.size(); ++i) {
                id = (id << 8) | static_cast<unsigned char>(hash[i]);
            }
            return id;
        };

        for (const QJsonValue& entry : modelsArray) {
            if (!entry.isObject()) {
                continue;
            }

            const QJsonObject obj = entry.toObject();

            const QString name = obj.value("name").toString("Unnamed");
            const double mass = obj.value("mass").toDouble();
            const double restitution = obj.contains("restitution") ? obj.value("restitution").toDouble() : 1.0;

            const std::uint32_t type = static_cast<std::uint32_t>(obj.value("type").toInteger());

            const Eigen::Vector3d position = readVector3(obj.value("pos"), Eigen::Vector3d::Zero());
            const Eigen::Vector3d scale = readVector3(obj.value("scale"), Eigen::Vector3d(1.0, 1.0, 1.0));
            const Eigen::Quaterniond rotation = readQuaternion(obj.value("rotation"), Eigen::Quaterniond::Identity());

            const Eigen::Vector3d initialVelocity = readVector3(obj.value("initial_velocity"), Eigen::Vector3d::Zero());
            const Eigen::Vector3d initialAngularVelocity = readVector3(obj.value("initial_angular_velocity"),
                                                                       Eigen::Vector3d::Zero());

            const bool hasBoxSize = obj.contains("box_size");
            const bool hasRadius = obj.contains("radius") || obj.contains("circle_radius");
            const bool hasHeight = obj.contains("height");
            const bool hasLength = obj.contains("length");

            ShapeLoadInfo shapeInfo{};

            if (magic_enum::enum_contains<ShapeType>(type) || type != static_cast<std::uint32_t>(ShapeType::Unknown)) {
                shapeInfo.typeId = type;
            }
            else {
                shapeInfo.typeId = static_cast<std::uint32_t>(ShapeType::Unknown);
                LOG_WARN_LITERAL("Unknown Shape ID");
            }

            const auto readRadius = [&]()
            {
                return obj.contains("radius")
                           ? obj.value("radius").toDouble()
                           : obj.value("circle_radius").toDouble();
            };

            switch (static_cast<ShapeType>(type)) {
            case ShapeType::Box: {
                if (!hasBoxSize) {
                    LOG_WARN_LITERAL("Missing box_size for Box");
                    continue;
                }
                const Eigen::Vector3d size = readVector3(obj.value("box_size"), Eigen::Vector3d(1.0, 1.0, 1.0));
                shapeInfo.numberParams["size"] = {size.x(), size.y(), size.z()};
                break;
            }
            case ShapeType::Cylinder:
            case ShapeType::CylindricalShell: {
                if (!hasRadius || !hasHeight) {
                    LOG_WARN_LITERAL("Missing radius/height for Cylinder");
                    continue;
                }
                const double radius = readRadius();
                const double height = obj.value("height").toDouble();
                shapeInfo.numberParams["radius"] = {radius};
                shapeInfo.numberParams["height"] = {height};
                break;
            }
            case ShapeType::Disk:
            case ShapeType::Ring:
            case ShapeType::Sphere:
            case ShapeType::SphericalShell: {
                if (!hasRadius) {
                    LOG_WARN_LITERAL("Missing radius for round shape");
                    continue;
                }
                const double radius = readRadius();
                shapeInfo.numberParams["radius"] = {radius};
                break;
            }
            case ShapeType::Rod: {
                if (!hasLength) {
                    LOG_WARN_LITERAL("Missing length for Rod");
                    continue;
                }
                const double length = obj.value("length").toDouble();
                shapeInfo.numberParams["length"] = {length};
                break;
            }
            default:
                LOG_WARN_LITERAL("Unknown Shape ID");
                continue;
            }

            std::unique_ptr<ShapeBase> shape = ShapeFactoryRegistry::instance().create(shapeInfo);
            if (shape == nullptr) {
                LOG_WARN_LITERAL("Shape Created Failed");
                continue;
            }


            auto entity = std::make_shared<RigidBody>(position, initialVelocity, mass,
                                                      UniqueIDGenerator::instance().generate());
            entity->setRotation(rotation);
            entity->setScale(scale);

            // qDebug() << __func__ << scale.x() << scale.y() << scale.z();
            entity->setRestitution(restitution);
            entity->setAngular(initialAngularVelocity);
            entity->setShape(std::shared_ptr<ShapeBase>(std::move(shape)));

            BodyRecord body{};
            const QString idText = obj.value("id").toString();
            std::uint64_t bodyId = idText.isEmpty() ? 0 : hashIdString(idText);
            if (bodyId == 0 || usedIds.contains(bodyId)) {
                while (usedIds.contains(fallbackId) || fallbackId == 0) {
                    ++fallbackId;
                }
                bodyId = fallbackId++;
            }
            usedIds.insert(bodyId);

            body.name = name.toStdString();
            body.entity = std::move(entity);
            record.bodies.push_back(std::move(body));
        }

        return record;
    }

    SceneRecord SceneManagerViewModel::createSceneFromJsonFile(const QString& fileName)
    {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        QByteArray data = file.readAll();
        file.close();
        return createSceneFromJsonString(data);
    }

    void SceneManagerViewModel::initialize()
    {
        SolverConfig config{};

        auto solver = std::make_unique<Solver>(
            config,
            std::make_unique<VelocityVerletIntegratorStrategy>(),
            std::make_unique<MultiProcessBruteForceBroadPhaseStrategy>(),
            std::make_unique<FrictionalContactResolverStrategy>());

        m_world = std::make_unique<WorldSceneManager>(std::move(solver));
        m_world->clear();
    }

    bool SceneManagerViewModel::isRunning() const
    {
        return m_running;
    }

    double SceneManagerViewModel::simTime() const
    {
        return m_simTime;
    }

    quint64 SceneManagerViewModel::stepCount() const
    {
        return m_stepCount;
    }

    int SceneManagerViewModel::entityCount() const
    {
        return m_entityCount;
    }

    double SceneManagerViewModel::fps() const
    {
        return m_fps;
    }

    double SceneManagerViewModel::fixedStepMs() const
    {
        return m_fixedStepSeconds * 1000.0;
    }

    double SceneManagerViewModel::timeScale() const
    {
        return m_timeScale;
    }

    void SceneManagerViewModel::setFixedStepMs(const double ms)
    {
        if (ms <= 0.0 || ms > 1000.0) {
            qWarning() << "setFixedStepMs: 无效步长" << ms << "ms，必须在 (0, 1000] 范围内";
            return;
        }
        m_fixedStepSeconds = ms / 1000.0;
        m_accumulator = 0.0;
        emit fixedStepMsChanged();
    }

    void SceneManagerViewModel::setTimeScale(const double scale)
    {
        if (scale <= 0.0 || scale > 100.0) {
            qWarning() << "setTimeScale: 无效缩放因子" << scale << "，必须在 (0, 100] 范围内";
            return;
        }
        m_timeScale = scale;
        m_accumulator = 0.0;
        emit timeScaleChanged();
    }

    bool SceneManagerViewModel::isDragActive() const
    {
        return m_dragActive;
    }

    SceneBodyModel* SceneManagerViewModel::bodyModel()
    {
        return &m_bodyModel;
    }

    SceneConstraintModel* SceneManagerViewModel::constraintModel()
    {
        return &m_constraintModel;
    }

    SceneSurfaceModel* SceneManagerViewModel::surfaceModel()
    {
        return &m_surfaceModel;
    }

    SceneCurveModel* SceneManagerViewModel::curveModel()
    {
        return &m_curveModel;
    }

    QString SceneManagerViewModel::getIntegratorName() const
    {
        const std::string_view name{m_world->getIntegratorName()};
        return QString::fromUtf8(name.data(), name.size());
    }

    void SceneManagerViewModel::play()
    {
        if (m_running) {
            return;
        }

        setRunning(true);
        m_frameClock.restart();
        m_timer.start();
    }

    void SceneManagerViewModel::pause()
    {
        if (!m_running) {
            return;
        }

        m_timer.stop();
        setRunning(false);
    }

    void SceneManagerViewModel::toggleRunning()
    {
        if (m_running) {
            pause();
        }
        else {
            play();
        }
    }

    void SceneManagerViewModel::stepOnce()
    {
        advanceFixedStep(true);
    }

    void SceneManagerViewModel::reset()
    {
        pause();
        endBodyDrag();
        initialize();
        rebuildDemoWorld();
        refreshBodyModel();
        refreshConstraintModel();
        refreshSurfaceModel();
        refreshCurveModel();

        m_simTime = 0.0;
        m_stepCount = 0;
        m_accumulator = 0.0;
        m_timeScale = 1.0;
        m_entityCount = m_world != nullptr ? static_cast<int>(m_world->getBodyCount()) : 0;

        emit timeScaleChanged();

        emit simTimeChanged();
        emit stepCountChanged();
        emit entityCountChanged();
        emit sceneReset();
        emit entityChanged();
    }

    void SceneManagerViewModel::loadSceneFronJsonFile(const QString& fileName)
    {
    }

    void SceneManagerViewModel::loadSceneFromJsonString(const QString& jsonString)
    {
    }

    QVariantMap SceneManagerViewModel::mapScreenToWorldOnPlane(const double screenX,
                                                               const double screenY,
                                                               const double viewportWidth,
                                                               const double viewportHeight,
                                                               const QVariantMap& cameraState,
                                                               const double planeZ)
    {
        QVariantMap result{};
        Eigen::Vector3d rayOrigin{};
        Eigen::Vector3d rayDirection{};
        QString error{};
        if (!buildPickRay(screenX,
                          screenY,
                          viewportWidth,
                          viewportHeight,
                          cameraState,
                          &rayOrigin,
                          &rayDirection,
                          &error)) {
            result.insert("ok", false);
            result.insert("error", error);
            return result;
        }

        Eigen::Vector3d hitPoint{};
        if (!intersectRayWithPlaneZ(rayOrigin, rayDirection, planeZ, &hitPoint, &error)) {
            result.insert("ok", false);
            result.insert("error", error);
            return result;
        }

        result.insert("ok", true);
        result.insert("x", hitPoint.x());
        result.insert("y", hitPoint.y());
        result.insert("z", hitPoint.z());
        return result;
    }

    bool SceneManagerViewModel::beginBodyDrag(const QString& bodyId,
                                              const double screenX,
                                              const double screenY,
                                              const double viewportWidth,
                                              const double viewportHeight,
                                              const QVariantMap& cameraState,
                                              const QString& mode)
    {
        if (m_world == nullptr) {
            return false;
        }

        const quint64 id = bodyId.toULongLong();

        const std::optional<Eigen::Vector3d> bodyPosition = m_world->getBodyPosition(id);
        if (!bodyPosition.has_value()) {
            return false;
        }

        Eigen::Vector3d rayOrigin{};
        Eigen::Vector3d rayDirection{};
        if (!buildPickRay(screenX,
                          screenY,
                          viewportWidth,
                          viewportHeight,
                          cameraState,
                          &rayOrigin,
                          &rayDirection)) {
            return false;
        }

        Eigen::Vector3d planePoint{};
        if (!intersectRayWithPlaneZ(rayOrigin, rayDirection, bodyPosition->z(), &planePoint)) {
            return false;
        }

        m_dragBodyId = id;
        m_dragMode = mode;
        m_dragStartBodyPosition = *bodyPosition;
        m_dragStartPlanePoint = planePoint;
        setDragActive(true);
        return true;
    }

    bool SceneManagerViewModel::updateBodyDrag(const double screenX,
                                               const double screenY,
                                               const double viewportWidth,
                                               const double viewportHeight,
                                               const QVariantMap& cameraState)
    {
        if (!m_dragActive || m_world == nullptr || m_dragBodyId == 0) {
            return false;
        }

        Eigen::Vector3d rayOrigin{};
        Eigen::Vector3d rayDirection{};
        if (!buildPickRay(screenX,
                          screenY,
                          viewportWidth,
                          viewportHeight,
                          cameraState,
                          &rayOrigin,
                          &rayDirection)) {
            return false;
        }

        Eigen::Vector3d currentPlanePoint{};
        if (!intersectRayWithPlaneZ(rayOrigin, rayDirection, m_dragStartBodyPosition.z(), &currentPlanePoint)) {
            return false;
        }

        const Eigen::Vector3d dragDelta = currentPlanePoint - m_dragStartPlanePoint;
        Eigen::Vector3d nextPosition = m_dragStartBodyPosition;

        if (m_dragMode == "axis_x") {
            nextPosition.x() += dragDelta.x();
        }
        else if (m_dragMode == "axis_y") {
            nextPosition.y() += dragDelta.y();
        }
        else if (m_dragMode == "axis_z") {
            nextPosition.z() += dragDelta.z();
        }
        else {
            nextPosition.x() += dragDelta.x();
            nextPosition.y() += dragDelta.y();
        }

        if (!m_world->setBodyPosition(m_dragBodyId, nextPosition)) {
            return false;
        }

        refreshBodyModel();
        refreshConstraintModel();
        emit entityChanged();
        return true;
    }

    void SceneManagerViewModel::endBodyDrag()
    {
        if (!m_dragActive) {
            return;
        }

        m_dragBodyId = 0;
        m_dragMode = "xy_plane";
        m_dragStartBodyPosition = Eigen::Vector3d::Zero();
        m_dragStartPlanePoint = Eigen::Vector3d::Zero();
        setDragActive(false);
    }

    QVector3D SceneManagerViewModel::buildRenderScale(std::uint32_t type, const QVector3D& modelScale,
                                                      const QVector3D& boxSize_optional, double radius_optional,
                                                      double height_optional, double length_optional)
    {
        return TransformTransformer::buildQuick3DRenderScale(type, modelScale, boxSize_optional,
                                                             radius_optional, height_optional,
                                                             length_optional);
    }

    QVector3D SceneManagerViewModel::buildRenderPosition(const QVector3D& position)
    {
        return TransformTransformer::buildQuick3DRenderPosition(position);
    }

    QQuaternion SceneManagerViewModel::buildRenderRotation(const QQuaternion& rotation)
    {
        return TransformTransformer::buildQuick3DRenderRotation(rotation);
    }

    void SceneManagerViewModel::onFrameTick()
    {
        const double realFrameSeconds = std::max(0.0, static_cast<double>(m_frameClock.restart()) / 1000.0);

        // 时间缩放：累加器按缩放后的虚拟时间推进，物理积分仍以 m_fixedStepSeconds 为准
        m_accumulator += realFrameSeconds * m_timeScale;
        updateFps(realFrameSeconds);

        int subStepCount = 0;
        while (m_accumulator >= m_fixedStepSeconds && subStepCount < m_maxSubSteps) {
            advanceFixedStep(false);
            m_accumulator -= m_fixedStepSeconds;
            ++subStepCount;
        }

        // 修复死亡螺旋：累加器超限时钳制到 maxSubSteps*dt，
        // 保留溢出时间在下一帧继续消耗，而非丢弃导致物理时间永久落后。
        const double maxAccumulator = m_fixedStepSeconds * static_cast<double>(m_maxSubSteps);
        if (m_accumulator > maxAccumulator) {
            m_accumulator = maxAccumulator;
        }
    }

    void SceneManagerViewModel::rebuildDemoWorld()
    {
        if (m_world == nullptr) {
            LOG_FATAL("SceneManagerViewModel::rebuildDemoWorld(): WorldSceneManager is nullptr");
            return;
        }

        m_world->addGravityField({0.0, 0.0, -180.0}, {0.0, 0.0, 0.0}, "重力场");
        std::uint64_t id1 = m_world->spawnSphere("123", {0, 0, 300}, {0, 0, 0}, 10, 10);
        std::uint64_t id2 = m_world->spawnSphere("456", {0, 0, 400}, {0, 0, 0}, 10, 0);
        // 创建一个弹性系数 50 N/m、原长 300m 的弹簧，连接 bodyA 和 bodyB
        m_world->createSpring("MySpring", 50.0, 100.0, id2, id1);
        // std::uint64_t idBox = m_world->spawnBox("", {0, 0, 50}, {0, 0, 0}, {100, 100, 30}, 1);
        //
        // auto box = m_world->getEntity(idBox);
        //
        // Eigen::Matrix3d R;
        // double angle = 30.0 * M_PI / 180.0;  // 转为弧度
        // double c = std::cos(angle);
        // double s = std::sin(angle);
        //
        // R << 1, 0,  0,
        //      0, c, -s,
        //      0, s,  c;
        //
        // box->setRotation(R);
        // box->setAngular({10, 10, 10});

        std::uint64_t id3 = m_world->spawnSphere("1", {100, 100, 490}, {0, 0,  0}, 10, 10);
        // std::uint64_t idBox1 = m_world->spawnBox("2", {0, 0, 100}, {0, 0, 0}, {10, 10, 30}, 10);
        std::uint64_t idBox2 = m_world->spawnBox("3", {0, 0, 0}, {0, 0, 0}, {10000, 10000, 130}, 0);


        std::uint64_t id4 = m_world->spawnSphere("阻力测试", {300, 200, 400}, {0, 200, 0}, 10, 100);
        std::unique_ptr<AirResistance> airResistance = std::make_unique<AirResistance>(UniqueIDGenerator::instance().generate());
        // airResistance->registerEntity(m_world->getEntity(id4));
        auto body = dynamic_cast<RigidBody*>(m_world->getEntity(id4));
        airResistance->addEntity(body, DragConfig{
            .model = DragModel::Quadratic,
            .dragCoefficient = 0.47,
            // .crossSectionArea = quickSquare(dynamic_cast<ShapeSphere*>(body->getShape().get())->getRadius()) * PI
        });

        m_world->registerConstraint("Air Resistance", std::move(airResistance));


        qDebug() << id1 << id2 << id3 << idBox2;


        for (int i = 0; i < 100; ++i) {
            m_world->spawnSphere("并行测试" + std::to_string(i), {300, 210 + i * 10.0, 420 + i * 20.0}, {0, 0, 0}, 10, 10);
        }
        m_world->spawnSphere("并行测试2", {300, 220, 440}, {0, 0, 0}, 10, 10);
        m_world->spawnSphere("并行测试3", {300, 230, 450}, {0, 0, 0}, 10, 10);

        // === SlidingRail 滑轨约束测试 ===
        // 测试 1：圆柱螺旋线轨道
        // 螺旋线半径 150，轴向速度 30，4 圈（≈ 8π）
        // 在螺旋线起点附近生成两个球体，注册到滑轨上
        // {
        //     constexpr double helixRadius = 150.0;
        //     constexpr double helixAxialSpeed = 30.0;
        //     constexpr double helixEndT = 8.0 * std::numbers::pi;
        //
        //     const double startX = 0.0;
        //     const double startY = 0.0;
        //     const double startZ = 200.0;
        //
        //     // 在螺旋线起点附近创建两个测试球体
        //     std::uint64_t railBody1 = m_world->spawnSphere(
        //         "RailBall1", {startX + 0, startY + 0, startZ + 0},
        //         {50, 0, 0}, 10, 10);
        //     std::uint64_t railBody2 = m_world->spawnSphere(
        //         "RailBall2", {startX + 0, startY + 0, startZ + 100},
        //         {80, 0, 30}, 10, 10);
        //
        //     auto helix = std::make_unique<CurveHelix>(
        //         helixRadius, helixAxialSpeed, helixEndT);
        //     m_world->createSlidingRail("HelixRail", std::move(helix),
        //                                {railBody1, railBody2});
        // }

        // 测试 2：贝塞尔曲线轨道
        // 3 个控制点，形成一条从 (0, 400, 300) 到 (300, 700, 300) 的弧线
        // {
        //     std::vector<Eigen::Vector3d> controlPoints = {
        //         {0.0, 400.0, 300.0},
        //         {150.0, 550.0, 500.0},
        //         {300.0, 700.0, 300.0},
        //     };
        //
        //     std::uint64_t railBody3 = m_world->spawnSphere(
        //         "RailBall3", {0, 400, 300},
        //         {30, 30, 10}, 10, 10);
        //
        //     auto bezier = std::make_unique<CurveBezier>(std::move(controlPoints));
        //     m_world->createSlidingRail("BezierRail", std::move(bezier),
        //                                {railBody3});
        // }

        // // m_world->createConnectingLine("114514", 140, id1, id2);
        //
        //
        // std::uint64_t id3 = m_world->spawnSphere("123", {10, 120, 120}, {0, 0, 0}, 10, 10);
        // m_world->createSimplePendulum("1", 150, {10, 200, 200}, id3);
        // // m_world->createSpring("MySpring2", 50.0, 100.0, id3, id1);
        //
        // qDebug() << id1 << id2 << id3;
        //
        // m_surfaceModel.setPlaneExtent(2000.0);
        //
        // // === 创建 Bezier 曲面 SupportSurface 用于测试 ===
        // // 4x4 控制点网格，形成一个中心隆起的缓坡曲面
        // // 偏移到 (300, -150, 0) 处，避免与地面平面重叠
        // using CP = std::array<std::array<Eigen::Vector3d, 4>, 4>;
        // CP bezierCP = {
        //     {
        //         {
        //             {
        //                 Eigen::Vector3d{0, 0, 0}, Eigen::Vector3d{100, 0, 10},
        //                 Eigen::Vector3d{200, 0, 10}, Eigen::Vector3d{300, 0, 0}
        //             }
        //         },
        //         {
        //             {
        //                 Eigen::Vector3d{0, 100, 10}, Eigen::Vector3d{100, 100, 60},
        //                 Eigen::Vector3d{200, 100, 80}, Eigen::Vector3d{300, 100, 10}
        //             }
        //         },
        //         {
        //             {
        //                 Eigen::Vector3d{0, 200, 10}, Eigen::Vector3d{100, 200, 80},
        //                 Eigen::Vector3d{200, 200, 60}, Eigen::Vector3d{300, 200, 10}
        //             }
        //         },
        //         {
        //             {
        //                 Eigen::Vector3d{0, 300, 0}, Eigen::Vector3d{100, 300, 10},
        //                 Eigen::Vector3d{200, 300, 10}, Eigen::Vector3d{300, 300, 0}
        //             }
        //         }
        //     }
        // };
        //
        // // 整体偏移到场景中的目标位置
        // const Eigen::Vector3d bezierOffset{200, -150, 100};
        //
        // for (auto& row : bezierCP) {
        //     for (auto& cp : row) {
        //         cp += bezierOffset;
        //     }
        // }
        //
        // // 在 Bezier 曲面上方生成两个测试球体
        // std::uint64_t id4 = m_world->spawnSphere("b1", {300, -50, 500}, {0, 0, 0}, 10, 10);
        // std::uint64_t id5 = m_world->spawnSphere("b2", {250, 100, 450}, {0, 0, 0}, 10, 10);
        //
        //
        // m_world->createSpring("MySpring2", 50.0, 100.0, id5, id1);
        // m_world->createSpring("MySpring2", 50.0, 100.0, id5, id4);
        //
        // auto bezier = std::make_unique<SurfaceBezier>(bezierCP);
        // m_world->createSupportSurface(
        //     "BezierHill",
        //     std::move(bezier),
        //     0.6, // 恢复系数 0.6
        //     {id4, id5} // 注册到这个曲面的实体
        // );
        //
        //
        // std::uint64_t id8 = m_world->spawnSphere("c1", {400, 300, 500}, {0, 0, 0}, 10, 10);
        // std::uint64_t id9 = m_world->spawnSphere("c2", {460, 360, 450}, {0, 0, 0}, 10, 10);
        //
        // m_world->createRod("rod", sqrt(3600 + 3600 + 2500), id8, id9);
        //
        // std::uint64_t id10 = m_world->spawnSphere("1", {100, 100, 100}, {0, 0, 0}, 10, 10);
        // std::uint64_t id11 = m_world->spawnSphere("2", {200, 160, 180}, {0, 0, 0}, 10, 60);
        //
        // m_world->createRod("rod", 141, id10, id11);
        //
        // // 收集所有实体 ID 用于地面注册
        // std::vector<std::uint64_t> allIds = {
        //     id1, id2, id3, id4, id5, id8, id9, id10, id11
        // };
        //
        // // 注册地面
        // auto plane = std::make_unique<SurfacePlane>(
        //     Eigen::Vector3d(0, 0, 0),
        //     Eigen::Vector3d::UnitZ());
        //
        // m_world->createSupportSurface(
        //     "Ground",
        //     std::move(plane),
        //     1.0,
        //     allIds
        // );

        m_entityCount = static_cast<int>(m_world->getBodyCount());
    }

    void SceneManagerViewModel::refreshBodyModel()
    {
        if (m_world == nullptr) {
            m_bodyModel.clear();
            return;
        }

        QList<SceneBodyVisualItem> items;
        const std::vector<SceneRenderItem> renderItems = m_world->buildRenderItems();
        items.reserve(static_cast<qsizetype>(renderItems.size()));

        for (const SceneRenderItem& renderItem : renderItems) {
            SceneBodyVisualItem item{};
            item.id = renderItem.id;
            item.kind = renderItem.kind;

            item.velocity[0] = static_cast<float>(renderItem.velocity.x());
            item.velocity[1] = static_cast<float>(renderItem.velocity.y());
            item.velocity[2] = static_cast<float>(renderItem.velocity.z());

            item.angularVelocity[0] = static_cast<float>(renderItem.angularVelocity.x());
            item.angularVelocity[1] = static_cast<float>(renderItem.angularVelocity.y());
            item.angularVelocity[2] = static_cast<float>(renderItem.angularVelocity.z());

            item.centerPos[0] = static_cast<float>(renderItem.position.x());
            item.centerPos[1] = static_cast<float>(renderItem.position.y());
            item.centerPos[2] = static_cast<float>(renderItem.position.z());

            item.scale[0] = static_cast<float>(renderItem.scale.x());
            item.scale[1] = static_cast<float>(renderItem.scale.y());
            item.scale[2] = static_cast<float>(renderItem.scale.z());

            item.rotation[0] = static_cast<float>(renderItem.rotation.w());
            item.rotation[1] = static_cast<float>(renderItem.rotation.x());
            item.rotation[2] = static_cast<float>(renderItem.rotation.y());
            item.rotation[3] = static_cast<float>(renderItem.rotation.z());

            item.color = QString::fromStdString(renderItem.color);
            item.label = QString::fromStdString(renderItem.label);

            items.push_back(std::move(item));
        }

        m_bodyModel.setItems(items);
    }

    void SceneManagerViewModel::build3BodyModel() const
    {

        if (m_world == nullptr) {
            LOG_FATAL("SceneManagerViewModel::build3BodyModel(): m_world == nullptr");
            return;
        }
        auto generateField{
            [&](const Eigen::Vector3d& position, const Eigen::Vector3d& speed, double mass,
                double coupling_G = G)
            {
                static int i = 0;
                i++;
                auto gravitationalField = std::make_shared<GravitationalField>(
                    position,
                    speed,
                    mass, coupling_G,
                    false, i);

                GravitationalField::setMinDistanceSquared(1600);

                // gravitationalField->setCouplingCoefficient(1.0);
                auto gravFieldEntity = std::static_pointer_cast<PhysicalEntity>(gravitationalField);
                auto gravFieldBase = std::static_pointer_cast<FieldBase>(gravitationalField);
                static int id = 0;
                id++;
                QString bodyName{"测试引力源"};
                bodyName += QString::number(i);
                QString fieldName{"引力场"};
                fieldName += QString::number(i);

                m_world->registerBodyField(bodyName.toStdString(),
                                           fieldName.toStdString(),
                                           gravFieldEntity,
                                           gravFieldBase,
                                           std::make_unique<ShapeSphere>(10.0));
            }
        };

        generateField({300, 0, -1}, {0, 75, 100}, 200, 50000);
        generateField({-150, 259.8, 100}, {-120, -45, 43}, 100, 50000);
        generateField({-150, -259.8, -300}, {-120, 45, 23}, 50, 50000);

    }

    void SceneManagerViewModel::refreshConstraintModel()
    {
        if (m_world == nullptr) {
            m_constraintModel.clear();
            return;
        }

        QList<SceneConstraintVisualItem> items;
        const std::vector<SceneConstraintRenderItem> renderItems = m_world->buildConstraintRenderItems();
        items.reserve(static_cast<qsizetype>(renderItems.size()));

        for (const SceneConstraintRenderItem& renderItem : renderItems) {
            SceneConstraintVisualItem item{};
            item.id = renderItem.id;
            item.kind = renderItem.kind;
            item.entityIdA = renderItem.entityIdA;
            item.entityIdB = renderItem.entityIdB;

            item.positionA[0] = static_cast<float>(renderItem.positionA.x());
            item.positionA[1] = static_cast<float>(renderItem.positionA.y());
            item.positionA[2] = static_cast<float>(renderItem.positionA.z());

            item.positionB[0] = static_cast<float>(renderItem.positionB.x());
            item.positionB[1] = static_cast<float>(renderItem.positionB.y());
            item.positionB[2] = static_cast<float>(renderItem.positionB.z());

            item.constraintLength = renderItem.constraintLength;
            item.currentDistance = renderItem.currentDistance;

            item.color = QString::fromStdString(renderItem.color);
            item.label = QString::fromStdString(renderItem.label);

            items.push_back(std::move(item));
        }

        m_constraintModel.setItems(items);
    }

    void SceneManagerViewModel::refreshSurfaceModel()
    {
        if (m_world == nullptr) {
            m_surfaceModel.clear();
            return;
        }

        const std::vector<SceneSurfaceRenderItem> renderItems =
            m_world->buildSurfaceRenderItems();
        m_surfaceModel.setItems(renderItems);
    }

    void SceneManagerViewModel::refreshCurveModel()
    {
        if (m_world == nullptr) {
            m_curveModel.clear();
            return;
        }

        const std::vector<SceneCurveRenderItem> renderItems =
            m_world->buildCurveRenderItems();
        m_curveModel.setItems(renderItems);
    }

    void SceneManagerViewModel::advanceFixedStep(const bool emitFrameSignal)
    {
        if (m_world == nullptr) {
            return;
        }

        const SolverStepResult result = m_world->tick(m_fixedStepSeconds);
        std::ignore = result;

        m_simTime = m_world->getSimulationTime();
        m_stepCount = m_world->getStepCount();
        m_entityCount = static_cast<int>(m_world->getBodyCount());

        refreshBodyModel();
        refreshConstraintModel();
        refreshSurfaceModel();
        refreshCurveModel();
        emit simTimeChanged();
        emit stepCountChanged();
        emit entityCountChanged();
        emit entityChanged();
        if (emitFrameSignal) {
            emit frameAdvanced();
        }
    }

    void SceneManagerViewModel::updateFps(const double realFrameSeconds)
    {
        if (realFrameSeconds <= 0.0) {
            return;
        }

        const double newFps = 1.0 / realFrameSeconds;
        m_fps = m_fps <= 0.0 ? newFps : m_fps * 0.9 + newFps * 0.1;
        emit fpsChanged();
    }

    void SceneManagerViewModel::setRunning(const bool running)
    {
        if (m_running == running) {
            return;
        }

        m_running = running;
        emit runningChanged();
    }

    bool SceneManagerViewModel::buildPickRay(const double screenX,
                                             const double screenY,
                                             const double viewportWidth,
                                             const double viewportHeight,
                                             const QVariantMap& cameraState,
                                             Eigen::Vector3d* origin,
                                             Eigen::Vector3d* direction,
                                             QString* error)
    {
        if (origin == nullptr || direction == nullptr) {
            if (error != nullptr) {
                *error = "invalid_output_pointer";
            }
            return false;
        }

        if (viewportWidth <= 1.0 || viewportHeight <= 1.0) {
            if (error != nullptr) {
                *error = "invalid_viewport";
            }
            return false;
        }

        const CameraState camera = parseCameraState(cameraState);
        QVector3D up = camera.up.normalized();
        if (up.lengthSquared() < 1e-6f) {
            up = QVector3D(0.0f, 0.0f, 1.0f);
        }

        QMatrix4x4 view{};
        view.lookAt(camera.position, camera.target, up);

        QMatrix4x4 projection{};
        projection.perspective(static_cast<float>(camera.fovY),
                               static_cast<float>(viewportWidth / viewportHeight),
                               static_cast<float>(std::max(0.001, camera.nearClip)),
                               static_cast<float>(std::max(camera.nearClip + 0.001, camera.farClip)));

        const QMatrix4x4 inverseViewProjection = (projection * view).inverted();

        const auto ndcX = static_cast<float>((screenX / viewportWidth) * 2.0 - 1.0);
        const auto ndcY = static_cast<float>(1.0 - (screenY / viewportHeight) * 2.0);

        QVector4D nearPoint = inverseViewProjection * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
        QVector4D farPoint = inverseViewProjection * QVector4D(ndcX, ndcY, 1.0f, 1.0f);
        if (std::abs(nearPoint.w()) < 1e-8f || std::abs(farPoint.w()) < 1e-8f) {
            if (error != nullptr) {
                *error = "non_invertible_view_projection";
            }
            return false;
        }

        nearPoint /= nearPoint.w();
        farPoint /= farPoint.w();

        const QVector3D rayOriginVec = nearPoint.toVector3D();
        QVector3D rayDirectionVec = (farPoint - nearPoint).toVector3D();
        if (rayDirectionVec.lengthSquared() < 1e-10f) {
            if (error != nullptr) {
                *error = "invalid_ray_direction";
            }
            return false;
        }
        rayDirectionVec.normalize();

        *origin = Eigen::Vector3d(rayOriginVec.x(), rayOriginVec.y(), rayOriginVec.z());
        *direction = Eigen::Vector3d(rayDirectionVec.x(), rayDirectionVec.y(), rayDirectionVec.z());
        return true;
    }

    bool SceneManagerViewModel::intersectRayWithPlaneZ(const Eigen::Vector3d& rayOrigin,
                                                       const Eigen::Vector3d& rayDirection,
                                                       const double planeZ,
                                                       Eigen::Vector3d* hitPoint,
                                                       QString* error)
    {
        if (hitPoint == nullptr) {
            if (error != nullptr) {
                *error = "invalid_hit_pointer";
            }
            return false;
        }

        if (std::abs(rayDirection.z()) < 1e-9) {
            if (error != nullptr) {
                *error = "ray_parallel_to_plane";
            }
            return false;
        }

        const double t = (planeZ - rayOrigin.z()) / rayDirection.z();
        *hitPoint = rayOrigin + rayDirection * t;
        return true;
    }

    void SceneManagerViewModel::setDragActive(const bool active)
    {
        if (m_dragActive == active) {
            return;
        }
        m_dragActive = active;
        emit dragActiveChanged();
    }
}
