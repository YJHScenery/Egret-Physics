//
// Created by jehor on 2026/4/25.
//

#include "gravity_field.h"

#include <utility>
#include "physical_entity.h"

namespace egret
{
    GravityField::GravityField(): FieldBase(generateID(FieldType::Gravity))
    {
    }

    GravityField::GravityField(Eigen::Vector3d  gravity, Eigen::Vector3d  referencePoint) :
        FieldBase(generateID(FieldType::Gravity)),
        m_gravity(std::move(gravity)), m_zeroReferencePoint(std::move(referencePoint))
    {
    }

    GravityField::GravityField(const double gravity, const double referenceZ) :
        FieldBase(generateID(FieldType::Gravity)),
        m_gravity({0, 0, -gravity}), m_zeroReferencePoint({0, 0, referenceZ})
    {
    }

    Eigen::Vector3d GravityField::sample(const Eigen::Vector3d& position)
    {
        std::ignore = position;
        return m_gravity;
    }

    double GravityField::potential(const Eigen::Vector3d& position)
    {
        return -m_gravity.dot(position - m_zeroReferencePoint);
    }

    void GravityField::applyToEntity(PhysicalEntity* entity)
    {
        if (entity == nullptr) {
            return;
        }

        const Force gravityForce{.applyPosition = std::nullopt, .force = entity->getMass() * m_gravity, .id = m_id};
        entity->upsertForce(gravityForce);

    }
} // egret
