//
// Created by jehor on 2026/4/23.
//

#include "particle.h"

#include <cmath>

namespace egret
{
    Particle::Particle(const Eigen::Vector3d& position, const Eigen::Vector3d& speed, double mass): PhysicalEntity(position, speed, mass),
        m_mass(mass)
    {
    }

    void Particle::applyForce(const double time)
    {
        Eigen::Vector3d acceleration{};
        if (m_mass != 0) {
            acceleration =  getJoinForce().force / m_mass;
        }else {
            acceleration = {0, 0, 0};
        }

        m_position += m_speed * time + 0.5 * acceleration * time * time;
        m_speed += acceleration * time;
    }

    void Particle::movePosition(const double time)
    {
        m_position += m_speed * time;
    }

    void Particle::rotateMandatory(const Axis& axis, double radians)
    {
        // 1. 将旋转轴方向归一化
        Eigen::Vector3d u = axis.rotationAxis.normalized();

        // 2. 将旋转平移到原点（将点相对于轴上的基点表示）
        const Eigen::Vector3d p_rel = m_position - axis.basePoint;

        // 3. 构建旋转矩阵（罗德里格斯旋转公式）
        // R = I + sin(theta)*[u]_× + (1-cos(theta))*[u]_×^2

        const double cos_theta = std::cos(radians);
        const double sin_theta = std::sin(radians);

        // 叉积矩阵 [u]_×
        Eigen::Matrix3d cross_matrix;
        cross_matrix << 0, -u(2), u(1),
            u(2), 0, -u(0),
            -u(1), u(0), 0;

        // 旋转矩阵
        const Eigen::Matrix3d rotation_matrix = Eigen::Matrix3d::Identity()
            + sin_theta * cross_matrix
            + (1 - cos_theta) * (cross_matrix * cross_matrix);

        // 4. 应用旋转
        const Eigen::Vector3d p_rotated = rotation_matrix * p_rel;

        // 5. 平移回原位
        m_position = p_rotated + axis.basePoint;
    }

    double Particle::getRotationalInertia(const Axis& axis)
    {
        // 原代码：
        // const double distance{axis.basePoint.cross(axis.rotationAxis).norm() / axis.basePoint.norm()};
        // return pow(distance, 2) * m_mass;
        // 新代码说明：避免轴基点接近原点时发生除零，并保持点质点惯性模型的一致性。
        const Eigen::Vector3d direction = axis.rotationAxis.normalized();
        const Eigen::Vector3d relativePosition = m_position - axis.basePoint;
        const double distance = relativePosition.cross(direction).norm();
        return std::pow(distance, 2.0) * m_mass;
    }

    Eigen::Vector3d Particle::getTorque(const Eigen::Vector3d& base)
    {
        return (m_position - base).cross(getJoinForce().force);
    }

    Force Particle::getJoinForce()
    {
        Force joinForce{};
        // 原代码：
        // for (const auto& [_, force] : m_forces) {
        //     joinForce.force += force;
        // }
        // 新代码说明：Force 不是二元组，改为按成员遍历，确保可正确汇总所有外力。
        for (const Force& force : m_forces) {
            joinForce.force += force.force;
        }
        return joinForce;
    }

    Eigen::Vector3d Particle::getMomentum()
    {
        return m_mass * m_speed;
    }

    Eigen::Vector3d Particle::getAngularMomentum(const Eigen::Vector3d& base)
    {
        return (m_position - base).cross(getMomentum());
    }
} // egret
