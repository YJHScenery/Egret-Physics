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

#include <cmath>

namespace egret
{
    RigidBody::RigidBody(const double mass, const std::uint64_t id) : PhysicsAbstract(id), PhysicalEntity(mass, id)
    {
    }

    RigidBody::RigidBody(const Eigen::Vector3d& position, Eigen::Vector3d speed, double mass, std::uint64_t id) :
        PhysicsAbstract(id), PhysicalEntity(position, speed, mass, id)
    {
    }

    RigidBody::RigidBody(const Eigen::Vector3d& position, const Eigen::Vector3d& speed, double mass,
                         std::vector<Force> forces, std::uint64_t id) : PhysicsAbstract(id),
                                                                        PhysicalEntity(
                                                                            position, speed, mass, forces, id)
    {
    }

    void RigidBody::applyForce(const double time)
    {
        if (m_mass <= 0.0) {
            movePosition(time);
            return;
        }

        // 半隐式欧拉：先更新速度，再用新速度更新位置
        const Eigen::Vector3d acceleration{getJoinForce() / m_mass};
        m_speed += acceleration * time;
        m_transform.setTranslation(m_transform.getTranslation() + m_speed * time);
    }

    void RigidBody::movePosition(const double time)
    {
        m_transform.setTranslation(m_transform.getTranslation() + m_speed * time);
    }

    void RigidBody::applyTorque(const double time)
    {
        if (m_mass <= 0.0 || !m_shape) {
            return;
        }

        // 1. 读取累积力矩，计算角加速度 α = I⁻¹ · τ
        const Eigen::Vector3d torque = getTorque();
        Eigen::Vector3d angularAcceleration = Eigen::Vector3d::Zero();

        if (torque.norm() >= 1e-10) {
            const Eigen::Matrix3d I_local = m_shape->getInertiaTensor(m_mass);
            const Eigen::Matrix3d R = m_transform.getRotationMatrix();
            const Eigen::Matrix3d I_world = R * I_local * R.transpose();
            angularAcceleration = I_world.inverse() * torque;
        }

        // 2. 半隐式欧拉：先更新角速度，再用新角速度更新姿态
        //    （与 applyForce 先更新速度、再用新速度更新位置的顺序一致）
        m_angular += angularAcceleration * time;

        // 3. 用新角速度积分四元数姿态
        if (m_angular.norm() >= 1e-10) {
            Eigen::Quaterniond currentRot = m_transform.getRotation();
            Eigen::Quaterniond omegaQuat(0, m_angular.x(), m_angular.y(), m_angular.z());

            // 四元数微分：dq/dt = 0.5 * ω_q * q
            Eigen::Quaterniond deltaQ = omegaQuat * currentRot;
            deltaQ.coeffs() *= 0.5;
            currentRot.coeffs() += deltaQ.coeffs() * time;
            currentRot.normalize();

            m_transform.setRotation(currentRot);
        }
    }

    void RigidBody::rotate(const double time)
    {
        // 独立匀速转动：不处理力矩，仅用当前角速度积分姿态。
        // 供需要分离角速度更新与姿态更新的特殊场景使用。
        if (m_angular.norm() < 1e-10) {
            return;
        }

        Eigen::Quaterniond currentRot = m_transform.getRotation();
        const Eigen::Quaterniond omegaQuat(0, m_angular.x(), m_angular.y(), m_angular.z());

        // 四元数微分：d(q)/dt = 0.5 * ω_q * q
        Eigen::Quaterniond deltaQ = omegaQuat * currentRot;
        deltaQ.coeffs() *= 0.5;
        currentRot.coeffs() += deltaQ.coeffs() * time;
        currentRot.normalize();

        m_transform.setRotation(currentRot);
    }

    double RigidBody::getRotationalInertia(const Axis& axis)
    {
        const Eigen::Vector3d dir = axis.rotationAxis.normalized();
        const Eigen::Vector3d rel = m_transform.getTranslation() - axis.basePoint;
        const double distance = rel.cross(dir).norm();
        return distance * distance * m_mass;
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

    Eigen::Vector3d RigidBody::getJoinForce()
    {
        Force joinForce{};
        for (const auto& [force, _2] : m_forces) {
            joinForce.force += force;
        }
        return joinForce.force;
    }

    Eigen::Vector3d RigidBody::getMomentum()
    {
        return m_mass * m_speed;
    }

    Eigen::Vector3d RigidBody::getAngularMomentum()
    {
        if (!m_shape) {
            return Eigen::Vector3d::Zero();
        }
        const Eigen::Matrix3d I_local = m_shape->getInertiaTensor(m_mass);
        const Eigen::Matrix3d R = m_transform.getRotationMatrix();
        const Eigen::Matrix3d I_world = R * I_local * R.transpose();

        return I_world * m_angular;
    }

    std::unique_ptr<PhysicsAbstract> RigidBody::clone(std::uint64_t id) const
    {
        auto entity = std::make_unique<RigidBody>(*this);
        entity->setId(id);
        return entity;
    }
} // egret
