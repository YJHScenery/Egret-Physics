/**
* @file        rigid_body.cpp
 * @brief       刚体实现文件，定义具有转动惯性的物理实体。
 * @details     实现 RigidBody 类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-30
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "rigid_body.h"

#include <utility>

namespace egret
{
    RigidBody::RigidBody(const double mass, const std::uint64_t id) : PhysicsAbstract(id), PhysicalEntity(mass, id)
    {
    }

    RigidBody::RigidBody(const Eigen::Vector3d& position, Eigen::Vector3d speed, double mass, std::uint64_t id) :
        PhysicsAbstract(id), PhysicalEntity(position, speed, mass, id)
    {
    }

    RigidBody::RigidBody(const Eigen::Vector3d& position, const Eigen::Vector3d& speed, const double mass,
                         std::vector<Force> forces, std::uint64_t id) : PhysicsAbstract(id),
                                                                        PhysicalEntity(
                                                                            position, speed, mass, std::move(forces), id)
    {
    }

    bool RigidBody::hasRotation() const
    {
        return true;
    }

    Eigen::Vector3d RigidBody::getTorque()
    {
        // 力矩来源：直接存储的力矩 m_torques（力作用于零点）
        Eigen::Vector3d totalTorque = Eigen::Vector3d::Zero();

        for (const Torque& torque : m_torques) {
            totalTorque += torque.torque;
        }

        return totalTorque;
    }

    Eigen::Vector3d RigidBody::getAngularMomentum()
    {
        if (!m_shape) {
            return Eigen::Vector3d::Zero();
        }
        const Eigen::Matrix3d I_local = m_shape->getInertiaTensor(m_mass);
        const Eigen::Matrix3d R = m_transform.getRotationMatrix();
        const Eigen::Matrix3d I_world = R * I_local * R.transpose();

        return I_world * m_angularVelocity;
    }

    double RigidBody::getKineticEnergy()
    {
        double ke = 0.5 * m_mass * m_velocity.squaredNorm();

        const Eigen::Matrix3d R = m_transform.getRotationMatrix();
        const Eigen::Vector3d wLocal = R.transpose() * m_angularVelocity;
        const Eigen::Matrix3d I_local = m_shape->getInertiaTensor(m_mass);
        ke += 0.5 * wLocal.dot(I_local * wLocal);

        return ke;
    }

    std::unique_ptr<PhysicsAbstract> RigidBody::clone(std::uint64_t id) const
    {
        auto entity = std::make_unique<RigidBody>(*this);
        entity->setId(id);
        return entity;
    }
} // egret
