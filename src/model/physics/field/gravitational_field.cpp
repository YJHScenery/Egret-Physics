//
// Created by jehor on 2026/4/25.
//

#include "gravitational_field.h"
#include "physical_entity.h"

namespace egret
{
    GravitationalField::GravitationalField()
        : FieldBase(generateID(FieldType::Gravitation)),
          Particle()
    {
    }

    GravitationalField::GravitationalField(Eigen::Vector3d position,
                                           Eigen::Vector3d speed,
                                           double mass,
                                           double coupling,
                                           bool fixed)
        : FieldBase(generateID(FieldType::Gravitation)),
          Particle(position, speed, mass),
            m_coupling(coupling),
          m_fixed(fixed)
    {
    }

    Eigen::Vector3d GravitationalField::sample(const Eigen::Vector3d& position)
    {
        const Eigen::Vector3d displacement = position - getPosition();
        const double distanceSquared = displacement.squaredNorm();

        if (distanceSquared < MIN_DISTANCE_SQUARED) {
            return Eigen::Vector3d::Zero();
        }

        const double distance = std::sqrt(distanceSquared);
        const Eigen::Vector3d direction = -displacement / distance;

        const double fieldMagnitude = m_coupling * getMass() / distanceSquared;

        return fieldMagnitude * direction;
    }

    double GravitationalField::potential(const Eigen::Vector3d& position)
    {
        const Eigen::Vector3d displacement = position - getPosition();
        const double distanceSquared = displacement.squaredNorm();

        if (distanceSquared < MIN_DISTANCE_SQUARED) {
            return 0.0;
        }

        const double distance = std::sqrt(distanceSquared);

        return -m_coupling * getMass() / distance;
    }

    void GravitationalField::applyToEntity(PhysicalEntity* entity)
    {
        if (entity == nullptr) {
            return;
        }

        const Eigen::Vector3d fieldVector = sample(entity->getPosition());
        const Force gravityForce{
            .applyPosition = std::nullopt,
            .force = entity->getMass() * fieldVector,
            .id = m_id
        };
        entity->upsertForce(gravityForce);
    }

    void GravitationalField::applyForce(const double time)
    {
        if (m_fixed) {
            return;
        }

        Particle::applyForce(time);
    }

    void GravitationalField::setFixed(const bool fixed)
    {
        m_fixed = fixed;
    }

    bool GravitationalField::isFixed() const
    {
        return m_fixed;
    }

    void GravitationalField::setCouplingCoefficient(const double coefficient)
    {
        m_coupling = coefficient;
    }

    double GravitationalField::getCouplingCoefficient() const
    {
        return m_coupling;
    }
} // egret