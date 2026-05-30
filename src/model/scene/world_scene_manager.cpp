/**
 * @file        world_scene_manager.cpp
 * @brief       世界场景管理器实现文件，定义物理世界的场景管理功能。
 * @details     实现 WorldSceneManager 类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-04
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Scene
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "world_scene_manager.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include <QVector3D>

#include "constants.h"
#include "gravity_field.h"
#include "field_base.h"
#include "transform_transformer.h"
#include "shape_factory_registry.h"
#include "shape_box.h"
#include "shape_cylinder.h"
#include "shape_sphere.h"
#include "simple_pendulum.h"
#include "uuid_generator.h"
#include "constraints/connecting_line.h"
#include "constraints/simple_pendulum.h"

namespace egret
{
    namespace
    {
        [[nodiscard]] double readShapeParamAsDouble(const ShapeLoadInfo& info, const std::string& key,
                                                    const double fallback)
        {
            // 优先从 numberParams 中查找
            const auto numIter = info.numberParams.find(key);
            if (numIter != info.numberParams.end() && !numIter->second.empty()) {
                // 返回第一个元素（假设存储的是单个值）
                return numIter->second.front();
            }

            // 未找到参数，返回 fallback
            return fallback;
        }

        [[nodiscard]] Eigen::Vector3d readShapeParamAsVector3(const ShapeLoadInfo& info,
                                                              const std::string& key,
                                                              const Eigen::Vector3d& fallback)
        {
            // 从 numberParams 中查找
            const auto numIter = info.numberParams.find(key);
            if (numIter != info.numberParams.end() && numIter->second.size() >= 3) {
                // 假设存储的是 [x, y, z] 三个双精度值
                return Eigen::Vector3d(numIter->second[0], numIter->second[1], numIter->second[2]);
            }

            // 未找到参数或数据不足，返回 fallback
            return fallback;
        }

    }

    WorldSceneManager::WorldSceneManager(std::unique_ptr<SolverBase> solver) : m_solver(std::move(solver))
    {
    }

    std::uint64_t WorldSceneManager::spawnBody(const std::string& name,
                                               const Eigen::Vector3d& position,
                                               const Eigen::Vector3d& speed,
                                               std::unique_ptr<ShapeBase> shape,
                                               const double mass)
    {
        if (shape == nullptr) {
            return 0;
        }

        auto record = std::make_unique<BodyRecord>();
        record->name = name;
        record->entity = std::make_shared<RigidBody>(position, speed, mass, UuidGenerator::instance().generate());

        record->entity->setShape(std::move(shape));
        // record->shape = std::move(shape);
        m_bodies.push_back(std::move(record));
        rebuildSolverCaches();
        return m_bodies.back()->entity->getId();
    }

    std::uint64_t WorldSceneManager::registerBody(const std::string& name,
                                                  const std::shared_ptr<PhysicalEntity>& entity,
                                                  std::unique_ptr<ShapeBase> shape)
    {
        if (entity == nullptr || shape == nullptr) {
            return 0;
        }

        auto record = std::make_unique<BodyRecord>();
        record->name = name;
        record->entity = unique_ptr_cast<PhysicalEntity, PhysicsAbstract>(std::move(entity->clone(UuidGenerator::instance().generate())));
        record->entity->setShape(std::move(shape));
        m_bodies.push_back(std::move(record));
        rebuildSolverCaches();
        return m_bodies.back()->entity->getId();
    }

    std::uint64_t WorldSceneManager::spawnBodyFromLoadInfo(const std::string& name,
                                                           const Eigen::Vector3d& position,
                                                           const Eigen::Vector3d& speed,
                                                           const ShapeLoadInfo& shapeInfo,
                                                           const double mass)
    {
        std::unique_ptr<ShapeBase> shape = ShapeFactoryRegistry::instance().create(shapeInfo);
        return spawnBody(name, position, speed, std::move(shape), mass);
    }

    std::uint64_t WorldSceneManager::spawnSphere(const std::string& name,
                                                 const Eigen::Vector3d& position,
                                                 const Eigen::Vector3d& speed,
                                                 const double radius,
                                                 const double mass)
    {
        return spawnBody(name, position, speed, std::make_unique<ShapeSphere>(radius), mass);
    }

    std::uint64_t WorldSceneManager::spawnBox(const std::string& name,
                                              const Eigen::Vector3d& position,
                                              const Eigen::Vector3d& speed,
                                              const Eigen::Vector3d& size,
                                              const double mass)
    {
        return spawnBody(name, position, speed, std::make_unique<ShapeBox>(size), mass);
    }

    std::uint64_t WorldSceneManager::spawnCylinder(const std::string& name,
                                                   const Eigen::Vector3d& position,
                                                   const Eigen::Vector3d& speed,
                                                   const double radius,
                                                   const double height,
                                                   const double mass)
    {
        return spawnBody(name, position, speed, std::make_unique<ShapeCylinder>(radius, height), mass);
    }

    std::optional<ShapeLoadInfo> WorldSceneManager::getBodyShapeLoadInfo(const std::uint64_t id) const
    {
        const BodyRecord* body = findBody(id);
        if (body == nullptr || body->entity->getShape() == nullptr) {
            return std::nullopt;
        }
        return body->entity->getShape()->getLoadInfo();
    }

    std::optional<Eigen::Vector3d> WorldSceneManager::getBodyPosition(const std::uint64_t id) const
    {
        const BodyRecord* body = findBody(id);
        if (body == nullptr || body->entity == nullptr) {
            return std::nullopt;
        }
        return body->entity->getPosition();
    }

    bool WorldSceneManager::setBodyPosition(const std::uint64_t id, const Eigen::Vector3d& position)
    {
        BodyRecord* body = findBody(id);
        if (body == nullptr || body->entity == nullptr) {
            return false;
        }

        body->entity->setPosition(position);
        return true;
    }

    std::optional<Eigen::Quaterniond> WorldSceneManager::getBodyRotationQuat(std::uint64_t id) const
    {
        const BodyRecord* body = findBody(id);
        if (body == nullptr || body->entity == nullptr) {
            return std::nullopt;
        }
        return body->entity->getTransform().getRotation();
    }

    std::optional<Eigen::Matrix3d> WorldSceneManager::getBodyRotationMat(std::uint64_t id) const
    {
        const BodyRecord* body = findBody(id);
        if (body == nullptr || body->entity == nullptr) {
            return std::nullopt;
        }
        return body->entity->getTransform().getRotationMatrix();
    }

    bool WorldSceneManager::setBodyRotation(std::uint64_t id, const Eigen::Quaterniond& rotationQuat)
    {
        BodyRecord* body = findBody(id);
        if (body == nullptr || body->entity == nullptr) {
            return false;
        }

        body->entity->setRotation(rotationQuat);
        return true;
    }

    bool WorldSceneManager::setBodyRotation(std::uint64_t id, const Eigen::Matrix3d& rotationMat)
    {
        BodyRecord* body = findBody(id);
        if (body == nullptr || body->entity == nullptr) {
            return false;
        }

        body->entity->setRotation(Eigen::Quaterniond(rotationMat));
        return true;
    }

    std::uint64_t WorldSceneManager::addGravityField(const Eigen::Vector3d& gravity,
                                                     const Eigen::Vector3d& referencePoint,
                                                     const std::string& name)
    {
        auto record = std::make_unique<FieldRecord>();
        record->name = name;
        record->field = std::make_shared<GravityField>(gravity, referencePoint, UuidGenerator::instance().generate());
        m_fields.push_back(std::move(record));
        rebuildSolverCaches();
        return m_fields.back()->field->getId();
    }

    std::uint64_t WorldSceneManager::registerField(const std::string& name,
                                                   const std::shared_ptr<FieldBase>& field)
    {
        if (field == nullptr) {
            return 0;
        }

        auto record = std::make_unique<FieldRecord>();
        record->name = name;
        record->field = field;
        m_fields.push_back(std::move(record));
        rebuildSolverCaches();
        return m_fields.back()->field->getId();
    }

    std::uint64_t WorldSceneManager::registerBodyField(const std::string& bodyName,
                                                       const std::string& fieldName,
                                                       const std::shared_ptr<PhysicalEntity>& entity,
                                                       const std::shared_ptr<FieldBase>& field,
                                                       std::unique_ptr<ShapeBase> shape)
    {
        if (entity == nullptr || field == nullptr || shape == nullptr) {
            return 0;
        }

        if (entity.owner_before(field) || field.owner_before(entity)) {
            return 0;
        }

        auto bodyRecord = std::make_unique<BodyRecord>();
        const std::uint64_t id{UuidGenerator::instance().generate()};
        bodyRecord->name = bodyName;
        bodyRecord->entity = unique_ptr_cast<PhysicalEntity, PhysicsAbstract>(std::move(entity->clone(id)));
        bodyRecord->entity->setShape(std::move(shape));
        m_bodies.push_back(std::move(bodyRecord));

        auto fieldRecord = std::make_unique<FieldRecord>();
        fieldRecord->name = fieldName;
        fieldRecord->field = field;
        fieldRecord->field->setId(id);

        m_fields.push_back(std::move(fieldRecord));

        rebuildSolverCaches();
        return id;
    }

    void WorldSceneManager::registerScene(const SceneRecord& record)
    {
        for (const auto& body : record.bodies) {
            m_bodies.push_back(std::make_unique<BodyRecord>(body));
        }
        for (const auto& field : record.fields) {
            m_fields.push_back(std::make_unique<FieldRecord>(field));
        }
        for (const auto& constraint : record.constraints) {
            m_constraints.push_back(std::make_unique<ConstraintRecord>(constraint));
        }
    }

    bool WorldSceneManager::removeBody(const std::uint64_t id)
    {
        const auto beforeSize = m_bodies.size();
        std::erase_if(m_bodies, [id](const std::unique_ptr<BodyRecord>& item)
        {
            return item != nullptr && item->entity->getId() == id;
        });
        const bool removed = m_bodies.size() != beforeSize;
        if (removed) {
            rebuildSolverCaches();
        }
        return removed;
    }

    bool WorldSceneManager::removeField(const std::uint64_t id)
    {
        const auto beforeSize = m_fields.size();
        std::erase_if(m_fields, [id](const std::unique_ptr<FieldRecord>& item)
        {
            return item != nullptr && item->field->getId() == id;
        });
        const bool removed = m_fields.size() != beforeSize;
        if (removed) {
            rebuildSolverCaches();
        }
        return removed;
    }

    bool WorldSceneManager::removeBodyField(const std::uint64_t id)
    {
        const auto bodiesBefore = m_bodies.size();
        std::erase_if(m_bodies, [id](const std::unique_ptr<BodyRecord>& item)
        {
            return item != nullptr && item->entity->getId() == id;
        });

        const auto fieldsBefore = m_fields.size();
        std::erase_if(m_fields, [id](const std::unique_ptr<FieldRecord>& item)
        {
            return item != nullptr && item->field->getId() == id;
        });

        const bool removed = m_bodies.size() != bodiesBefore || m_fields.size() != fieldsBefore;
        if (removed) {
            rebuildSolverCaches();
        }
        return removed;
    }

    void WorldSceneManager::clear()
    {
        m_bodies.clear();
        m_fields.clear();
        m_constraints.clear();
        m_solverBodies.clear();
        m_solverBodyOwners.clear();
        m_solverFields.clear();
        m_solverFieldOwners.clear();
        m_solverConstraints.clear();
        m_solverConstraintOwners.clear();
        m_simulationTime = 0.0;
        m_stepCount = 0;
        m_nextId = 1;
    }

    SolverStepResult WorldSceneManager::tick(const double dt)
    {
        if (m_solver == nullptr) {
            return {};
        }

        rebuildSolverCaches();
        const SolverStepResult result = m_solver->step(*this, dt);
        ++m_stepCount;
        return result;
    }

    std::vector<SceneRenderItem> WorldSceneManager::buildRenderItems() const
    {
        std::vector<SceneRenderItem> items;
        items.reserve(m_bodies.size());

        for (const auto& body : m_bodies) {
            if (body == nullptr || body->entity == nullptr || body->entity->getShape() == nullptr) {
                continue;
            }

            const Eigen::Vector3d position = body->entity->getPosition();

            SceneRenderItem item{};
            item.id = body->entity->getId();
            item.kind = static_cast<std::uint32_t>(body->entity->getShape()->getType());
            item.velocity = body->entity->getVelocity();
            item.angularVelocity = body->entity->getAngularVelocity();

            item.label = body->name.empty() ? (std::string("Body ") + std::to_string(body->entity->getId())) : body->name;
            item.color = body->entity->getMass() <= 0.0 ? "#6C7A89" : "#3EC5FF";

            const Transform& transform = body->entity->getTransform();
            const Eigen::Vector3d modelScale = transform.getScale();
            const Eigen::Quaterniond modelRotation = transform.getRotation();

            double boxSizeX = 1.0;
            double boxSizeY = 1.0;
            double boxSizeZ = 1.0;
            double modelRadius = 1.0;
            double modelLength = 1.0;
            double modelHeight = 1.0;

            const auto shape = body->entity->getShape();
            const ShapeLoadInfo shapeInfo = shape->getLoadInfo();
            switch (shape->getType()) {
            case ShapeType::Sphere:
                [[fallthrough]];
            case ShapeType::Disk:
                [[fallthrough]];
            case ShapeType::Ring:
                [[fallthrough]];
            case ShapeType::SphericalShell: {
                modelRadius = readShapeParamAsDouble(shapeInfo, "radius", 1.0);
                break;
            }
            case ShapeType::Cylinder:
                [[fallthrough]];
            case ShapeType::CylindricalShell: {
                modelRadius = readShapeParamAsDouble(shapeInfo, "radius", 1.0);
                modelHeight = readShapeParamAsDouble(shapeInfo, "height", 1);
                break;
            }
            case ShapeType::Box: {
                const Eigen::Vector3d size = readShapeParamAsVector3(shapeInfo,
                                                                     "size",
                                                                     Eigen::Vector3d::Ones());
                boxSizeX = std::abs(size.x());
                boxSizeY = std::abs(size.y());
                boxSizeZ = std::abs(size.z());
                break;
            }
            case ShapeType::Rod: {
                modelLength = readShapeParamAsDouble(shapeInfo, "length", 1.0);
                break;
            }
            default:
                break;
            }

            const Eigen::Vector3d scale = {modelScale.x(), modelScale.y(), modelScale.z()};
            const Eigen::Vector3d boxSize = {boxSizeX, boxSizeY, boxSizeZ};
            const Eigen::Vector3d renderScale = TransformTransformer::buildQuick3DRenderScale(
                item.kind, scale, boxSize, modelRadius, modelHeight, modelLength);
            item.scale = renderScale;
            item.rotation = {modelRotation.w(), modelRotation.y(), modelRotation.z(), modelRotation.x()};
            item.position = {position.y(), position.z(), position.x()};

            items.push_back(std::move(item));
        }

        return items;
    }

    std::size_t WorldSceneManager::getBodyCount() const
    {
        return m_bodies.size();
    }

    std::size_t WorldSceneManager::getFieldCount() const
    {
        return m_fields.size();
    }

    std::size_t WorldSceneManager::getStepCount() const
    {
        return m_stepCount;
    }

    double WorldSceneManager::getSimulationTime() const
    {
        return m_simulationTime;
    }

    void WorldSceneManager::advanceSimulationTime(const double dt)
    {
        m_simulationTime += dt;
    }

    std::span<SolverBodyHandle> WorldSceneManager::getBodies()
    {
        rebuildSolverBodyCache();
        return {m_solverBodies.data(), m_solverBodies.size()};
    }

    std::span<const SolverBodyHandle> WorldSceneManager::getBodies() const
    {
        rebuildSolverBodyCache();
        return {m_solverBodies.data(), m_solverBodies.size()};
    }

    std::span<FieldBase*> WorldSceneManager::getFields()
    {
        rebuildSolverFieldCache();
        return {m_solverFields.data(), m_solverFields.size()};
    }

    std::span<FieldBase*const> WorldSceneManager::getFields() const
    {
        rebuildSolverFieldCache();
        return {m_solverFields.data(), m_solverFields.size()};
    }

    std::vector<std::string> WorldSceneManager::getBodyNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_bodies.size());
        for (const auto& body : m_bodies) {
            if (body != nullptr) {
                names.push_back(body->name);
            }
        }
        return names;
    }

    void WorldSceneManager::rebuildSolverCaches() const
    {
        rebuildSolverBodyCache();
        rebuildSolverFieldCache();
        rebuildSolverConstraintCache();
    }

    void WorldSceneManager::rebuildSolverBodyCache() const
    {
        // 保存旧的缓存，用于保留惯性张量等数据
        std::vector<SolverBodyHandle> oldSolverBodies = std::move(m_solverBodies);
        std::unordered_map<PhysicalEntity*, SolverBodyHandle*> oldHandleMap;
        for (auto& oldHandle : oldSolverBodies) {
            if (oldHandle.entity != nullptr) {
                oldHandleMap[oldHandle.entity] = &oldHandle;
            }
        }

        m_solverBodies.clear();
        m_solverBodies.reserve(m_bodies.size());
        m_solverBodyOwners.clear();
        m_solverBodyOwners.reserve(m_bodies.size());

        for (const auto& body : m_bodies) {
            if (body == nullptr || body->entity == nullptr || body->entity->getShape() == nullptr) {
                continue;
            }

            m_solverBodyOwners.push_back(body->entity);

            SolverBodyHandle handle{};
            handle.entity = body->entity.get();
            // handle.shape = body->entity->getShape().get();
            handle.inverseMass = body->entity->getMass() > 0.0 ? 1.0 / body->entity->getMass() : 0.0;
            handle.enableCollision = body->enableCollision;
            handle.enableIntegration = body->enableIntegration;

            // 如果实体已在旧缓存中，保留其惯性张量
            auto it = oldHandleMap.find(body->entity.get());
            if (it != oldHandleMap.end()) {
                handle.inverseInertiaTensor = it->second->inverseInertiaTensor;
            }

            m_solverBodies.push_back(handle);
        }
    }

    void WorldSceneManager::rebuildSolverFieldCache() const
    {
        std::size_t validFieldCount = 0;
        for (const auto& field : m_fields) {
            if (field != nullptr && field->field != nullptr) {
                ++validFieldCount;
            }
        }

        std::vector<FieldBase*> rebuiltFields(validFieldCount, nullptr);
        m_solverFieldOwners.clear();
        m_solverFieldOwners.reserve(validFieldCount);

        std::size_t writeIndex = 0;
        for (const auto& field : m_fields) {
            if (field != nullptr && field->field != nullptr) {
                m_solverFieldOwners.push_back(field->field);
                rebuiltFields[writeIndex] = field->field.get();
                ++writeIndex;
            }
        }

        m_solverFields = std::move(rebuiltFields);
    }


    BodyRecord* WorldSceneManager::findBody(const std::uint64_t id)
    {
        for (auto& body : m_bodies) {
            if (body != nullptr && body->entity->getId() == id) {
                return body.get();
            }
        }
        return nullptr;
    }

    FieldRecord* WorldSceneManager::findField(const std::uint64_t id)
    {
        for (auto& field : m_fields) {
            if (field != nullptr && field->field->getId() == id) {
                return field.get();
            }
        }
        return nullptr;
    }

    const BodyRecord* WorldSceneManager::findBody(const std::uint64_t id) const
    {
        for (const auto& body : m_bodies) {
            if (body != nullptr && body->entity->getId() == id) {
                return body.get();
            }
        }
        return nullptr;
    }

    const FieldRecord* WorldSceneManager::findField(const std::uint64_t id) const
    {
        for (const auto& field : m_fields) {
            if (field != nullptr && field->field->getId() == id) {
                return field.get();
            }
        }
        return nullptr;
    }

    std::uint64_t WorldSceneManager::registerConstraint(const std::string& name,
                                                        const std::shared_ptr<ConstraintsBase>& constraint)
    {
        if (constraint == nullptr) {
            return 0;
        }

        auto record = std::make_unique<ConstraintRecord>();
        record->name = name;
        record->constraint = constraint;
        record->enabled = true;
        record->constraint->setId(UuidGenerator::instance().generate());
        m_constraints.push_back(std::move(record));
        return m_constraints.back()->constraint->getId();
    }

    std::uint64_t WorldSceneManager::createConnectingLine(const std::string& name,
                                                          const double length,
                                                          const std::uint64_t entityStartId,
                                                          const std::uint64_t entityEndId,
                                                          const std::initializer_list<Eigen::Vector3d>&
                                                          turningPositions)
    {
        BodyRecord* startBody = findBody(entityStartId);
        BodyRecord* endBody = findBody(entityEndId);

        if (startBody == nullptr || endBody == nullptr || startBody->entity == nullptr || endBody->entity == nullptr) {
            return 0;
        }

        auto constraint = std::make_shared<ConnectingLine>(
            length, startBody->entity.get(), endBody->entity.get(), UuidGenerator::instance().generate(), turningPositions);

        return registerConstraint(name, constraint);
    }

    std::uint64_t WorldSceneManager::createSimplePendulum(const std::string& name, double length,
                                                          const Eigen::Vector3d& anchorPos, std::uint64_t entityId)
    {
        BodyRecord* body = findBody(entityId);
        if (body == nullptr) {
            return 0;
        }

        const auto simplePendulum{
            std::make_shared<SimplePendulum>(
                length, anchorPos, body->entity.get(), UuidGenerator::instance().generate())
        };

        return registerConstraint(name, simplePendulum);
    }

    ConstraintsBase* WorldSceneManager::getConstraint(const std::uint64_t id)
    {
        ConstraintRecord* record = findConstraint(id);
        if (record == nullptr) {
            return nullptr;
        }
        return record->constraint.get();
    }

    std::vector<ConstraintsBase*> WorldSceneManager::getAllConstraints() const
    {
        rebuildSolverConstraintCache();
        std::vector<ConstraintsBase*> result;
        result.reserve(m_constraints.size());
        for (auto& record : m_constraints) {
            if (record != nullptr && record->enabled) {
                result.push_back(record->constraint.get());
            }
        }
        return result;
    }

    bool WorldSceneManager::removeConstraint(const std::uint64_t id)
    {
        const auto beforeSize = m_constraints.size();
        std::erase_if(m_constraints, [id](const std::unique_ptr<ConstraintRecord>& item)
        {
            return item != nullptr && item->constraint->getId() == id;
        });
        const bool removed = m_constraints.size() != beforeSize;
        if (removed) {
            rebuildSolverConstraintCache();
        }
        return removed;
    }

    bool WorldSceneManager::setConstraintEnabled(const std::uint64_t id, const bool enabled)
    {
        ConstraintRecord* record = findConstraint(id);
        if (record == nullptr) {
            return false;
        }
        record->enabled = enabled;
        return true;
    }

    double WorldSceneManager::getConstraintError(const std::uint64_t id) const
    {
        const ConstraintRecord* record = findConstraint(id);
        if (record == nullptr || record->constraint == nullptr) {
            return 0.0;
        }
        return record->constraint->computeConstraintError();
    }

    bool WorldSceneManager::addTurningPoint(const std::uint64_t constraintId,
                                            const size_t index,
                                            const Eigen::Vector3d& pos)
    {
        ConstraintRecord* record = findConstraint(constraintId);
        if (record == nullptr || record->constraint == nullptr) {
            return false;
        }

        if (record->constraint->getType() == ConstraintType::ConnectingLine) {
            auto* connectingLine = dynamic_cast<ConnectingLine*>(record->constraint.get());
            connectingLine->addPathTurningPoint(index, pos);
            return true;
        }
        return false;
    }

    bool WorldSceneManager::removeTurningPoint(const std::uint64_t constraintId, const size_t index)
    {
        ConstraintRecord* record = findConstraint(constraintId);
        if (record == nullptr || record->constraint == nullptr) {
            return false;
        }

        if (record->constraint->getType() == ConstraintType::ConnectingLine) {
            auto* connectingLine = dynamic_cast<ConnectingLine*>(record->constraint.get());
            connectingLine->removePathTurningPoint(index);
            return true;
        }
        return false;
    }

    bool WorldSceneManager::changeTurningPoint(const std::uint64_t constraintId,
                                               const size_t index,
                                               const Eigen::Vector3d& newPos)
    {
        ConstraintRecord* record = findConstraint(constraintId);
        if (record == nullptr || record->constraint == nullptr) {
            return false;
        }

        if (record->constraint->getType() == ConstraintType::ConnectingLine) {
            auto* connectingLine = dynamic_cast<ConnectingLine*>(record->constraint.get());
            connectingLine->changePathTurningPoint(index, newPos);
            return true;
        }
        return false;
    }

    std::span<ConstraintsBase*> WorldSceneManager::getConstraints()
    {
        rebuildSolverConstraintCache();
        // Safe because the solver only reads through this interface
        return {const_cast<ConstraintsBase**>(m_solverConstraints.data()), m_solverConstraints.size()};
    }

    std::span<const ConstraintsBase*> WorldSceneManager::getConstraints() const
    {
        rebuildSolverConstraintCache();
        return {m_solverConstraints.data(), m_solverConstraints.size()};
    }

    ConstraintRecord* WorldSceneManager::findConstraint(const std::uint64_t id)
    {
        for (auto& constraint : m_constraints) {
            if (constraint != nullptr && constraint->constraint->getId() == id) {
                return constraint.get();
            }
        }
        return nullptr;
    }

    const ConstraintRecord* WorldSceneManager::findConstraint(const std::uint64_t id) const
    {
        for (const auto& constraint : m_constraints) {
            if (constraint != nullptr && constraint->constraint->getId() == id) {
                return constraint.get();
            }
        }
        return nullptr;
    }

    void WorldSceneManager::rebuildSolverConstraintCache() const
    {
        m_solverConstraints.clear();
        m_solverConstraintOwners.clear();
        m_solverConstraints.reserve(m_constraints.size());
        m_solverConstraintOwners.reserve(m_constraints.size());

        for (auto& record : m_constraints) {
            if (record == nullptr || record->constraint == nullptr || !record->enabled) {
                continue;
            }

            m_solverConstraintOwners.push_back(record->constraint);
            m_solverConstraints.push_back(record->constraint.get());
        }
    }
}
