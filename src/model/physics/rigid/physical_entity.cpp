//
// Created by jehor on 2026/4/23.
//

#include "physical_entity.h"

namespace egret
{
    PhysicalEntity::PhysicalEntity(const double mass): m_mass(mass)
    {
    }

    PhysicalEntity::PhysicalEntity(Eigen::Vector3d position, Eigen::Vector3d speed, const double mass):
        m_speed(std::move(speed)), m_mass(mass)
    {
        m_transform.setTranslation(position);
    }

    PhysicalEntity::PhysicalEntity(const Eigen::Vector3d& position, const Eigen::Vector3d& speed, const double mass,
        std::vector<Force> forces): PhysicalEntity(position, speed, mass)
    {
        m_forces = std::move(forces);
    }

    void PhysicalEntity::upsertForce(const Force& force)
    {
        std::erase_if(m_forces, [&force](const Force& item)
        {
            return item.id == force.id;
        });
        m_forces.push_back(force);
    }

    void PhysicalEntity::setRestitution(const double restitution)
    {
        m_restitution = restitution;
    }

    bool PhysicalEntity::removeForceById(const std::uint64_t id)
    {
        const auto beforeSize = m_forces.size();
        std::erase_if(m_forces, [id](const Force& item)
        {
            return item.id == id;
        });
        return m_forces.size() != beforeSize;
    }

    bool PhysicalEntity::hasForce(const std::uint64_t id) const
    {
        return std::ranges::any_of(m_forces, [id](const Force& item)
        {
            return item.id == id;
        });
    }

    void PhysicalEntity::removeForce(const long long index)
    {
        const auto iter {m_forces.begin() + index};
        if (iter != m_forces.end()) {
            m_forces.erase(iter);
        }
    }

    void PhysicalEntity::upsertTorque(const Torque& torque)
    {
        std::erase_if(m_torques, [&torque](const Torque& item)
        {
            return item.id == torque.id;
        });
        m_torques.push_back(torque);
    }

    bool PhysicalEntity::removeTorqueById(const std::uint64_t id)
    {
        const auto beforeSize = m_torques.size();
        std::erase_if(m_torques, [id](const Torque& item)
        {
            return item.id == id;
        });
        return m_torques.size() != beforeSize;
    }

    bool PhysicalEntity::hasTorque(const std::uint64_t id) const
    {
        return std::ranges::any_of(m_torques, [id](const Torque& item)
        {
            return item.id == id;
        });
    }

    void PhysicalEntity::removeTorque(const long long index)
    {
        const auto iter {m_torques.begin() + index};
        if (iter != m_torques.end()) {
            m_torques.erase(iter);
        }
    }
}
