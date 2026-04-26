//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#include "world_scene_manager.h"

#include <algorithm>

#include "gravity_field.h"
#include "field_base.h"

namespace egret
{
    WorldSceneManager::WorldSceneManager(std::unique_ptr<ISolver> solver): m_solver(std::move(solver))
    {
    }

    std::uint64_t WorldSceneManager::spawnSphere(const std::string& name,
                                                 const Eigen::Vector3d& position,
                                                 const Eigen::Vector3d& speed,
                                                 const double radius,
                                                 const double mass)
    {
        auto record = std::make_unique<BodyRecord>();
        record->id = nextId();
        record->name = name;
        record->entity = std::make_unique<Particle>(position, speed, mass);
        record->shape = std::make_unique<ShapeSphere>(radius);
        record->transform.setTranslation(position);
        m_bodies.push_back(std::move(record));
        rebuildSolverCaches();
        return m_bodies.back()->id;
    }

    std::uint64_t WorldSceneManager::spawnBox(const std::string& name,
                                              const Eigen::Vector3d& position,
                                              const Eigen::Vector3d& speed,
                                              const Eigen::Vector3d& size,
                                              const double mass)
    {
        auto record = std::make_unique<BodyRecord>();
        record->id = nextId();
        record->name = name;
        record->entity = std::make_unique<Particle>(position, speed, mass);
        record->shape = std::make_unique<ShapeBox>(size);
        record->transform.setTranslation(position);
        m_bodies.push_back(std::move(record));
        rebuildSolverCaches();
        return m_bodies.back()->id;
    }

    std::uint64_t WorldSceneManager::addGravityField(const Eigen::Vector3d& gravity,
                                                     const Eigen::Vector3d& referencePoint,
                                                     const std::string& name)
    {
        auto record = std::make_unique<FieldRecord>();
        record->id = nextId();
        record->name = name;
        record->field = std::make_unique<GravityField>(gravity, referencePoint);
        m_fields.push_back(std::move(record));
        rebuildSolverCaches();
        return m_fields.back()->id;
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

    void WorldSceneManager::clear()
    {
        m_bodies.clear();
        m_fields.clear();
        m_solverBodies.clear();
        m_solverFields.clear();
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

            SceneRenderItem item{};
            item.id = body->id;
            item.label = body->name.empty() ? (std::string("Body ") + std::to_string(body->id)) : body->name;
            item.color = body->entity->getMass() <= 0.0 ? "#6C7A89" : "#3EC5FF";

            const Eigen::Vector3d position = body->entity->getPosition();

            if (const auto* sphere = dynamic_cast<const ShapeSphere*>(body->shape.get()); sphere != nullptr) {
                const double diameter = sphere->getRadius() * 2.0;
                item.kind = "sphere";
                item.width = diameter;
                item.height = diameter;
                item.x = position.x() - sphere->getRadius();
                item.y = position.y() - sphere->getRadius();
            } else if (const auto* box = dynamic_cast<const ShapeBox*>(body->shape.get()); box != nullptr) {
                const Eigen::Vector3d size = box->getSize();
                item.kind = "box";
                item.width = size.x();
                item.height = size.y();
                item.x = position.x() - size.x() * 0.5;
                item.y = position.y() - size.y() * 0.5;
            } else {
                item.kind = "particle";
                item.width = 16.0;
                item.height = 16.0;
                item.x = position.x() - 8.0;
                item.y = position.y() - 8.0;
            }

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

        for (const auto& body : m_bodies) {
            if (body == nullptr || body->entity == nullptr || body->shape == nullptr) {
                continue;
            }

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

        std::size_t writeIndex = 0;
        for (const auto& field : m_fields) {
            if (field != nullptr && field->field != nullptr) {
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
