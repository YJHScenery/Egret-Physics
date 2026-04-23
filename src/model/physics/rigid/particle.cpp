//
// Created by jehor on 2026/4/23.
//

#include "particle.h"

#include <numeric>

namespace egret
{
    void Particle::applyForce(const double time)
    {
        const Eigen::Vector3f acceleration{getJoinForce().force / m_mass};
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
        Eigen::Vector3f u = axis.rotationAxis.normalized();

        // 2. 将旋转平移到原点（将点相对于轴上的基点表示）
        const Eigen::Vector3f p_rel = m_position - axis.basePoint;

        // 3. 构建旋转矩阵（罗德里格斯旋转公式）
        // R = I + sin(theta)*[u]_× + (1-cos(theta))*[u]_×^2

        const double cos_theta = std::cos(radians);
        const double sin_theta = std::sin(radians);

        // 叉积矩阵 [u]_×
        Eigen::Matrix3f cross_matrix;
        cross_matrix << 0, -u(2), u(1),
            u(2), 0, -u(0),
            -u(1), u(0), 0;

        // 旋转矩阵
        const Eigen::Matrix3f rotation_matrix = Eigen::Matrix3f::Identity()
            + sin_theta * cross_matrix
            + (1 - cos_theta) * (cross_matrix * cross_matrix);

        // 4. 应用旋转
        const Eigen::Vector3f p_rotated = rotation_matrix * p_rel;

        // 5. 平移回原位
        m_position = p_rotated + axis.basePoint;
    }

    double Particle::getRotationalInertia(const Axis& axis)
    {
        const double distance{axis.basePoint.cross(axis.rotationAxis).norm() / axis.basePoint.norm()};
        return pow(distance, 2) * m_mass;
    }

    Eigen::Vector3f Particle::getTorque(const Eigen::Vector3f& base)
    {
        return (m_position - base).cross(getJoinForce().force);
    }

    Force Particle::getJoinForce()
    {
        Force joinForce{};
        for (const auto& [_, force] : m_forces) {
            joinForce.force += force;
        }
        return joinForce;
    }

    Eigen::Vector3f Particle::getMomentum()
    {
        return m_mass * m_speed;
    }

    Eigen::Vector3f Particle::getAngularMomentum(const Eigen::Vector3f& base)
    {
        return (m_position - base).cross(getMomentum());
    }
} // egret
