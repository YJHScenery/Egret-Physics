/**
* @file        gravitational_field.cpp
 * @brief       引力场实现文件
 * @details     实现GravitationalField类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-26
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "gravitational_field.h"
#include "physical_entity.h"

namespace egret
{
    GravitationalField::GravitationalField()
        : FieldBase(generateID(FieldType::Gravitation)),
          Particle()
    {
    }

    GravitationalField::GravitationalField(const Eigen::Vector3d& position,
                                           const Eigen::Vector3d& speed,
                                           const double mass,
                                           const double coupling,
                                           const bool fixed)
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

        if (distanceSquared < static_minDistanceSquared) {
            return Eigen::Vector3d::Zero();
        }

        const double distance = std::sqrt(distanceSquared);
        const Eigen::Vector3d direction = -displacement / distance;

        const double fieldMagnitude = m_coupling * getMass() / distanceSquared;

        return fieldMagnitude * direction;
    }

    void GravitationalField::setMinDistanceSquared(const double minDistance)
    {
        static_minDistanceSquared = minDistance;
    }

    double GravitationalField::getMinDistanceSquared()
    {
        return static_minDistanceSquared;
    }

    double GravitationalField::potential(const Eigen::Vector3d& position)
    {
        const Eigen::Vector3d displacement = position - getPosition();
        const double distanceSquared = displacement.squaredNorm();

        if (distanceSquared < static_minDistanceSquared) {
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
