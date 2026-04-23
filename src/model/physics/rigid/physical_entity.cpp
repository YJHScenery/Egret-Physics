//
// Created by jehor on 2026/4/23.
//

#include "physical_entity.h"

namespace egret
{
    PhysicalEntity::PhysicalEntity(const double mass): m_mass(mass)
    {
    }

    PhysicalEntity::PhysicalEntity(Eigen::Vector3f position, Eigen::Vector3f speed, const double mass):
        m_position(std::move(position)), m_speed(std::move(speed)), m_mass(mass)
    {
    }

    PhysicalEntity::PhysicalEntity(const Eigen::Vector3f& position, const Eigen::Vector3f& speed, const double mass,
        std::vector<Force> forces): PhysicalEntity(position, speed, mass)
    {
        m_forces = std::move(forces);
    }
}
