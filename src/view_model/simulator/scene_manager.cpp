//
// 由 GitHub Copilot 于 2026/4/23 创建。
//

#include "scene_manager.h"
#include <QCryptographicHash>
// 原代码：
// namespace egret {
//
// class SceneManager {
//
// };
//
// } // egret
//
// 新代码说明：
// 1. 将原来的空壳类替换为可直接暴露给 QML 的 ViewModel 门面对象。
// 2. 该对象负责驱动模型层世界管理器、维护固定步长累积，并刷新 QML 渲染数据。

#include <algorithm>
#include <cmath>
#include <QFile>

#include <QMatrix4x4>
#include <QVector3D>

#include "world_scene_manager.h"
#include "broad_phase_strategy/brute_force_broad_phase.h"
#include "contact_strategy/frictionless_contact_resolver.h"
#include "integrator_strategy/semi_implicit_euler_integrator.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "shape_box.h"
#include "shape_sphere.h"
#include "shape_cylinder.h"
#include "rigid_body.h"
#include "gravitational_field.h"
#include "logger.h"
#include "model_item_data.h"

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

        [[nodiscard]] double readCameraScalar(const QVariantMap &cameraState,
                                              const QString &key,
                                              const double fallback)
        {
            const QVariant value = cameraState.value(key);
            bool ok = false;
            const double converted = value.toDouble(&ok);
            return ok ? converted : fallback;
        }

        [[nodiscard]] CameraState parseCameraState(const QVariantMap &cameraState)
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

    SceneManagerViewModel::SceneManagerViewModel(QObject *parent) : QObject(parent), m_bodyModel(this)
    {
        m_timer.setInterval(16);
        m_timer.setTimerType(Qt::PreciseTimer);
        connect(&m_timer, &QTimer::timeout, this, &SceneManagerViewModel::onFrameTick);

        rebuildDemoWorld();
        refreshBodyModel();
        m_frameClock.start();
    }

    SceneRecord SceneManagerViewModel::createSceneFromJsonString(const QString &jsonString)
    {
        SceneRecord record;

        QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
        if (doc.isNull() || !doc.isObject())
        {
            return record;
        }

        QJsonObject root = doc.object();
        if (!root.contains("models") || !root["models"].isArray())
        {
            return record;
        }

        QJsonArray modelsArray = root["models"].toArray();
        for (const QJsonValueRef &value : modelsArray)
        {
            if (!value.isObject())
            {
                continue;
            }

            std::shared_ptr<ModelItemData> model = std::make_shared<ModelItemData>();
            if (!model->fromJson(value.toObject()))
            {
                continue;
            }

            // Determine shape type from source
            QString source = model->source();
            std::unique_ptr<ShapeBase> shape;

            if (source == "#Cube")
            {
                QVector3D scale = model->scale();
                if (scale.x() <= 0)
                    scale.setX(1.0);
                if (scale.y() <= 0)
                    scale.setY(1.0);
                if (scale.z() <= 0)
                    scale.setZ(1.0);
                shape = std::make_unique<ShapeBox>(Eigen::Vector3d(scale.x(), scale.y(), scale.z()));
            }
            else if (source == "#Sphere")
            {
                double radius = model->scale().x();
                if (radius <= 0)
                    radius = 1.0;
                shape = std::make_unique<ShapeSphere>(radius);
            }
            else if (source == "#Cylinder")
            {
                double radius = model->scale().x();
                double height = model->scale().y();
                if (radius <= 0)
                    radius = 1.0;
                if (height <= 0)
                    height = 1.0;
                shape = std::make_unique<ShapeCylinder>(radius, height);
            }
            else
            {
                // Default to box with scale or unit box
                QVector3D scale = model->scale();
                if (scale.x() <= 0)
                    scale.setX(1.0);
                if (scale.y() <= 0)
                    scale.setY(1.0);
                if (scale.z() <= 0)
                    scale.setZ(1.0);
                shape = std::make_unique<ShapeBox>(Eigen::Vector3d(scale.x(), scale.y(), scale.z()));
            }

            // Create body position and velocity from model data
            QVector3D pos = model->pos();
            QVector3D vel = model->initialVelo();
            Eigen::Vector3d position(pos.x(), pos.y(), pos.z());
            Eigen::Vector3d velocity(vel.x(), vel.y(), vel.z());
            double mass = model->mass();

            // Create RigidBody
            auto entity = std::make_shared<RigidBody>(position, velocity, mass);

            // LOG_DEBUG(QString::number(mass));

            entity->setShape(std::move(shape));

            auto hashToUInt64 = [](const QString &str) -> quint64 {
                QByteArray data = str.toUtf8();
                QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha512);

                quint64 result = 0;
                for (int i = 0; i < 8; ++i) {
                    result <<= 8;
                    result |= static_cast<unsigned char>(hash[i]);
                }
                return result;
            };

            // Create BodyRecord
            BodyRecord bodyRecord;
            bodyRecord.id = hashToUInt64(model->id()); // Use model pointer as id
            bodyRecord.name = model->name().toStdString();
            bodyRecord.entity = entity;
            bodyRecord.enableCollision = true;
            bodyRecord.enableIntegration = true;

            record.bodies.push_back(bodyRecord);
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

    bool SceneManagerViewModel::isDragActive() const
    {
        return m_dragActive;
    }

    SceneBodyModel *SceneManagerViewModel::bodyModel()
    {
        return &m_bodyModel;
    }

    void SceneManagerViewModel::play()
    {
        if (m_running)
        {
            return;
        }

        setRunning(true);
        m_frameClock.restart();
        m_timer.start();
    }

    void SceneManagerViewModel::pause()
    {
        if (!m_running)
        {
            return;
        }

        m_timer.stop();
        setRunning(false);
    }

    void SceneManagerViewModel::toggleRunning()
    {
        if (m_running)
        {
            pause();
        }
        else
        {
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
        rebuildDemoWorld();
        refreshBodyModel();

        m_simTime = 0.0;
        m_stepCount = 0;
        m_accumulator = 0.0;
        m_entityCount = m_world != nullptr ? static_cast<int>(m_world->getBodyCount()) : 0;

        emit simTimeChanged();
        emit stepCountChanged();
        emit entityCountChanged();
        emit sceneReset();
        emit entityChanged();
    }

    void SceneManagerViewModel::loadSceneFronJsonFile(const QString &fileName)
    {
    }

    void SceneManagerViewModel::loadSceneFromJsonString(const QString &jsonString)
    {
    }

    QVariantMap SceneManagerViewModel::mapScreenToWorldOnPlane(const double screenX,
                                                               const double screenY,
                                                               const double viewportWidth,
                                                               const double viewportHeight,
                                                               const QVariantMap &cameraState,
                                                               const double planeZ) const
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
                          &error))
        {
            result.insert("ok", false);
            result.insert("error", error);
            return result;
        }

        Eigen::Vector3d hitPoint{};
        if (!intersectRayWithPlaneZ(rayOrigin, rayDirection, planeZ, &hitPoint, &error))
        {
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

    bool SceneManagerViewModel::beginBodyDrag(const quint64 bodyId,
                                              const double screenX,
                                              const double screenY,
                                              const double viewportWidth,
                                              const double viewportHeight,
                                              const QVariantMap &cameraState,
                                              const QString &mode)
    {
        if (m_world == nullptr)
        {
            return false;
        }

        const std::optional<Eigen::Vector3d> bodyPosition = m_world->getBodyPosition(bodyId);
        if (!bodyPosition.has_value())
        {
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
                          &rayDirection))
        {
            return false;
        }

        Eigen::Vector3d planePoint{};
        if (!intersectRayWithPlaneZ(rayOrigin, rayDirection, bodyPosition->z(), &planePoint))
        {
            return false;
        }

        m_dragBodyId = bodyId;
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
                                               const QVariantMap &cameraState)
    {
        if (!m_dragActive || m_world == nullptr || m_dragBodyId == 0)
        {
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
                          &rayDirection))
        {
            return false;
        }

        Eigen::Vector3d currentPlanePoint{};
        if (!intersectRayWithPlaneZ(rayOrigin, rayDirection, m_dragStartBodyPosition.z(), &currentPlanePoint))
        {
            return false;
        }

        const Eigen::Vector3d dragDelta = currentPlanePoint - m_dragStartPlanePoint;
        Eigen::Vector3d nextPosition = m_dragStartBodyPosition;

        if (m_dragMode == "axis_x")
        {
            nextPosition.x() += dragDelta.x();
        }
        else if (m_dragMode == "axis_y")
        {
            nextPosition.y() += dragDelta.y();
        }
        else if (m_dragMode == "axis_z")
        {
            nextPosition.z() += dragDelta.z();
        }
        else
        {
            nextPosition.x() += dragDelta.x();
            nextPosition.y() += dragDelta.y();
        }

        if (!m_world->setBodyPosition(m_dragBodyId, nextPosition))
        {
            return false;
        }

        refreshBodyModel();
        emit entityChanged();
        return true;
    }

    void SceneManagerViewModel::endBodyDrag()
    {
        if (!m_dragActive)
        {
            return;
        }

        m_dragBodyId = 0;
        m_dragMode = "xy_plane";
        m_dragStartBodyPosition = Eigen::Vector3d::Zero();
        m_dragStartPlanePoint = Eigen::Vector3d::Zero();
        setDragActive(false);
    }

    void SceneManagerViewModel::onFrameTick()
    {
        const double realFrameSeconds = std::max(0.0, static_cast<double>(m_frameClock.restart()) / 1000.0);
        m_accumulator += realFrameSeconds;
        updateFps(realFrameSeconds);

        int subStepCount = 0;
        while (m_accumulator >= m_fixedStepSeconds && subStepCount < m_maxSubSteps)
        {
            advanceFixedStep(false);
            m_accumulator -= m_fixedStepSeconds;
            ++subStepCount;
        }

        if (m_accumulator >= m_fixedStepSeconds * static_cast<double>(m_maxSubSteps))
        {
            m_accumulator = 0.0;
        }
    }

    void SceneManagerViewModel::rebuildDemoWorld()
    {
        SolverConfig config{};

        // config.enableNarrowPhase = false;

        auto solver = std::make_unique<Solver>(
            config,
            std::make_unique<SemiImplicitEulerIntegratorStrategy>(),
            std::make_unique<BruteForceBroadPhaseStrategy>(),
            std::make_unique<FrictionlessContactResolverStrategy>());

        m_world = std::make_unique<WorldSceneManager>(std::move(solver));
        m_world->clear();
        //
        m_world->addGravityField({0.0, 0.0, -180.0}, {0.0, 0.0, 0.0}, "重力场");
        //
        auto record = createSceneFromJsonFile("C:/Users/jehor/Desktop/1.json");
        m_world->registerScene(record);

        auto generateBox{[&]()
                         {
                             m_world->spawnBox("地面", {0.0, 0.0, -15.0}, {0.0, 0.0, 0.0}, {800.0, 600.0, 30.0}, 0.0);
                             m_world->spawnBox("地面", {0, -300, 300}, {0.0, 0.0, 0.0}, {800, 30, 600}, 0.0);
                             m_world->spawnBox("地面", {0, 300, 300}, {0.0, 0.0, 0.0}, {800, 30, 600}, 0.0);
                             m_world->spawnBox("地面", {400, 0, 300}, {0.0, 0.0, 0.0}, {30, 600, 600}, 0.0);
                             m_world->spawnBox("地面", {-400, 0, 300}, {0.0, 0.0, 0.0}, {30, 600, 600}, 0.0);
                         }};

        auto generateRotationTest{[&]()
                                  {
                                      std::uint64_t idBox = m_world->spawnBox("测试", {0, 400, 300}, {320, 100, 233}, {100, 30, 100}, 10.0);
                                      // std::uint64_t idBox2 = m_world->spawnBox("测试", {0, 0, 430}, {0, 0, 0}, {100, 30, 100}, 10.0);
                                      // Eigen::Matrix3d R;
                                      double angle_z = M_PI / 5.0;          // 36 degrees
                                      double angle_x = 40.0 * M_PI / 180.0; // 40 degrees

                                      double cz = std::cos(angle_z);
                                      double sz = std::sin(angle_z);
                                      double cx = std::cos(angle_x);
                                      double sx = std::sin(angle_x);

                                      // 绕Z轴旋转矩阵
                                      Eigen::Matrix3d R_z;
                                      R_z << cz, -sz, 0,
                                          sz, cz, 0,
                                          0, 0, 1;

                                      // 绕X轴旋转矩阵
                                      Eigen::Matrix3d R_x;
                                      R_x << 1, 0, 0,
                                          0, cx, -sx,
                                          0, sx, cx;

                                      // 先绕Z转，再绕X转（注意乘法顺序：右边先应用）
                                      Eigen::Matrix3d R = R_x * R_z;
                                      //
                                      // double angle2 = M_PI / 6.0; // 45 degrees
                                      // double c2 = std::cos(angle2);
                                      // double s2 = std::sin(angle2);
                                      //
                                      // Eigen::Matrix3d R2{}; // 显式行优先
                                      // R2 << c2, -s2, 0,
                                      //     s2, c2, 0,
                                      //     0, 0, 1;

                                      m_world->setBodyRotation(idBox, R);

                                      m_world->createSimplePendulum("simple_pendulum_test", 100, {0, 0, 500}, idBox);
                                  }};

        auto generate3BodyScene{[&]()
                                {
                                    auto generateField{[&](const Eigen::Vector3d &position, const Eigen::Vector3d &speed, double mass, double coupling_G = G)
                                                       {
                                                           auto gravitationalField = std::make_shared<GravitationalField>(
                                                               position,
                                                               speed,
                                                               mass, coupling_G,
                                                               false);

                                                           GravitationalField::setMinDistanceSquared(1600);

                                                           // gravitationalField->setCouplingCoefficient(1.0);
                                                           auto gravFieldEntity = std::static_pointer_cast<PhysicalEntity>(gravitationalField);
                                                           auto gravFieldBase = std::static_pointer_cast<FieldBase>(gravitationalField);
                                                           static int i = 0;
                                                           i++;
                                                           QString bodyName{"测试引力源"};
                                                           bodyName += QString::number(i);
                                                           QString fieldName{"引力场"};
                                                           fieldName += QString::number(i);

                                                           m_world->registerBodyField(bodyName.toStdString(),
                                                                                      fieldName.toStdString(),
                                                                                      gravFieldEntity,
                                                                                      gravFieldBase,
                                                                                      std::make_unique<ShapeSphere>(10.0));
                                                       }};

                                    generateField({300, 0, -1}, {0, 75, 100}, 200, 50000);
                                    generateField({-150, 259.8, 100}, {-120, -45, 43}, 100, 50000);
                                    generateField({-150, -259.8, -300}, {-120, 45, 23}, 50, 50000);
                                }};

        auto generateSupportSurfaceTest{[&]()
                                        {
                                            m_world->spawnSphere("球", {0, 0, 300}, {0, 0, 0}, 10.0, 10.0);
                                            std::uint64_t id = m_world->spawnSphere("球", {0, 0, 200}, {0, 0, 0}, 10.0, 10.0);
                                        }};
        // generateBox();
        // generateSupportSurfaceTest();
        // m_world->spawnCylinder("cylinder", {}, {}, 4, 2, 4);

        // generateField({10, 0, 0}, {-0.1, 0, 0.1}, 1, 1000);
        // generateField({-10, 0, 0}, {0, 0.1, -0.1}, 1, 1000);
        // generateField({0, 0, 5}, {0.2, -0.1, 0}, 1, 1000);

        m_entityCount = static_cast<int>(m_world->getBodyCount());
    }

    void SceneManagerViewModel::refreshBodyModel()
    {
        if (m_world == nullptr)
        {
            m_bodyModel.clear();
            return;
        }

        std::vector<SceneBodyVisualItem> items;
        const std::vector<SceneRenderItem> renderItems = m_world->buildRenderItems();
        items.reserve(renderItems.size());

        for (const SceneRenderItem &renderItem : renderItems)
        {
            SceneBodyVisualItem item{};
            item.id = renderItem.id;
            item.kind = QString::fromStdString(renderItem.kind);
            item.x = renderItem.x;
            item.y = renderItem.y;
            item.width = renderItem.width;
            item.height = renderItem.height;
            item.centerX = renderItem.centerX;
            item.centerY = renderItem.centerY;
            item.centerZ = renderItem.centerZ;
            item.speedX = renderItem.speedX;
            item.speedY = renderItem.speedY;
            item.speedZ = renderItem.speedZ;
            item.sizeX = renderItem.sizeX;
            item.sizeY = renderItem.sizeY;
            item.sizeZ = renderItem.sizeZ;
            item.color = QColor(QString::fromStdString(renderItem.color));
            item.label = QString::fromStdString(renderItem.label);
            for (int i = 0; i < 9; ++i)
            {
                item.rotation[i] = renderItem.rotation[i];
            }
            items.push_back(std::move(item));
        }

        m_bodyModel.setItems(items);
    }

    void SceneManagerViewModel::advanceFixedStep(const bool emitFrameSignal)
    {
        if (m_world == nullptr)
        {
            return;
        }

        const SolverStepResult result = m_world->tick(m_fixedStepSeconds);
        std::ignore = result;

        m_simTime = m_world->getSimulationTime();
        m_stepCount = m_world->getStepCount();
        m_entityCount = static_cast<int>(m_world->getBodyCount());

        refreshBodyModel();
        emit simTimeChanged();
        emit stepCountChanged();
        emit entityCountChanged();
        emit entityChanged();
        if (emitFrameSignal)
        {
            emit frameAdvanced();
        }
    }

    void SceneManagerViewModel::updateFps(const double realFrameSeconds)
    {
        if (realFrameSeconds <= 0.0)
        {
            return;
        }

        const double newFps = 1.0 / realFrameSeconds;
        m_fps = m_fps <= 0.0 ? newFps : m_fps * 0.9 + newFps * 0.1;
        emit fpsChanged();
    }

    void SceneManagerViewModel::setRunning(const bool running)
    {
        if (m_running == running)
        {
            return;
        }

        m_running = running;
        emit runningChanged();
    }

    bool SceneManagerViewModel::buildPickRay(const double screenX,
                                             const double screenY,
                                             const double viewportWidth,
                                             const double viewportHeight,
                                             const QVariantMap &cameraState,
                                             Eigen::Vector3d *origin,
                                             Eigen::Vector3d *direction,
                                             QString *error) const
    {
        if (origin == nullptr || direction == nullptr)
        {
            if (error != nullptr)
            {
                *error = "invalid_output_pointer";
            }
            return false;
        }

        if (viewportWidth <= 1.0 || viewportHeight <= 1.0)
        {
            if (error != nullptr)
            {
                *error = "invalid_viewport";
            }
            return false;
        }

        const CameraState camera = parseCameraState(cameraState);
        QVector3D up = camera.up.normalized();
        if (up.lengthSquared() < 1e-6f)
        {
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
        if (std::abs(nearPoint.w()) < 1e-8f || std::abs(farPoint.w()) < 1e-8f)
        {
            if (error != nullptr)
            {
                *error = "non_invertible_view_projection";
            }
            return false;
        }

        nearPoint /= nearPoint.w();
        farPoint /= farPoint.w();

        const QVector3D rayOriginVec = nearPoint.toVector3D();
        QVector3D rayDirectionVec = (farPoint - nearPoint).toVector3D();
        if (rayDirectionVec.lengthSquared() < 1e-10f)
        {
            if (error != nullptr)
            {
                *error = "invalid_ray_direction";
            }
            return false;
        }
        rayDirectionVec.normalize();

        *origin = Eigen::Vector3d(rayOriginVec.x(), rayOriginVec.y(), rayOriginVec.z());
        *direction = Eigen::Vector3d(rayDirectionVec.x(), rayDirectionVec.y(), rayDirectionVec.z());
        return true;
    }

    bool SceneManagerViewModel::intersectRayWithPlaneZ(const Eigen::Vector3d &rayOrigin,
                                                       const Eigen::Vector3d &rayDirection,
                                                       const double planeZ,
                                                       Eigen::Vector3d *hitPoint,
                                                       QString *error)
    {
        if (hitPoint == nullptr)
        {
            if (error != nullptr)
            {
                *error = "invalid_hit_pointer";
            }
            return false;
        }

        if (std::abs(rayDirection.z()) < 1e-9)
        {
            if (error != nullptr)
            {
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
        if (m_dragActive == active)
        {
            return;
        }
        m_dragActive = active;
        emit dragActiveChanged();
    }
}
