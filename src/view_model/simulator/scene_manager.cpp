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
#include "eps_reader.h"
#include "offline_simulation_runner.h"
#include <QCryptographicHash>
#include "basic_render_item.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_set>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QtConcurrent>
#include <xlsxdocument.h>

#include <QMatrix4x4>
#include <QVector3D>

#include "world_scene_manager.h"
#include "broad_phase_strategy/brute_force_broad_strategy.h"
#include "contact_strategy/frictionless_contact_resolver.h"
#include "integrator_strategy/semi_implicit_euler_integrator.h"
#include "kinetic_sleep_strategy.h"
#include "sleep_strategy.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "air_resistance.h"
#include "shape_box.h"
#include "shape_capsule.h"
#include "shape_cylinder.h"
#include "shape_cylindrical_shell.h"
#include "shape_disk.h"
#include "shape_ring.h"
#include "shape_rod.h"
#include "shape_sphere.h"
#include "shape_spherical_shell.h"
#include "rigid_body.h"
#include "gravity_field.h"
#include "gravitational_field.h"
#include "constraints/connecting_line.h"
#include "constraints/simple_pendulum.h"
#include "constraints/spring.h"
#include "async_logger.h"
#include "model_item_data.h"
#include "transform_transformer.h"
#include "magic_enum.hpp"
// #include "surface_plane.h"
#include "constraints/connecting_rod.h"
// #include "surface_bezier.h"
// #include "surface_cylindrical.h"
#include "curve_bezier.h"
#include "curve_helix.h"
#include "curve_conic_section.h"
#include "constants.h"
#include "convex_mesh.h"
#include "sliding_rail.h"

#include "unique_id_generator.h"
#include "broad_phase_strategy/bvh_broad_strategy.h"
#include "broad_phase_strategy/octree_broad_strategy.h"
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

    SceneManagerViewModel::SceneManagerViewModel(QObject* parent) : QObject(parent), m_bodyModel(this),
                                                                    m_convexMeshModel(this)
    {
        m_timer.setInterval(16);
        m_timer.setTimerType(Qt::PreciseTimer);
        connect(&m_timer, &QTimer::timeout, this, &SceneManagerViewModel::onFrameTick);

        m_flushTimer.setInterval(2000);
        connect(&m_flushTimer, &QTimer::timeout, this, &SceneManagerViewModel::flushRecordings);

        initialize();
        rebuildDemoWorld();
        refreshBodyModel();
        refreshConstraintModel();
        refreshCurveModel();
        refreshConvexMeshModel();
        m_frameClock.start();
    }

    SceneManagerViewModel::~SceneManagerViewModel() = default;

    // -----------------------------------------------------------------------
    //  内部辅助 — JSON 解析 helper
    // -----------------------------------------------------------------------

    namespace
    {
        Eigen::Vector3d readVec3(const QJsonValue& v, const Eigen::Vector3d& fallback = Eigen::Vector3d::Zero())
        {
            if (v.isArray()) {
                const QJsonArray a = v.toArray();
                if (a.size() >= 3)
                    return {a[0].toDouble(), a[1].toDouble(), a[2].toDouble()};
            }
            if (v.isObject()) {
                const QJsonObject o = v.toObject();
                return {o.value("x").toDouble(), o.value("y").toDouble(), o.value("z").toDouble()};
            }
            return fallback;
        }

        Eigen::Quaterniond readQuat(const QJsonValue& v,
                                    const Eigen::Quaterniond& fallback = Eigen::Quaterniond::Identity())
        {
            if (!v.isArray()) return fallback;
            const QJsonArray a = v.toArray();
            if (a.size() < 4) return fallback;
            return {a[0].toDouble(), a[1].toDouble(), a[2].toDouble(), a[3].toDouble()};
        }

        /** 将 QJsonObject 中的形状参数填入 ShapeLoadInfo。 */
        void fillShapeLoadInfo(const QJsonObject& obj, std::uint32_t typeId, ShapeLoadInfo& info)
        {
            info.typeId = typeId;
            switch (static_cast<ShapeType>(typeId)) {
            case ShapeType::Box:
                if (obj.contains("box_size"))
                    info.numberParams["size"] = [&]
                    {
                        const Eigen::Vector3d s = readVec3(obj["box_size"], {1, 1, 1});
                        return std::vector{s.x(), s.y(), s.z()};
                    }();
                break;
            case ShapeType::Cylinder:
                [[fallthrough]];
            case ShapeType::CylindricalShell:
                info.numberParams["radius"] = {
                    obj.contains("radius") ? obj["radius"].toDouble() : obj.value("circle_radius").toDouble()
                };
                info.numberParams["height"] = {obj["height"].toDouble()};
                break;
            case ShapeType::Disk:
                [[fallthrough]];
            case ShapeType::Ring:
                [[fallthrough]];
            case ShapeType::Sphere:
                [[fallthrough]];
            case ShapeType::SphericalShell:
                info.numberParams["radius"] = {
                    obj.contains("radius") ? obj["radius"].toDouble() : obj.value("circle_radius").toDouble()
                };
                break;
            case ShapeType::Rod:
                info.numberParams["length"] = {obj["length"].toDouble()};
                break;
            default: break;
            }
        }

        /** 根据 ShapeLoadInfo 直接创建对应的形状实例。 */
        std::unique_ptr<ShapeBase> createShapeFromLoadInfo(const ShapeLoadInfo& info)
        {
            switch (static_cast<ShapeType>(info.typeId)) {
            case ShapeType::Sphere: {
                const auto it = info.numberParams.find("radius");
                if (it == info.numberParams.end() || it->second.empty()) return nullptr;
                return std::make_unique<ShapeSphere>(it->second[0]);
            }
            case ShapeType::Box: {
                const auto it = info.numberParams.find("size");
                if (it == info.numberParams.end() || it->second.size() < 3) return nullptr;
                return std::make_unique<ShapeBox>(
                    Eigen::Vector3d(it->second[0], it->second[1], it->second[2]));
            }
            case ShapeType::Capsule: {
                const auto rIt = info.numberParams.find("radius");
                const auto hIt = info.numberParams.find("height");
                if (rIt == info.numberParams.end() || hIt == info.numberParams.end()) return nullptr;
                if (rIt->second.empty() || hIt->second.empty()) return nullptr;
                return std::make_unique<ShapeCapsule>(rIt->second[0], hIt->second[0]);
            }
            case ShapeType::Cylinder: {
                const auto rIt = info.numberParams.find("radius");
                const auto hIt = info.numberParams.find("height");
                if (rIt == info.numberParams.end() || hIt == info.numberParams.end()) return nullptr;
                if (rIt->second.empty() || hIt->second.empty()) return nullptr;
                return std::make_unique<ShapeCylinder>(rIt->second[0], hIt->second[0]);
            }
            case ShapeType::CylindricalShell: {
                const auto rIt = info.numberParams.find("radius");
                const auto hIt = info.numberParams.find("height");
                if (rIt == info.numberParams.end() || hIt == info.numberParams.end()) return nullptr;
                if (rIt->second.empty() || hIt->second.empty()) return nullptr;
                return std::make_unique<ShapeCylindricalShell>(rIt->second[0], hIt->second[0]);
            }
            case ShapeType::Disk: {
                const auto it = info.numberParams.find("radius");
                if (it == info.numberParams.end() || it->second.empty()) return nullptr;
                return std::make_unique<ShapeDisk>(it->second[0]);
            }
            case ShapeType::Ring: {
                const auto it = info.numberParams.find("radius");
                if (it == info.numberParams.end() || it->second.empty()) return nullptr;
                return std::make_unique<ShapeRing>(it->second[0]);
            }
            case ShapeType::Rod: {
                const auto it = info.numberParams.find("length");
                if (it == info.numberParams.end() || it->second.empty()) return nullptr;
                return std::make_unique<ShapeRod>(it->second[0]);
            }
            case ShapeType::SphericalShell: {
                const auto it = info.numberParams.find("radius");
                if (it == info.numberParams.end() || it->second.empty()) return nullptr;
                return std::make_unique<ShapeSphericalShell>(it->second[0]);
            }
            default:
                return nullptr;
            }
        }

        /** 解析 DragConfig 子对象。 */
        DragConfig parseDragConfig(const QJsonObject& obj)
        {
            DragConfig cfg;
            if (obj.contains("model")) {
                const QString m = obj["model"].toString();
                if (m == "Linear") cfg.model = DragModel::Linear;
                else if (m == "Constant") cfg.model = DragModel::Constant;
                else cfg.model = DragModel::Quadratic;
            }
            if (obj.contains("linear_coeff")) cfg.linearCoeff = obj["linear_coeff"].toDouble();
            if (obj.contains("drag_coefficient")) cfg.dragCoefficient = obj["drag_coefficient"].toDouble();
            if (obj.contains("cross_section_area")) cfg.crossSectionArea = obj["cross_section_area"].toDouble();
            if (obj.contains("fluid_density")) cfg.fluidDensity = obj["fluid_density"].toDouble();
            if (obj.contains("constant_magnitude")) cfg.constantMagnitude = obj["constant_magnitude"].toDouble();
            if (obj.contains("rotational_linear_coeff"))
                cfg.rotationalLinearCoeff = obj["rotational_linear_coeff"].
                    toDouble();
            if (obj.contains("rotational_quadratic_coeff"))
                cfg.rotationalQuadraticCoeff = obj[
                    "rotational_quadratic_coeff"].toDouble();
            if (obj.contains("enable_rotational_drag"))
                cfg.enableRotationalDrag = obj["enable_rotational_drag"].
                    toBool();
            return cfg;
        }

        /** 把三维向量序列化为场景 JSON 数组。 */
        QJsonArray toJsonArray(const Eigen::Vector3d& value)
        {
            return QJsonArray{value.x(), value.y(), value.z()};
        }

        /** 把四元数序列化为场景 JSON 数组（w, x, y, z）。 */
        QJsonArray toJsonArray(const Eigen::Quaterniond& value)
        {
            return QJsonArray{value.w(), value.x(), value.y(), value.z()};
        }

        /**
         * @brief  把单个刚体序列化为场景 JSON 条目（category = 0）。
         * @details 形状参数直接取自 ShapeBase::getLoadInfo()，再映射回
         *          createSceneFromJsonString 认识的键名（size → box_size 等）。
         * @param  models 目标模型数组。
         * @param  entity 实体指针。
         * @param  name 实体名称。
         * @return 形状可被加载器还原时追加条目并返回 true，否则返回 false。
         */
        bool appendBodyJson(QJsonArray* models, PhysicalEntity* entity, const std::string& name)
        {
            if (models == nullptr || entity == nullptr || entity->getShape() == nullptr) {
                return false;
            }

            const ShapeLoadInfo info{entity->getShape()->getLoadInfo()};
            const auto shapeType{static_cast<ShapeType>(info.typeId)};
            const Transform& transform{entity->getTransformCR()};

            QJsonObject obj;
            obj["category"] = 0;
            obj["type"] = static_cast<qint64>(info.typeId);
            obj["id"] = QString::number(entity->getId());
            obj["name"] = QString::fromStdString(name);
            obj["pos"] = toJsonArray(entity->getPosition());
            obj["initial_velocity"] = toJsonArray(entity->getVelocity());
            obj["initial_angular_velocity"] = toJsonArray(entity->getAngularVelocity());
            obj["rotation"] = toJsonArray(transform.getRotation());
            obj["scale"] = toJsonArray(transform.getScale());
            obj["mass"] = entity->getMass();
            obj["restitution"] = entity->getRestitution();
            obj["visible"] = true;

            const auto emitParam = [&info, &obj](const char* sourceKey, const char* targetKey,
                                                 const bool asVector)
            {
                const auto iterator{info.numberParams.find(sourceKey)};
                if (iterator == info.numberParams.end() || iterator->second.empty()) {
                    return false;
                }
                if (asVector) {
                    if (iterator->second.size() < 3) {
                        return false;
                    }
                    obj[targetKey] = QJsonArray{iterator->second[0], iterator->second[1], iterator->second[2]};
                }
                else {
                    obj[targetKey] = iterator->second[0];
                }
                return true;
            };

            bool paramsOk{false};
            switch (shapeType) {
            case ShapeType::Box:
                paramsOk = emitParam("size", "box_size", true);
                break;
            case ShapeType::Sphere:
            case ShapeType::SphericalShell:
            case ShapeType::Disk:
            case ShapeType::Ring:
                paramsOk = emitParam("radius", "radius", false);
                break;
            case ShapeType::Cylinder:
            case ShapeType::CylindricalShell: {
                const bool radiusOk{emitParam("radius", "radius", false)};
                const bool heightOk{emitParam("height", "height", false)};
                paramsOk = radiusOk && heightOk;
                break;
            }
            case ShapeType::Rod:
                paramsOk = emitParam("length", "length", false);
                break;
            case ShapeType::Capsule:
                // 加载器的 fillShapeLoadInfo 尚未处理胶囊参数，跳过以免静默变形
                return false;
            default:
                // 凸体网格等形状没有可写入 JSON 的参数
                return false;
            }

            if (!paramsOk) {
                return false;
            }

            models->append(obj);
            return true;
        }

        /**
         * @brief  把单个力场序列化为场景 JSON 条目（category = 1）。
         * @param  models 目标模型数组。
         * @param  field 力场指针。
         * @param  name 力场名称。
         * @return 支持的场类型追加条目并返回 true，否则返回 false。
         */
        bool appendFieldJson(QJsonArray* models, FieldBase* field, const std::string& name)
        {
            if (models == nullptr || field == nullptr) {
                return false;
            }

            QJsonObject obj;
            obj["category"] = 1;
            obj["id"] = QString::number(field->getId());
            obj["name"] = QString::fromStdString(name);

            switch (field->getType()) {
            case FieldType::Gravity: {
                const auto* gravity{dynamic_cast<GravityField*>(field)};
                if (gravity == nullptr) {
                    return false;
                }
                obj["type"] = 1;
                obj["pos"] = toJsonArray(Eigen::Vector3d::Zero());

                QJsonObject data;
                data["gravity"] = toJsonArray(gravity->getGravity());
                data["reference_point"] = toJsonArray(Eigen::Vector3d::Zero());
                obj["field_data"] = data;
                break;
            }
            case FieldType::Gravitation: {
                const auto* gravitation{dynamic_cast<GravitationalField*>(field)};
                if (gravitation == nullptr) {
                    return false;
                }
                obj["type"] = 2;
                obj["pos"] = toJsonArray(gravitation->getPosition());

                QJsonObject data;
                data["speed"] = toJsonArray(gravitation->getVelocity());
                data["mass"] = gravitation->getMass();
                data["coupling"] = gravitation->getCouplingCoefficient();
                data["fixed"] = gravitation->isFixed();
                obj["field_data"] = data;
                break;
            }
            default:
                return false;
            }

            models->append(obj);
            return true;
        }
    } // anonymous namespace

    // -----------------------------------------------------------------------
    //  createSceneFromJsonString — 三遍创建
    // -----------------------------------------------------------------------

    SceneRecord SceneManagerViewModel::createSceneFromJsonString(const QString& jsonString)
    {
        // 1. 基础 JSON 解析
        const QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
        if (!doc.isObject()) return {};

        const QJsonObject root = doc.object();
        if (!root.contains("models") || !root["models"].isArray()) return {};

        const QJsonArray models = root["models"].toArray();

        // 2. 收集原始数据（不创建物理对象）—— 将 JSON 拍平为 vector<QJsonObject>
        struct RawEntry
        {
            int category;
            QJsonObject obj;
        };

        std::vector<RawEntry> entries;
        entries.reserve(models.size());
        for (const QJsonValueConstRef& val : models) {
            if (!val.isObject()) continue;
            const QJsonObject obj = val.toObject();
            entries.push_back({obj.value("category").toInt(0), obj});
        }

        SceneRecord record;
        // 字符串 JSON ID → entity 指针（用于约束的 entity_ids 解析）
        std::unordered_map<std::uint64_t, PhysicalEntity*> strIdToEntity;

        std::unordered_set<std::uint64_t> usedIds;

        // ================================================================
        //  第一遍 — Shape（category = 0）
        // ================================================================
        for (const RawEntry& entry : entries) {
            if (entry.category != 0) continue;
            const QJsonObject& obj = entry.obj;

            const std::uint32_t typeId = static_cast<std::uint32_t>(obj["type"].toInteger());
            ShapeLoadInfo shapeInfo;
            fillShapeLoadInfo(obj, typeId, shapeInfo);

            std::unique_ptr<ShapeBase> shape = createShapeFromLoadInfo(shapeInfo);
            if (!shape) {
                LOG_WARN("Shape 创建失败");
                LOG_WARN(obj["name"].toString().toStdString());
                continue;
            }

            const std::uint64_t bodyId = obj["id"].toString().toULongLong();
            auto rb = std::make_shared<RigidBody>(
                readVec3(obj["pos"]), readVec3(obj["initial_velocity"]),
                obj["mass"].toDouble(1.0), bodyId);

            rb->setRotation(readQuat(obj["rotation"]));
            rb->setScale(readVec3(obj["scale"], {1, 1, 1}));
            rb->setRestitution(obj.contains("restitution") ? obj["restitution"].toDouble() : 1.0);
            rb->setAngular(readVec3(obj["initial_angular_velocity"]));
            rb->setShape(std::shared_ptr<ShapeBase>(std::move(shape)));

            BodyRecord body;
            body.name = obj["name"].toString().toStdString();
            body.entity = std::move(rb);
            const QString sid = obj["id"].toString();
            if (!sid.isEmpty()) {
                strIdToEntity[sid.toULongLong()] = body.entity.get();
            };
            record.bodies.push_back(std::move(body));
        }

        // ================================================================
        //  第二遍 — Field（category = 1）
        // ================================================================
        for (const RawEntry& entry : entries) {
            if (entry.category != 1) continue;
            const QJsonObject& obj = entry.obj;
            const std::uint32_t ftype = static_cast<std::uint32_t>(obj["type"].toInteger());
            const QJsonObject fd = obj.value("field_data").toObject();

            if (ftype == 1) {
                // Gravity
                const std::uint64_t fid = obj["id"].toString().toULongLong();
                auto field = std::make_shared<GravityField>(
                    readVec3(fd["gravity"], {0, 0, -9.81}),
                    readVec3(fd["reference_point"]), 0);
                field->setId(fid);
                record.fields.push_back({obj["name"].toString().toStdString(), std::move(field)});
            }
            else if (ftype == 2) {
                // Gravitation
                const std::uint64_t fid = obj["id"].toString().toULongLong();
                auto gf = std::make_shared<GravitationalField>(
                    readVec3(obj["pos"]),
                    readVec3(fd["speed"]),
                    fd["mass"].toDouble(0),
                    fd.contains("coupling") ? fd["coupling"].toDouble() : G,
                    fd["fixed"].toBool(false), 0);
                gf->setId(fid);

                BodyRecord body;
                body.name = obj["name"].toString().toStdString();
                body.entity = std::static_pointer_cast<PhysicalEntity>(gf);
                body.entity->setShape(std::make_unique<ShapeSphere>(10.0));
                record.bodies.push_back(std::move(body));

                FieldRecord fr;
                fr.name = obj["name"].toString().toStdString();
                fr.field = std::static_pointer_cast<FieldBase>(gf);
                record.fields.push_back(std::move(fr));

                const QString sidStr = obj["id"].toString();
                if (!sidStr.isEmpty()) strIdToEntity[sidStr.toULongLong()] = record.bodies.back().entity.get();
            }
        }

        // ================================================================
        //  第三遍 — Constraint（category = 2）
        // ================================================================
        for (const RawEntry& entry : entries) {
            if (entry.category != 2) continue;
            const QJsonObject& obj = entry.obj;
            const std::uint32_t ctype = static_cast<std::uint32_t>(obj["type"].toInteger());
            const QJsonObject cd = obj.value("constraint_data").toObject();
            const QJsonArray eids = obj.value("entity_ids").toArray();

            // qDebug() << eids;
            // qDebug() << ctype;
            // qDebug() << entry.category << entry.obj;
            const std::uint64_t cid = obj["id"].toString().toULongLong();

            // ConstraintType: ConnectingLine=1, SimplePendulum=2, ConnectingRod=3,
            //                 Spring=5, AirResistance=7
            if (ctype == magic_enum::enum_integer(ConstraintType::ConnectingLine) && eids.size() >= 2) {
                // ConnectingLine
                auto* a = strIdToEntity[eids[0].toString().toULongLong()];
                auto* b = strIdToEntity[eids[1].toString().toULongLong()];
                if (a && b) {
                    auto c = std::make_shared<ConnectingLine>(cd["length"].toDouble(), a, b, cid);
                    record.constraints.push_back({obj["name"].toString().toStdString(), std::move(c)});
                }
                // else {
                //     qDebug() << a << " " << b << " " << cid;
                //     qDebug() << "Invalid Pointer";
                // }
            }
            else if (ctype == magic_enum::enum_integer(ConstraintType::SimplePendulum) && !eids.empty()) {
                // SimplePendulum
                auto* body = strIdToEntity[eids[0].toString().toULongLong()];
                if (body) {
                    auto c = std::make_shared<SimplePendulum>(
                        cd["length"].toDouble(),
                        readVec3(cd["anchor_pos"]), body, cid);
                    record.constraints.push_back({obj["name"].toString().toStdString(), std::move(c)});
                }
            }
            else if (ctype == magic_enum::enum_integer(ConstraintType::ConnectingRod) && eids.size() >= 2) {
                // ConnectingRod
                auto* a = strIdToEntity[eids[0].toString().toULongLong()];
                auto* b = strIdToEntity[eids[1].toString().toULongLong()];
                if (a && b) {
                    auto c = std::make_shared<ConnectingRod>(cd["length"].toDouble(), a, b, cid);
                    record.constraints.push_back({obj["name"].toString().toStdString(), std::move(c)});
                }
            }
            else if (ctype == magic_enum::enum_integer(ConstraintType::Spring) && eids.size() >= 2) {
                // Spring
                auto* a = strIdToEntity[eids[0].toString().toULongLong()];
                auto* b = strIdToEntity[eids[1].toString().toULongLong()];
                if (a && b) {
                    auto c = std::make_shared<Spring>(
                        cd["stiffness"].toDouble(), cd["rest_length"].toDouble(), a, b, cid);
                    record.constraints.push_back({obj["name"].toString().toStdString(), std::move(c)});
                }
            }
            else if (ctype == magic_enum::enum_integer(ConstraintType::AirResistance)) {
                // AirResistance
                auto constraint = std::make_shared<AirResistance>(0);
                constraint->setId(cid);
                if (cd.contains("drag_config")) {
                    constraint->setDefaultDragConfig(parseDragConfig(cd["drag_config"].toObject()));
                }
                else if (cd.contains("__sub_type_id")) {
                    // 回退：解析 QML 编辑器序列化的 sub_* 格式
                    DragConfig cfg;
                    const int subType = cd["__sub_type_id"].toInt();
                    if (subType == 0) {
                        cfg.model = DragModel::Linear;
                        if (cd.contains("sub_linear_coeff"))
                            cfg.linearCoeff = cd["sub_linear_coeff"].toDouble();
                    }
                    else if (subType == 1) {
                        cfg.model = DragModel::Quadratic;
                        if (cd.contains("sub_drag_coefficient"))
                            cfg.dragCoefficient = cd["sub_drag_coefficient"].toDouble();
                        if (cd.contains("sub_cross_section_area"))
                            cfg.crossSectionArea = cd["sub_cross_section_area"].toDouble();
                        if (cd.contains("sub_fluid_density"))
                            cfg.fluidDensity = cd["sub_fluid_density"].toDouble();
                    }
                    else if (subType == 2) {
                        cfg.model = DragModel::Constant;
                        if (cd.contains("sub_constant_magnitude"))
                            cfg.constantMagnitude = cd["sub_constant_magnitude"].toDouble();
                    }
                    constraint->setDefaultDragConfig(cfg);
                }

                if (cd.contains("entity_overrides") && cd["entity_overrides"].isArray()) {
                    for (const QJsonValueConstRef& ov : cd["entity_overrides"].toArray()) {
                        if (!ov.isObject()) continue;
                        const QJsonObject eo = ov.toObject();
                        auto* pe = strIdToEntity[eo["entity_id"].toString().toULongLong()];
                        if (auto* rb = dynamic_cast<RigidBody*>(pe))
                            constraint->addEntity(rb, parseDragConfig(eo));
                    }
                }
                record.constraints.push_back({obj["name"].toString().toStdString(), std::move(constraint)});
            }
        }

        return record;
    }

    SceneRecord SceneManagerViewModel::createSceneFromJsonFile(const QString& fileName)
    {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        const QByteArray data = file.readAll();
        file.close();
        return createSceneFromJsonString(QString::fromUtf8(data));
    }

    void SceneManagerViewModel::initialize()
    {
        SolverConfig config{};

        auto solver = std::make_unique<Solver>(
            config,
            std::make_unique<VelocityVerletIntegratorStrategy>(),
            std::make_unique<OctreeBroadStrategy>(),
            std::make_unique<FrictionalContactResolverStrategy>());

        // 挂载休眠策略：解决实体受恒定外力时无法自然静止的问题
        solver->setSleepStrategy(std::make_unique<KineticSleepStrategy>(m_sleepConfig));

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

    // bool SceneManagerViewModel::isDragActive() const
    // {
    //     return m_dragActive;
    // }

    SceneBodyModel* SceneManagerViewModel::bodyModel()
    {
        return &m_bodyModel;
    }

    SceneConstraintModel* SceneManagerViewModel::constraintModel()
    {
        return &m_constraintModel;
    }


    SceneCurveModel* SceneManagerViewModel::curveModel()
    {
        return &m_curveModel;
    }

    SceneConvexMeshModel* SceneManagerViewModel::convexMeshModel()
    {
        return &m_convexMeshModel;
    }

    QString SceneManagerViewModel::getIntegratorName() const
    {
        const std::string_view name{m_world->getIntegratorName()};
        return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
    }

    QString SceneManagerViewModel::getBroadStrategyName() const
    {
        const std::string_view name{m_world->getBroadStrategyName()};
        return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
    }

    QString SceneManagerViewModel::getSleepStrategyName() const
    {
        if (m_world == nullptr) {
            return {};
        }

        SleepStrategy* const strategy = m_world->getSleepStrategy();
        if (strategy == nullptr) {
            return {};
        }

        const std::string_view name{strategy->name()};
        return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
    }

    void SceneManagerViewModel::setSleepEnabled(const bool enabled)
    {
        m_sleepConfig.enabled = enabled;
        if (m_world != nullptr) {
            if (SleepStrategy* const strategy = m_world->getSleepStrategy(); strategy != nullptr) {
                strategy->setConfig(m_sleepConfig);
            }
        }
    }

    void SceneManagerViewModel::setWakeUpForceRatio(const double ratio)
    {
        if (ratio <= 0.0 || ratio > 1.0) {
            qWarning() << "setWakeUpForceRatio: 无效比例" << ratio << "，必须在 (0, 1] 范围内";
            return;
        }
        m_sleepConfig.wakeUpForceRatio = ratio;
        if (m_world != nullptr) {
            if (SleepStrategy* const strategy = m_world->getSleepStrategy(); strategy != nullptr) {
                strategy->setConfig(m_sleepConfig);
            }
        }
    }

    void SceneManagerViewModel::setSleepMetricType(const int type)
    {
        if (type < 0 || type > 2) {
            qWarning() << "setSleepMetricType: 无效指标类型" << type << "，可选 0=速度平方 1=动能 2=动量";
            return;
        }
        m_sleepConfig.metricType = static_cast<SleepMetricType>(type);
        if (m_world != nullptr) {
            if (SleepStrategy* const strategy = m_world->getSleepStrategy(); strategy != nullptr) {
                strategy->setConfig(m_sleepConfig);
            }
        }
    }

    void SceneManagerViewModel::setSleepThreshold(const double threshold)
    {
        if (threshold < 0.0) {
            qWarning() << "setSleepThreshold: 阈值不能为负" << threshold;
            return;
        }

        switch (m_sleepConfig.metricType) {
        case SleepMetricType::SpeedSquared:
            m_sleepConfig.speedSquaredThreshold = threshold;
            break;
        case SleepMetricType::KineticEnergy:
            m_sleepConfig.kineticEnergyThreshold = threshold;
            break;
        case SleepMetricType::Momentum:
            m_sleepConfig.momentumThreshold = threshold;
            break;
        }

        if (m_world != nullptr) {
            if (SleepStrategy* const strategy = m_world->getSleepStrategy(); strategy != nullptr) {
                strategy->setConfig(m_sleepConfig);
            }
        }
    }

    void SceneManagerViewModel::setSleepFrozenFrameCount(const int frameCount)
    {
        if (frameCount <= 0) {
            qWarning() << "setSleepFrozenFrameCount: 帧数必须为正" << frameCount;
            return;
        }
        m_sleepConfig.frozenFrameCount = static_cast<std::uint32_t>(frameCount);
        if (m_world != nullptr) {
            if (SleepStrategy* const strategy = m_world->getSleepStrategy(); strategy != nullptr) {
                strategy->setConfig(m_sleepConfig);
            }
        }
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
        // endBodyDrag();
        initialize();
        rebuildDemoWorld();
        refreshBodyModel();
        refreshConstraintModel();
        refreshCurveModel();
        refreshConvexMeshModel();

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
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_WARN("loadSceneFronJsonFile: 无法打开文件 %s");
            LOG_WARN(fileName.toStdString());
            return;
        }
        const QByteArray data = file.readAll();
        file.close();
        loadSceneFromJsonString(QString::fromUtf8(data));
        // 场景来自文件：把来源描述细化到文件名，供离线仿真面板提示初始状态
        setSceneSourceLabel(QStringLiteral("场景文件：%1").arg(QFileInfo(fileName).fileName()));
    }

    void SceneManagerViewModel::loadSceneFromJsonString(const QString& jsonString)
    {
        if (m_world == nullptr) {
            LOG_WARN_LITERAL("loadSceneFromJsonString: WorldSceneManager 未初始化");
            return;
        }

        // 1. 解析 JSON → SceneRecord（创建物理对象）
        SceneRecord record = createSceneFromJsonString(jsonString);
        if (record.bodies.empty() && record.fields.empty() && record.constraints.empty()) {
            LOG_WARN_LITERAL("loadSceneFromJsonString: 场景解析结果为空");
            return;
        }

        // 2. 清空当前世界并注册新场景
        m_world->clear();
        m_world->registerScene(record);

        // qDebug() << record.fields.size();
        // qDebug() << record.constraints.size();
        //
        // qDebug() << record.fields[0].name.c_str() << record.constraints[0].name.c_str();
        // qDebug() << record.fields[0].field->getId() << magic_enum::enum_name<FieldType>(
        //     record.fields[0].field->getType());
        // qDebug() << dynamic_cast<GravityField*>(record.fields[0].field.get())->getGravity().norm();
        //
        // for (auto i: record.bodies) {
        //     qDebug() << i.name.c_str() << i.entity->getId();
        // }
        //
        // for (auto i: record.constraints) {
        //     qDebug() << i.name.c_str();
        //     qDebug() << i.constraint->getId();
        //     for (auto j: i.constraint->getConstrainedEntities()) {
        //         qDebug() << dynamic_cast<PhysicalEntity*>(j)->getId();
        //     }
        // }

        // 3. 重置仿真状态
        m_simTime = 0.0;
        m_stepCount = 0;
        m_accumulator = 0.0;
        m_entityCount = static_cast<int>(m_world->getBodyCount());

        // 记录场景 JSON，供离线求解器复用同一份场景描述
        setCurrentSceneJson(jsonString, QStringLiteral("已加载场景"));

        // 4. 刷新视图模型
        refreshBodyModel();
        refreshConstraintModel();
        refreshCurveModel();
        refreshConvexMeshModel();

        emit simTimeChanged();
        emit stepCountChanged();
        emit entityCountChanged();
        emit entityChanged();
    }

    QString SceneManagerViewModel::currentSceneJson() const
    {
        return m_currentSceneJson;
    }

    QString SceneManagerViewModel::sceneSourceLabel() const
    {
        return m_sceneSourceLabel;
    }

    void SceneManagerViewModel::setCurrentSceneJson(const QString& json, const QString& sourceLabel)
    {
        m_currentSceneJson = json;
        setSceneSourceLabel(sourceLabel);
    }

    void SceneManagerViewModel::setSceneSourceLabel(const QString& sourceLabel)
    {
        if (m_sceneSourceLabel == sourceLabel) {
            return;
        }
        m_sceneSourceLabel = sourceLabel;
        emit sceneSourceChanged();
    }

    QString SceneManagerViewModel::serializeWorldToSceneJson(int* skippedObjects) const
    {
        int skipped{0};
        QJsonArray models;

        if (m_world != nullptr) {
            // ---- 刚体（引力场同时出现在力场列表中，这里跳过，由力场遍统一输出） ----
            const std::vector<std::string> bodyNames{m_world->getBodyNames()};
            const std::vector<PhysicalEntity*> entities{m_world->getAllPhysicalEntities()};

            for (std::size_t index = 0; index < entities.size(); ++index) {
                PhysicalEntity* entity{entities[index]};
                if (entity == nullptr || dynamic_cast<FieldBase*>(entity) != nullptr) {
                    continue;
                }

                const std::string name{index < bodyNames.size() ? bodyNames[index] : std::string{}};
                if (appendBodyJson(&models, entity, name)) {
                    continue;
                }

                ++skipped;
                LOG_INFO(std::format("离线仿真快照：刚体 {} 的形状无法写入场景 JSON，已跳过（不影响实时仿真）",
                                     entity->getId()));
            }

            // ---- 力场 ----
            for (FieldBase* field : m_world->getAllFields()) {
                if (field == nullptr) {
                    continue;
                }

                const std::string name{"field-" + std::to_string(field->getId())};
                if (appendFieldJson(&models, field, name)) {
                    continue;
                }

                ++skipped;
                LOG_INFO(std::format("离线仿真快照：力场 {} 无法写入场景 JSON，已跳过（不影响实时仿真）",
                                     field->getId()));
            }

            // ---- 约束：场景 JSON 方言暂未定义约束参数的序列化形式 ----
            const auto constraintCount{static_cast<int>(m_world->getAllConstraints().size())};
            if (constraintCount > 0) {
                skipped += constraintCount;
                LOG_INFO(std::format("离线仿真快照：{} 个约束无法写入场景 JSON，离线仿真将不含约束", constraintCount));
            }
        }

        if (skippedObjects != nullptr) {
            *skippedObjects = skipped;
        }

        QJsonObject root;
        root["models"] = models;
        return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
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
        updateFps(realFrameSeconds);

        // 播放模式：已加载仿真文件时不再求解，只按真实时间把播放时钟推进到对应帧。
        if (m_simulationReader != nullptr) {
            advancePlayback(realFrameSeconds);
            return;
        }

        // 时间缩放：累加器按缩放后的虚拟时间推进，物理积分仍以 m_fixedStepSeconds 为准
        m_accumulator += realFrameSeconds * m_timeScale;

        int subStepCount = 0;
        while (m_accumulator >= m_fixedStepSeconds && subStepCount < m_maxSubSteps) {
            advanceFixedStep(false);
            m_accumulator -= m_fixedStepSeconds;
            ++subStepCount;
        }

        // 渲染帧结束：结算休眠策略在本渲染帧内的冻结计数
        if (m_world != nullptr) {
            m_world->endFrame();
        }

        // 修复死亡螺旋：累加器超限时钳制到 maxSubSteps*dt，
        // 保留溢出时间在下一帧继续消耗，而非丢弃导致物理时间永久落后。
        const double maxAccumulator = m_fixedStepSeconds * static_cast<double>(m_maxSubSteps);
        if (m_accumulator > maxAccumulator) {
            m_accumulator = maxAccumulator;
        }
    }

    // -----------------------------------------------------------------------
    //  离线仿真文件播放
    // -----------------------------------------------------------------------

    bool SceneManagerViewModel::isSimulationLoaded() const
    {
        return m_simulationReader != nullptr;
    }

    QString SceneManagerViewModel::simulationFile() const
    {
        return m_simulationFile;
    }

    QString SceneManagerViewModel::simulationError() const
    {
        return m_simulationError;
    }

    int SceneManagerViewModel::simulationFrameCount() const
    {
        return m_simulationReader == nullptr ? 0 : static_cast<int>(m_simulationReader->frameCount());
    }

    double SceneManagerViewModel::simulationSampleIntervalMs() const
    {
        return m_simulationReader == nullptr
                   ? 0.0
                   : static_cast<double>(m_simulationReader->sampleIntervalNs()) / 1.0e6;
    }

    double SceneManagerViewModel::simulationDurationSeconds() const
    {
        if (m_simulationReader == nullptr) {
            return 0.0;
        }
        return static_cast<double>(simulationFrameCount()) * simulationSampleIntervalMs() / 1000.0;
    }

    int SceneManagerViewModel::playbackFrame() const
    {
        return m_playbackFrame;
    }

    double SceneManagerViewModel::playbackTimeSeconds() const
    {
        return m_playbackTime;
    }

    bool SceneManagerViewModel::isPlaybackPlaying() const
    {
        return m_playbackPlaying;
    }

    double SceneManagerViewModel::playbackRate() const
    {
        return m_playbackRate;
    }

    void SceneManagerViewModel::setPlaybackRate(const double rate)
    {
        const double clamped = std::clamp(rate, 0.01, 100.0);
        if (qFuzzyCompare(clamped, m_playbackRate)) {
            return;
        }
        m_playbackRate = clamped;
        emit playbackRateChanged();
    }

    bool SceneManagerViewModel::openSimulationFile(const QString& fileName)
    {
        auto reader = std::make_unique<eps::EpsReader>(fileName);
        QString error;
        if (!reader->open(&error)) {
            m_simulationError = error;
            emit simulationChanged();
            return false;
        }

        // 用场景描述块重建场景；之后每一帧只需把记录的状态套用到实体上。
        loadSceneFromJsonString(QString::fromStdString(reader->sceneJson()));
        // 场景来自仿真文件：来源描述标明是回放文件内嵌的场景
        setSceneSourceLabel(QStringLiteral("仿真文件：%1").arg(QFileInfo(m_simulationFile).fileName()));
        if (m_world == nullptr || m_world->getBodyCount() == 0) {
            m_simulationError = QStringLiteral("仿真文件的场景描述块无法重建场景");
            emit simulationChanged();
            return false;
        }

        m_simulationReader = std::move(reader);
        m_simulationFile = fileName;
        m_simulationError.clear();
        m_playbackPlaying = false;
        m_playbackFrame = 0;
        m_playbackTime = 0.0;

        applySimulationFrame(0);
        emit simulationChanged();
        emit playbackStateChanged();
        return true;
    }

    bool SceneManagerViewModel::openDefaultSimulationFile()
    {
        return openSimulationFile(OfflineSimulationRunner::defaultSimulationFilePath());
    }

    void SceneManagerViewModel::closeSimulationFile()
    {
        if (m_simulationReader == nullptr) {
            return;
        }

        m_simulationReader.reset();
        m_simulationFile.clear();
        m_playbackPlaying = false;
        m_playbackFrame = 0;
        m_playbackTime = 0.0;
        m_accumulator = 0.0;
        emit simulationChanged();
        emit playbackStateChanged();
    }

    void SceneManagerViewModel::setPlaybackFrame(const int frame)
    {
        if (m_simulationReader == nullptr) {
            return;
        }

        const int total = simulationFrameCount();
        const int clamped = std::clamp(frame, 0, total > 0 ? total - 1 : 0);
        m_playbackFrame = clamped;
        m_playbackTime = static_cast<double>(clamped) * simulationSampleIntervalMs() / 1000.0;

        applySimulationFrame(clamped);
        emit playbackStateChanged();
    }

    void SceneManagerViewModel::setPlaybackTime(const double seconds)
    {
        const double interval = simulationSampleIntervalMs() / 1000.0;
        if (m_simulationReader == nullptr || interval <= 0.0) {
            return;
        }
        setPlaybackFrame(static_cast<int>(seconds / interval));
    }

    void SceneManagerViewModel::stepPlaybackFrame(const int delta)
    {
        setPlaybackFrame(m_playbackFrame + delta);
    }

    void SceneManagerViewModel::playSimulation()
    {
        if (m_simulationReader == nullptr || m_playbackPlaying) {
            return;
        }
        m_playbackPlaying = true;
        emit playbackStateChanged();
    }

    void SceneManagerViewModel::pauseSimulation()
    {
        if (!m_playbackPlaying) {
            return;
        }
        m_playbackPlaying = false;
        emit playbackStateChanged();
    }

    void SceneManagerViewModel::toggleSimulationPlayback()
    {
        if (m_playbackPlaying) {
            pauseSimulation();
        }
        else {
            playSimulation();
        }
    }

    void SceneManagerViewModel::advancePlayback(const double realFrameSeconds)
    {
        if (!m_playbackPlaying || m_simulationReader == nullptr) {
            return;
        }

        m_playbackTime += realFrameSeconds * m_playbackRate;

        const double interval = simulationSampleIntervalMs() / 1000.0;
        if (interval <= 0.0) {
            return;
        }

        const int target = static_cast<int>(m_playbackTime / interval);
        if (target >= simulationFrameCount()) {
            setPlaybackFrame(simulationFrameCount() - 1);
            pauseSimulation();
            return;
        }

        if (target != m_playbackFrame) {
            m_playbackFrame = target;
            applySimulationFrame(target);
            emit playbackStateChanged();
        }
    }

    bool SceneManagerViewModel::applySimulationFrame(const int frame)
    {
        if (m_simulationReader == nullptr || m_world == nullptr) {
            return false;
        }

        eps::FrameState state{};
        QString error;
        if (!m_simulationReader->readFrame(static_cast<std::uint64_t>(std::max(0, frame)), &state, &error)) {
            m_simulationError = error;
            emit simulationChanged();
            return false;
        }

        const std::vector<PhysicalEntity*> entities = m_world->getAllPhysicalEntities();
        for (const eps::BodyState& body : state.bodies) {
            if (body.slot >= entities.size()) {
                continue;
            }
            PhysicalEntity* entity = entities[body.slot];
            if (entity == nullptr) {
                continue;
            }
            entity->setPosition(body.position);
            entity->setRotation(body.rotation);
            entity->setSpeed(body.linearVelocity);
            entity->setAngular(body.angularVelocity);
        }

        m_simTime = static_cast<double>(state.simTimeNs) / 1.0e9;
        m_stepCount = state.stepIndex;

        refreshBodyModel();
        refreshConstraintModel();
        refreshCurveModel();
        refreshConvexMeshModel();
        emit simTimeChanged();
        emit stepCountChanged();
        emit entityChanged();
        return true;
    }

    void SceneManagerViewModel::rebuildDemoWorld()
    {
        if (m_world == nullptr) {
            LOG_FATAL("SceneManagerViewModel::rebuildDemoWorld(): WorldSceneManager is nullptr");
            return;
        }

        // loadSceneFronJsonFile("C:/Users/jehor/Desktop/114514.json");
        // return;


        // qDebug() << QDir::currentPath();
        //
        // auto convexMesh = std::make_unique<ConvexMesh>(makeTessellatedMesh("../tools/sphere.obj"));
        //
        // //
        // if (convexMesh == nullptr) {
        //     LOG_FATAL("convexMesh is nullptr");
        // }else {
        //     qDebug() << convexMesh->getVolume();
        //     for (auto ver: convexMesh->getTessellatedMesh().indices) {
        //         qDebug() << ver.x() << " " << ver.y() << " " << ver.z();
        //     }
        // }
        //
        //
        // TessellatedMesh mesh;
        //
        // double width = 200;
        // double height = 100;
        // double depth = 100;
        //
        // // 尺寸参数：宽(X轴) = width, 高(Y轴) = height, 深(Z轴) = depth
        // // 8个顶点坐标（世界空间）
        // std::vector<Eigen::Vector3d> vertices = {
        //     // 前面4个顶点 (z = -depth/2)
        //     {-width / 2, -height / 2, -depth / 2}, // 0: 前下左
        //     {width / 2, -height / 2, -depth / 2}, // 1: 前下右
        //     {width / 2, height / 2, -depth / 2}, // 2: 前上右
        //     {-width / 2, height / 2, -depth / 2}, // 3: 前上左
        //
        //     // 后面4个顶点 (z = depth/2)
        //     {-width / 2, -height / 2, depth / 2}, // 4: 后下左
        //     {width / 2, -height / 2, depth / 2}, // 5: 后下右
        //     {width / 2, height / 2, depth / 2}, // 6: 后上右
        //     {-width / 2, height / 2, depth / 2} // 7: 后上左
        // };
        //
        // mesh.vertices = vertices;
        //
        // // 计算法线（每个顶点法线为单位向量）
        // // 对于长方体，每个顶点被3个面共享，需要平均法线并归一化
        // std::vector<Eigen::Vector3d> normals(8);
        //
        // // 归一化的面法线
        // Eigen::Vector3d frontNorm(0, 0, -1);
        // Eigen::Vector3d backNorm(0, 0, 1);
        // Eigen::Vector3d leftNorm(-1, 0, 0);
        // Eigen::Vector3d rightNorm(1, 0, 0);
        // Eigen::Vector3d bottomNorm(0, -1, 0);
        // Eigen::Vector3d topNorm(0, 1, 0);
        //
        // // 累加每个顶点相邻的面法线
        // // 顶点0: 前、左、下
        // normals[0] = (frontNorm + leftNorm + bottomNorm).normalized();
        // // 顶点1: 前、右、下
        // normals[1] = (frontNorm + rightNorm + bottomNorm).normalized();
        // // 顶点2: 前、右、上
        // normals[2] = (frontNorm + rightNorm + topNorm).normalized();
        // // 顶点3: 前、左、上
        // normals[3] = (frontNorm + leftNorm + topNorm).normalized();
        // // 顶点4: 后、左、下
        // normals[4] = (backNorm + leftNorm + bottomNorm).normalized();
        // // 顶点5: 后、右、下
        // normals[5] = (backNorm + rightNorm + bottomNorm).normalized();
        // // 顶点6: 后、右、上
        // normals[6] = (backNorm + rightNorm + topNorm).normalized();
        // // 顶点7: 后、左、上
        // normals[7] = (backNorm + leftNorm + topNorm).normalized();
        //
        // mesh.normals = normals;
        //
        // // 12个三角形（每个矩形面由2个三角形组成）
        // // 索引格式：每个三角形按逆时针顺序（从外部看）
        // std::vector<TessellatedMesh::TriangleIndexType> indices;
        //
        // // 前面 (z = -depth/2)
        // indices.push_back({0, 1, 2});
        // indices.push_back({0, 2, 3});
        //
        // // 后面 (z = depth/2)
        // indices.push_back({5, 4, 7});
        // indices.push_back({5, 7, 6});
        //
        // // 左面 (x = -width/2)
        // indices.push_back({4, 0, 3});
        // indices.push_back({4, 3, 7});
        //
        // // 右面 (x = width/2)
        // indices.push_back({1, 5, 6});
        // indices.push_back({1, 6, 2});
        //
        // // 下面 (y = -height/2)
        // indices.push_back({4, 5, 1});
        // indices.push_back({4, 1, 0});
        //
        // // 上面 (y = height/2)
        // indices.push_back({3, 2, 6});
        // indices.push_back({3, 6, 7});
        //
        // mesh.indices = indices;

        auto mesh{makeTessellatedMesh("../tools/icosahedron.obj")};
        if (mesh.vertices.empty()) {
            // 相对路径依赖进程工作目录：加载失败时直接跳过示例体，
            // 否则空网格会在后续求解 / 渲染阶段造成访问冲突。
            LOG_WARN_LITERAL("rebuildDemoWorld: 无法加载 ../tools/icosahedron.obj，已跳过凸体网格示例（请从构建目录启动程序）");
        }
        else {
            AABB ab = mesh.getAABB();
            LOG_DEBUG(std::format("AABB: ({}, {}, {}), ({}, {}, {})", ab.min.x(), ab.min.y(), ab.min.z(), ab.max.x(), ab.max.y(), ab.max.z()));
        }


        auto shapeTest{std::make_unique<ShapeBox>(100, 100, 100)};


        if (!mesh.vertices.empty()) {
            m_world->spawnBody("测试样例1", {-400, -400, 400}, {0, 0, 0},
                               std::move(std::make_unique<ConvexMesh>(std::move(mesh))), 1.0);
        }
        // m_world->spawnBody("测试样例1", {-400, -400, 400}, {0, 0, 0}, std::move(std::move(shapeTest)), 1.0);

        std::uint64_t idBox2 = m_world->spawnBox("3", {0, 0, 0}, {0, 0, 0}, {10000, 10000, 130}, 0);


        m_world->spawnBox("1", {300, 400, 500}, {0, 0, 0}, {100, 100, 100}, 10);
        m_world->spawnSphere("1123", {110, 0, 300}, {0, 0, 0}, 10, 10);
        m_world->spawnSphere("1223", {110, 10, 320}, {0, 0, 0}, 10, 10);
        m_world->spawnSphere("1323", {110, 20, 340}, {0, 0, 0}, 10, 10);
        m_world->spawnSphere("1423", {110, 30, 360}, {0, 0, 0}, 10, 10);
        m_world->spawnSphere("1523", {110, 40, 380}, {0, 0, 0}, 10, 10);
        m_world->spawnSphere("1623", {110, 50, 400}, {0, 0, 0}, 10, 10);

        m_world->spawnCylinder("cylinder", {-100, -100, 400}, {0, 0, 0}, 100, 200, 10);
        m_world->addGravityField({0.0, 0.0, -180.0}, {0.0, 0.0, 0.0}, "重力场");
        // std::uint64_t id1 = m_world->spawnSphere("123", {0, 0, 300}, {0, 0, 0}, 10, 10);
        // std::uint64_t id2 = m_world->spawnSphere("456", {0, 0, 400}, {0, 0, 0}, 10, 0);
        // m_world->createSpring("MySpring", 50.0, 100.0, id2, id1);
        //
        //
        // std::uint64_t id4 = m_world->spawnSphere("456", {0, 100, 800}, {0, 0, 0}, 10, 5);
        // m_world->createSimplePendulum("", 100, {0, 0, 800}, id4);
        //
        //
        // std::uint64_t id5 = m_world->spawnSphere("阻力测试", {300, 200, 400}, {0, 200, 0}, 10, 100);
        // auto airResistance = std::make_unique<AirResistance>(
        //     UniqueIDGenerator::instance().generate());
        // auto body = dynamic_cast<RigidBody*>(m_world->getEntity(id5));
        //
        // if (body == nullptr) {
        //     LOG_FATAL("SceneManagerViewModel::rebuildDemoWorld(): body is nullptr");
        //     return;
        // }

        // airResistance->addEntity(body, DragConfig{
        //                              .model = DragModel::Quadratic,
        //                              .dragCoefficient = 0.47,
        //                          });
        //
        //
        // std::uint64_t id6 = m_world->spawnSphere("123", {-100, 100, 300}, {0, 0, 0}, 10, 10);
        // std::uint64_t id7 = m_world->spawnSphere("456", {-100, 100, 400}, {0, 0, 0}, 10, 0);
        // m_world->createSpring("MySpring", 50.0, 100.0, id6, id7);
        // auto body2 = dynamic_cast<RigidBody*>(m_world->getEntity(id6));
        // airResistance->addEntity(body2, DragConfig{
        //                              .model = DragModel::Quadratic,
        //                              .dragCoefficient = 0.07,
        //                          });
        //
        //
        // m_world->registerConstraint("Air Resistance", std::move(airResistance));


        // auto curve{std::make_unique<CurveHelix>(10, 10, 1000)};
        // auto rail{std::make_unique<SlidingRail>(UniqueIDGenerator::instance().generate(), std::move(curve))};

        // std::uint64_t id10 = m_world->spawnSphere("45623", {-100, -400, 700}, {0, 0, 0}, 10, 10);

        //rail->registerEntity(m_world->getEntity(id10));

         //m_world->registerConstraint("Sliding Rail", std::move(rail));

        m_entityCount = static_cast<int>(m_world->getBodyCount());

        // 预设场景的 t=0 状态即离线仿真的初始状态：此处立刻快照一份场景 JSON，
        // 使「离线仿真」面板无需用户先加载场景文件即可求解预设场景。
        int skippedObjects{0};
        const QString snapshot{serializeWorldToSceneJson(&skippedObjects)};
        setCurrentSceneJson(snapshot,
                            skippedObjects > 0
                                ? QStringLiteral("内置预设场景（%1 个对象无法序列化，已忽略）").arg(skippedObjects)
                                : QStringLiteral("内置预设场景"));
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
            item.id = renderItem.basic.id;
            item.kind = renderItem.basic.kind;
            item.name = QString::fromStdString(renderItem.basic.name);

            item.velocity[0] = static_cast<float>(renderItem.velocity.x());
            item.velocity[1] = static_cast<float>(renderItem.velocity.y());
            item.velocity[2] = static_cast<float>(renderItem.velocity.z());

            item.angularVelocity[0] = static_cast<float>(renderItem.angularVelocity.x());
            item.angularVelocity[1] = static_cast<float>(renderItem.angularVelocity.y());
            item.angularVelocity[2] = static_cast<float>(renderItem.angularVelocity.z());

            item.kineticEnergy = renderItem.kineticEnergy;

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

            item.color = QString::fromStdString(renderItem.basic.color);
            item.label = QString::fromStdString(renderItem.basic.label);

            // ---- 追踪数据采集 ----
            const auto trackIt = m_trackingBuffers.find(renderItem.basic.id);
            if (trackIt != m_trackingBuffers.end() && trackIt->active && !trackIt->paused) {
                TrackingSnapshot snap;
                snap.simTime = m_simTime;
                snap.position = item.centerPos;
                snap.velocity = item.velocity;
                snap.kineticEnergy = item.kineticEnergy;

                trackIt->snapshots.push_back(snap);
                if (trackIt->snapshots.size() > std::numeric_limits<std::uint32_t>::max()) {
                    trackIt->snapshots.pop_front();
                }
            }

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
            item.basic.id = renderItem.basic.id;
            item.basic.kind = renderItem.basic.kind;
            item.basic.name = QString::fromStdString(renderItem.basic.name);

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

            item.basic.color = QString::fromStdString(renderItem.basic.color);
            item.basic.label = QString::fromStdString(renderItem.basic.label);

            items.push_back(std::move(item));
        }

        m_constraintModel.setItems(items);
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

    void SceneManagerViewModel::refreshConvexMeshModel()
    {
        if (m_world == nullptr) {
            m_convexMeshModel.clear();
            return;
        }

        const std::vector<SceneConvexMeshRenderItem> renderItems =
            m_world->buildConvexMeshRenderItems();
        m_convexMeshModel.setItems(renderItems);
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
        refreshCurveModel();
        refreshConvexMeshModel();
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

    //  数据追踪实现
    void SceneManagerViewModel::setBodyTracking(const QString& bodyId,
                                                const bool trackPosition,
                                                const bool trackVelocity,
                                                const bool trackAngularVelocity,
                                                const bool trackKineticEnergy)
    {
        bool ok = false;
        const std::uint64_t id = bodyId.toULongLong(&ok);
        if (!ok) return;

        auto buf = m_trackingBuffers.find(id);
        if (buf == m_trackingBuffers.end()) {
            return;
        }
        if (trackPosition || trackVelocity || trackAngularVelocity || trackKineticEnergy) {
            buf->active = true;
            std::uint32_t flags = 0;
            if (trackPosition) flags |= 1u << 0;
            if (trackVelocity) flags |= 1u << 1;
            if (trackAngularVelocity) flags |= 1u << 2;
            if (trackKineticEnergy) flags |= 1u << 3;
            buf->flags = flags;
        }
        else {
            buf->active = false;
            buf->flags = 0;
        }
    }

    int SceneManagerViewModel::getSnapshotCount(const QString& bodyId) const
    {
        bool ok = false;
        const std::uint64_t id = bodyId.toULongLong(&ok);
        if (!ok) return 0;

        const auto it = m_trackingBuffers.find(id);
        if (it == m_trackingBuffers.end() || !it->active) return 0;
        return static_cast<int>(it->snapshots.size());
    }

    void SceneManagerViewModel::ensureBodyTracking(const QString& bodyId)
    {
        bool ok = false;
        const std::uint64_t id = bodyId.toULongLong(&ok);
        if (!ok) return;

        auto& buf = m_trackingBuffers[id];
        if (!buf.active) {
            buf.active = true;
            // 仅在用户从未通过上下文菜单选择任何追踪项时，默认开启全部 4 项
            if (buf.flags == 0) {
                buf.flags = 0xF; // 位置 | 速度 | 角速度 | 动能
            }
        }
    }

    void SceneManagerViewModel::exportTrackingAsync(const QString& bodyId,
                                                    const QString& filePath)
    {
        bool ok = false;
        const std::uint64_t id = bodyId.toULongLong(&ok);
        if (!ok) return;

        const auto it = m_trackingBuffers.find(id);
        if (it == m_trackingBuffers.end() || !it->active) return;

        // 获取已打开的文件句柄
        const auto fileIt = m_recordingFiles.find(id);
        if (fileIt == m_recordingFiles.end() || !fileIt.value()) return;

        // 防止同一文件并发写入
        if (it->exporting.loadRelaxed()) return;
        it->exporting.storeRelaxed(1);

        // 拷贝快照（主线程只读）
        const std::vector<TrackingSnapshot> copy(it->snapshots.begin(),
                                                 it->snapshots.end());
        const std::uint32_t flags = it->flags;

        QAtomicInt* pExporting = &it->exporting;
        std::shared_ptr<QFile> pFile = fileIt.value();

        QXlsx::Format titleFormat;
        titleFormat.setFontBold(true);

        // 工作线程构建 xlsx → QBuffer；主线程写入已锁文件
        std::ignore = QtConcurrent::run([titleFormat, copy, flags, pExporting, pFile, this]
        {
            QXlsx::Document xlsx;
            int col = 0;

            constexpr double columnWidth = 15.0;

            // 表头
            xlsx.write(1, ++col, QStringLiteral("时间"), titleFormat);
            xlsx.setColumnWidth(col, columnWidth);
            xlsx.setColumnWidth(++col, columnWidth);

            if (flags & (1u << 0)) {
                xlsx.write(1, ++col, QStringLiteral("位置-x/m"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                xlsx.write(1, ++col, QStringLiteral("位置-y/m"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                xlsx.write(1, ++col, QStringLiteral("位置-z/m"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                xlsx.write(1, ++col, QStringLiteral("位置-标量/m"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                ++col;
                xlsx.setColumnWidth(col, columnWidth);
            }
            if (flags & (1u << 1)) {
                xlsx.write(1, ++col, QStringLiteral("速度-x/m·s⁻¹"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                xlsx.write(1, ++col, QStringLiteral("速度-y/m·s⁻¹"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                xlsx.write(1, ++col, QStringLiteral("速度-z/m·s⁻¹"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                xlsx.write(1, ++col, QStringLiteral("速度-标量/m·s⁻¹"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                ++col;
                xlsx.setColumnWidth(col, columnWidth);
            }
            if (flags & (1u << 2)) {
                xlsx.write(1, ++col, QStringLiteral("角速度-x/rad·s⁻¹"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                xlsx.write(1, ++col, QStringLiteral("角速度-y/rad·s⁻¹"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                xlsx.write(1, ++col, QStringLiteral("角速度-z/rad·s⁻¹"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                xlsx.write(1, ++col, QStringLiteral("角速度-标量/rad·s⁻¹"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                ++col;
                xlsx.setColumnWidth(col, columnWidth);
            }
            if (flags & (1u << 3)) {
                xlsx.write(1, ++col, QStringLiteral("动能/J"), titleFormat);
                xlsx.setColumnWidth(col, columnWidth);
                ++col;
                xlsx.setColumnWidth(col, columnWidth);
            }

            // 数据行
            int row = 1;
            for (const auto& snap : copy) {
                ++row;
                col = 0;
                xlsx.write(row, ++col, snap.simTime);

                ++col;
                if (flags & (1u << 0)) {
                    xlsx.write(row, ++col, snap.position[0]);
                    xlsx.write(row, ++col, snap.position[1]);
                    xlsx.write(row, ++col, snap.position[2]);
                    xlsx.write(row, ++col, sqrtSumOfSquares<double>(snap.position));
                    ++col;
                }

                if (flags & (1u << 1)) {
                    xlsx.write(row, ++col, snap.velocity[0]);
                    xlsx.write(row, ++col, snap.velocity[1]);
                    xlsx.write(row, ++col, snap.velocity[2]);
                    xlsx.write(row, ++col, sqrtSumOfSquares<double>(snap.velocity));
                    ++col;
                }

                if (flags & (1u << 2)) {
                    xlsx.write(row, ++col, snap.angularVelocity[0]);
                    xlsx.write(row, ++col, snap.angularVelocity[1]);
                    xlsx.write(row, ++col, snap.angularVelocity[2]);
                    xlsx.write(row, ++col, sqrtSumOfSquares<double>(snap.angularVelocity));
                    ++col;
                }

                if (flags & (1u << 3)) {
                    xlsx.write(row, ++col, snap.kineticEnergy);
                    ++col;
                }
            }

            // 保存到内存
            QBuffer buffer;
            buffer.open(QIODevice::WriteOnly);
            std::ignore = xlsx.saveAs(&buffer);
            const QByteArray data = buffer.data();

            // 回到主线程写入已锁文件
            QMetaObject::invokeMethod(this, [pFile, data, pExporting]
            {
                pFile->seek(0);
                pFile->write(data);
                pFile->resize(pFile->pos());
                pFile->flush();
                pExporting->storeRelaxed(0);
            }, Qt::QueuedConnection);
        });
    }

    bool SceneManagerViewModel::saveTrackingXlsx(const QString& bodyId,
                                                 const QString& filePath) const
    {
        qDebug() << filePath << bodyId;

        bool ok = false;
        const std::uint64_t id = bodyId.toULongLong(&ok);
        if (!ok) {
            qDebug() << "Not OK";
            return false;
        }

        const auto it = m_trackingBuffers.find(id);
        if (it == m_trackingBuffers.end() || !it->active || it->snapshots.empty()) {
            qDebug() << "Not OK: " << "it == m_trackingBuffers.end() || !it->active || it->snapshots.empty()";
            return false;
        }

        QXlsx::Document xlsx;
        const std::uint32_t f = it->flags;
        int col = 0;

        // 写入表头行
        xlsx.write(1, ++col, QStringLiteral("simTime"));
        if (f & (1u << 0)) {
            xlsx.write(1, ++col, QStringLiteral("pos_x"));
            xlsx.write(1, ++col, QStringLiteral("pos_y"));
            xlsx.write(1, ++col, QStringLiteral("pos_z"));
        }
        if (f & (1u << 1)) {
            xlsx.write(1, ++col, QStringLiteral("vel_x"));
            xlsx.write(1, ++col, QStringLiteral("vel_y"));
            xlsx.write(1, ++col, QStringLiteral("vel_z"));
        }
        if (f & (1u << 2)) {
            xlsx.write(1, ++col, QStringLiteral("ang_x"));
            xlsx.write(1, ++col, QStringLiteral("ang_y"));
            xlsx.write(1, ++col, QStringLiteral("ang_z"));
        }
        if (f & (1u << 3)) {
            xlsx.write(1, ++col, QStringLiteral("kineticEnergy"));
        }

        // 逐行写入快照数据
        int row = 1;
        for (const auto& snap : it->snapshots) {
            ++row;
            col = 0;
            xlsx.write(row, ++col, snap.simTime);
            if (f & (1u << 0)) {
                xlsx.write(row, ++col, snap.position[0]);
                xlsx.write(row, ++col, snap.position[1]);
                xlsx.write(row, ++col, snap.position[2]);
            }
            if (f & (1u << 1)) {
                xlsx.write(row, ++col, snap.velocity[0]);
                xlsx.write(row, ++col, snap.velocity[1]);
                xlsx.write(row, ++col, snap.velocity[2]);
            }
            if (f & (1u << 2)) {
                xlsx.write(row, ++col, snap.angularVelocity[0]);
                xlsx.write(row, ++col, snap.angularVelocity[1]);
                xlsx.write(row, ++col, snap.angularVelocity[2]);
            }
            if (f & (1u << 3)) {
                xlsx.write(row, ++col, snap.kineticEnergy);
            }
        }

        const bool status{xlsx.saveAs(filePath)};
        qDebug() << status;
        return status;
    }

    void SceneManagerViewModel::startRecording(const QString& bodyId,
                                               const QString& filePath)
    {
        bool ok = false;
        const std::uint64_t id = bodyId.toULongLong(&ok);
        if (!ok) return;

        // 激活追踪（幂等）
        ensureBodyTracking(bodyId);

        // 打开文件并保持句柄（Windows 下锁定文件，禁止外部打开）
        auto file = std::make_unique<QFile>(filePath);
        if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qWarning() << "录制：无法打开文件" << filePath;
            return;
        }
        m_recordingFiles[id] = std::move(file);
        m_recordingPaths[id] = filePath;

        // 启动定时刷新（幂等）
        if (!m_flushTimer.isActive()) {
            m_flushTimer.start();
        }
    }

    void SceneManagerViewModel::pauseRecording(const QString& bodyId)
    {
        bool ok = false;
        const std::uint64_t id = bodyId.toULongLong(&ok);
        if (!ok) return;

        auto it = m_trackingBuffers.find(id);
        if (it != m_trackingBuffers.end()) {
            it->paused = true;
        }
    }

    void SceneManagerViewModel::resumeRecording(const QString& bodyId)
    {
        bool ok = false;
        const std::uint64_t id = bodyId.toULongLong(&ok);
        if (!ok) return;

        auto it = m_trackingBuffers.find(id);
        if (it != m_trackingBuffers.end()) {
            it->paused = false;
        }
    }

    void SceneManagerViewModel::stopRecording(const QString& bodyId)
    {
        bool ok = false;
        const std::uint64_t id = bodyId.toULongLong(&ok);
        if (!ok) return;

        // 1. 最终刷新
        auto pathIt = m_recordingPaths.find(id);
        if (pathIt != m_recordingPaths.end()) {
            exportTrackingAsync(bodyId, pathIt.value());
            m_recordingPaths.erase(pathIt);
        }

        // 1.5 释放文件句柄（文件由异步写入回调中的 shared_ptr 持有，
        // 回调执行完毕后 QFile 析构函数自动关闭并释放）
        auto fileIt = m_recordingFiles.find(id);
        if (fileIt != m_recordingFiles.end() && fileIt.value()) {
            m_recordingFiles.erase(fileIt);
        }

        // 2. 清空缓冲区 + 停用追踪
        auto bufIt = m_trackingBuffers.find(id);
        if (bufIt != m_trackingBuffers.end()) {
            bufIt->snapshots.clear();
            bufIt->active = false;
            bufIt->paused = false;
            bufIt->flags = 0;
        }

        // 所有录制停止后关闭定时器
        if (m_recordingPaths.isEmpty()) {
            m_flushTimer.stop();
        }
    }

    void SceneManagerViewModel::flushRecordings()
    {
        for (auto it = m_recordingPaths.begin(); it != m_recordingPaths.end(); ++it) {
            // 将 id 转为字符串再调用已有方法
            exportTrackingAsync(QString::number(it.key()), it.value());
        }
    }

    // void SceneManagerViewModel::setDragActive(const bool active)
    // {
    //     if (m_dragActive == active) {
    //         return;
    //     }
    //     m_dragActive = active;
    //     emit dragActiveChanged();
    // }
}
