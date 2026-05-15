//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#include "world_scene_manager.h"

#include <algorithm>
#include <cmath>

#include "constants.h"
#include "gravity_field.h"
#include "field_base.h"
#include "shape_factory_registry.h"
#include "shape_box.h"
#include "shape_sphere.h"

namespace egret
{
    namespace
    {
        [[nodiscard]] double readShapeParamAsDouble(const ShapeLoadInfo& info, const std::string& key, const double fallback)
        {
            const auto iter = info.parameters.find(key);
            if (iter == info.parameters.end()) {
                return fallback;
            }

            return std::visit(
                [fallback](const auto& value) -> double
                {
                    using ValueType = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<ValueType, double>) {
                        return value;
                    }
                    if constexpr (std::is_same_v<ValueType, std::int64_t>) {
                        return static_cast<double>(value);
                    }
                    return fallback;
                },
                iter->second);
        }

        [[nodiscard]] Eigen::Vector3d readShapeParamAsVector3(const ShapeLoadInfo& info,
                                                               const std::string& key,
                                                               const Eigen::Vector3d& fallback)
        {
            const auto iter = info.parameters.find(key);
            if (iter == info.parameters.end()) {
                return fallback;
            }

            if (const auto* vec = std::get_if<Eigen::Vector3d>(&iter->second); vec != nullptr) {
                return *vec;
            }
            return fallback;
        }
    }

    WorldSceneManager::WorldSceneManager(std::unique_ptr<SolverBase> solver): m_solver(std::move(solver))
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
        record->id = nextId();
        record->name = name;
        record->entity = std::make_shared<Particle>(position, speed, mass);
        record->shape = std::move(shape);
        record->transform.setTranslation(position);
        m_bodies.push_back(std::move(record));
        rebuildSolverCaches();
        return m_bodies.back()->id;
    }

    std::uint64_t WorldSceneManager::registerBody(const std::string& name,
                                                  const std::shared_ptr<PhysicalEntity>& entity,
                                                  std::unique_ptr<ShapeBase> shape)
    {
        if (entity == nullptr || shape == nullptr) {
            return 0;
        }

        auto record = std::make_unique<BodyRecord>();
        record->id = nextId();
        record->name = name;
        record->entity = entity;
        record->shape = std::move(shape);
        record->transform.setTranslation(entity->getPosition());
        m_bodies.push_back(std::move(record));
        rebuildSolverCaches();
        return m_bodies.back()->id;
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

    std::optional<ShapeLoadInfo> WorldSceneManager::getBodyShapeLoadInfo(const std::uint64_t id) const
    {
        const BodyRecord* body = findBody(id);
        if (body == nullptr || body->shape == nullptr) {
            return std::nullopt;
        }
        return body->shape->getLoadInfo();
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
        body->transform.setTranslation(position);
        return true;
    }

    std::uint64_t WorldSceneManager::addGravityField(const Eigen::Vector3d& gravity,
                                                     const Eigen::Vector3d& referencePoint,
                                                     const std::string& name)
    {
        auto record = std::make_unique<FieldRecord>();
        record->id = nextId();
        record->name = name;
        record->field = std::make_shared<GravityField>(gravity, referencePoint);
        m_fields.push_back(std::move(record));
        rebuildSolverCaches();
        return m_fields.back()->id;
    }

    std::uint64_t WorldSceneManager::registerField(const std::string& name,
                                                   const std::shared_ptr<FieldBase>& field)
    {
        if (field == nullptr) {
            return 0;
        }

        auto record = std::make_unique<FieldRecord>();
        record->id = nextId();
        record->name = name;
        record->field = field;
        m_fields.push_back(std::move(record));
        rebuildSolverCaches();
        return m_fields.back()->id;
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

        const std::uint64_t id = nextId();

        auto bodyRecord = std::make_unique<BodyRecord>();
        bodyRecord->id = id;
        bodyRecord->name = bodyName;
        bodyRecord->entity = entity;
        bodyRecord->shape = std::move(shape);
        bodyRecord->transform.setTranslation(entity->getPosition());
        m_bodies.push_back(std::move(bodyRecord));

        auto fieldRecord = std::make_unique<FieldRecord>();
        fieldRecord->id = id;
        fieldRecord->name = fieldName;
        fieldRecord->field = field;
        m_fields.push_back(std::move(fieldRecord));

        rebuildSolverCaches();
        return id;
    }

    bool WorldSceneManager::removeBody(const std::uint64_t id)
    {
        const auto beforeSize = m_bodies.size();
        std::erase_if(m_bodies, [id](const std::unique_ptr<BodyRecord>& item)
        {
            return item != nullptr && item->id == id;
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
            return item != nullptr && item->id == id;
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
            return item != nullptr && item->id == id;
        });

        const auto fieldsBefore = m_fields.size();
        std::erase_if(m_fields, [id](const std::unique_ptr<FieldRecord>& item)
        {
            return item != nullptr && item->id == id;
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
        m_solverBodies.clear();
        m_solverBodyOwners.clear();
        m_solverFields.clear();
        m_solverFieldOwners.clear();
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
            if (body == nullptr || body->entity == nullptr || body->shape == nullptr) {
                continue;
            }


            const Eigen::Vector3d position = body->entity->getPosition();

            // const std::string& shapeTypeStr {body->shape->typeId()};

            SceneRenderItem item{body->shape->getBasicRenderInfo(position)};

            item.id = body->id;
            item.label = body->name.empty() ? (std::string("Body ") + std::to_string(body->id)) : body->name;
            item.color = body->entity->getMass() <= 0.0 ? "#6C7A89" : "#3EC5FF";
            item.centerX = position.x();
            item.centerY = position.y();
            item.centerZ = position.z();

            const ShapeLoadInfo shapeInfo = body->shape->getLoadInfo();
            item.sizeX = std::max(1.0, item.width);
            item.sizeY = std::max(1.0, item.height);
            item.sizeZ = std::max(1.0, std::min(item.sizeX, item.sizeY));

            if (shapeInfo.typeId == TYPE_ID_STANDARD_SPHERE || shapeInfo.typeId == TYPE_ID_STANDARD_DISK
                || shapeInfo.typeId == TYPE_ID_STANDARD_RING || shapeInfo.typeId == TYPE_ID_STANDARD_SPHERICAL_SHELL) {
                const double radius = std::max(0.5, readShapeParamAsDouble(shapeInfo, "radius", item.sizeX * 0.5));
                const double diameter = radius * 2.0;
                item.sizeX = diameter;
                item.sizeY = diameter;
                item.sizeZ = diameter;
            } else if (shapeInfo.typeId == TYPE_ID_STANDARD_CYLINDER || shapeInfo.typeId == TYPE_ID_STANDARD_CYLINDRICAL_SHELL) {
                const double radius = std::max(0.5, readShapeParamAsDouble(shapeInfo, "radius", item.sizeX * 0.5));
                const double height = std::max(1.0, readShapeParamAsDouble(shapeInfo, "height", item.sizeZ));
                const double diameter = radius * 2.0;
                item.sizeX = diameter;
                item.sizeY = diameter;
                item.sizeZ = height;
            } else if (shapeInfo.typeId == TYPE_ID_STANDARD_BOX) {
                const Eigen::Vector3d size = readShapeParamAsVector3(shapeInfo,
                                                                      "size",
                                                                      Eigen::Vector3d(item.sizeX, item.sizeY, item.sizeZ));
                item.sizeX = std::max(1.0, std::abs(size.x()));
                item.sizeY = std::max(1.0, std::abs(size.y()));
                item.sizeZ = std::max(1.0, std::abs(size.z()));
            } else if (shapeInfo.typeId == TYPE_ID_STANDARD_ROD) {
                const double length = std::max(1.0, readShapeParamAsDouble(shapeInfo, "length", item.sizeZ));
                item.sizeX = 6.0;
                item.sizeY = 6.0;
                item.sizeZ = length;
            }

            // if (const auto* sphere = dynamic_cast<const ShapeSphere*>(body->shape.get()); sphere != nullptr) {
            //     const double diameter = sphere->getRadius() * 2.0;
            //     item.kind = "sphere";
            //     item.width = diameter;
            //     item.height = diameter;
            //     item.x = position.x() - sphere->getRadius();
            //     item.y = position.y() - sphere->getRadius();
            // } else if (const auto* box = dynamic_cast<const ShapeBox*>(body->shape.get()); box != nullptr) {
            //     const Eigen::Vector3d size = box->getSize();
            //     item.kind = "box";
            //     item.width = size.x();
            //     item.height = size.y();
            //     item.x = position.x() - size.x() * 0.5;
            //     item.y = position.y() - size.y() * 0.5;
            // } else {
            //     item.kind = "particle";
            //     item.width = 16.0;
            //     item.height = 16.0;
            //     item.x = position.x() - 8.0;
            //     item.y = position.y() - 8.0;
            // }

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

    std::span<FieldBase* const> WorldSceneManager::getFields() const
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
    }

    void WorldSceneManager::rebuildSolverBodyCache() const
    {
        m_solverBodies.clear();
        m_solverBodies.reserve(m_bodies.size());
        m_solverBodyOwners.clear();
        m_solverBodyOwners.reserve(m_bodies.size());

        for (const auto& body : m_bodies) {
            if (body == nullptr || body->entity == nullptr || body->shape == nullptr) {
                continue;
            }

            m_solverBodyOwners.push_back(body->entity);

            SolverBodyHandle handle{};
            handle.id = body->id;
            handle.entity = body->entity.get();
            handle.transform = const_cast<Transform*>(&body->transform);
            handle.shape = body->shape.get();
            handle.inverseMass = body->entity->getMass() > 0.0 ? 1.0 / body->entity->getMass() : 0.0;
            handle.restitution = body->entity->getRestitution()/*(body->restitution)*/;
            handle.enableCollision = body->enableCollision;
            handle.enableIntegration = body->enableIntegration;
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

    std::uint64_t WorldSceneManager::nextId()
    {
        return m_nextId++;
    }

    WorldSceneManager::BodyRecord* WorldSceneManager::findBody(const std::uint64_t id)
    {
        for (auto& body : m_bodies) {
            if (body != nullptr && body->id == id) {
                return body.get();
            }
        }
        return nullptr;
    }

    WorldSceneManager::FieldRecord* WorldSceneManager::findField(const std::uint64_t id)
    {
        for (auto& field : m_fields) {
            if (field != nullptr && field->id == id) {
                return field.get();
            }
        }
        return nullptr;
    }

    const WorldSceneManager::BodyRecord* WorldSceneManager::findBody(const std::uint64_t id) const
    {
        for (const auto& body : m_bodies) {
            if (body != nullptr && body->id == id) {
                return body.get();
            }
        }
        return nullptr;
    }

    const WorldSceneManager::FieldRecord* WorldSceneManager::findField(const std::uint64_t id) const
    {
        for (const auto& field : m_fields) {
            if (field != nullptr && field->id == id) {
                return field.get();
            }
        }
        return nullptr;
    }
}
