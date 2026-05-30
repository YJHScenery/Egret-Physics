/**
* @file        particle.cpp
 * @brief       粒子实现文件，定义最简单的物理实体。
 * @details     实现 Particle 类的各项成员函数。
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

#include "particle.h"

#include <cmath>

namespace egret
{
    Particle::Particle(const Eigen::Vector3d &position, const Eigen::Vector3d &speed, double mass) : PhysicalEntity(position, speed, mass),
                                                                                                     m_mass(mass)
    {
    }

    void Particle::applyForce(const double time)
    {
        Eigen::Vector3d acceleration{};
        if (m_mass != 0)
        {
            acceleration = getJoinForce().force / m_mass;
        }
        else
        {
            acceleration = {0, 0, 0};
        }

        Eigen::Vector3d newPosition = m_transform.getTranslation() + m_speed * time + 0.5 * acceleration * time * time;
        m_transform.setTranslation(newPosition);
        m_speed += acceleration * time;
    }

    void Particle::movePosition(const double time)
    {
        m_transform.setTranslation(m_transform.getTranslation() + m_speed * time);
    }

    void Particle::applyTorque(const double time, Eigen::Matrix4d rotation)
    {
        // 质点不存在转动，忽略力矩的作用
    }

    void Particle::rotate(const double time, Eigen::Matrix4d rotation)
    {
        // 质点不存在转动，忽略旋转
    }

    double Particle::getRotationalInertia(const Axis &axis)
    {
        // 原代码：
        // const double distance{axis.basePoint.cross(axis.rotationAxis).norm() / axis.basePoint.norm()};
        // return pow(distance, 2) * m_mass;
        // 新代码说明：避免轴基点接近原点时发生除零，并保持点质点惯性模型的一致性。
        const Eigen::Vector3d direction = axis.rotationAxis.normalized();
        const Eigen::Vector3d relativePosition = m_transform.getTranslation() - axis.basePoint;
        const double distance = relativePosition.cross(direction).norm();
        return std::pow(distance, 2.0) * m_mass;
    }

    Eigen::Vector3d Particle::getTorque()
    {
        // 质点不存在转动，返回零向量
        return Eigen::Vector3d::Zero();
    }

    Force Particle::getJoinForce()
    {
        Force joinForce{};
        // 原代码：
        // for (const auto& [_, force] : m_forces) {
        //     joinForce.force += force;
        // }
        // 新代码说明：Force 不是二元组，改为按成员遍历，确保可正确汇总所有外力。
        for (const Force &force : m_forces)
        {
            joinForce.force += force.force;
        }
        return joinForce;
    }

    Eigen::Vector3d Particle::getMomentum()
    {
        return m_mass * m_speed;
    }

    Eigen::Vector3d Particle::getAngularMomentum()
    {
        // 质点不存在转动，返回零向量
        return Eigen::Vector3d::Zero();
    }
} // egret
