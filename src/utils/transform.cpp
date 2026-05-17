//
// Created by jehor on 2026/4/25.
//
#include "transform.h"
#include "basic_utils.h"

namespace egret
{
    Eigen::Matrix3d Transform::getRotationMatrix() const
    {
        // const std::function<double(double)> squareD<double, int, 2>{[](const double x) { return x * x; }};

        const double x{m_rotation.x()};
        const double y{m_rotation.y()};
        const double z{m_rotation.z()};
        const double w{m_rotation.w()};

        Eigen::Matrix3d rotationMat{};

        rotationMat << 1.0 - 2.0 * (quickPower<double, int, 2>(y) + quickPower<double, int, 2>(z)), 2.0 * (x * y - w * z), 2.0 * (x * z + w * y),
            2.0 * (x * y + w * z), 1.0 - 2.0 * (quickPower<double, int, 2>(x) + quickPower<double, int, 2>(z)), 2.0 * (y * z - w * x),
            2.0 * (x * z - w * y), 2.0 * (y * z + w * x), 1.0 - 2.0 * (quickPower<double, int, 2>(x) + quickPower<double, int, 2>(y));

        return rotationMat;
    }

    void Transform::setTranslation(const Eigen::Vector3d& translation)
    {
        m_translation = translation;
        updateMatrix();
    }

    void Transform::setRotation(const Eigen::Quaterniond& rotation)
    {
        m_rotation = rotation.normalized();
        updateMatrix();
    }

    void Transform::setScale(const Eigen::Vector3d& scale)
    {
        m_scale = scale;
        updateMatrix();
    }

    void Transform::setTRS(const Eigen::Vector3d& translation, const Eigen::Quaterniond& rotation,
        const Eigen::Vector3d& scale)
    {
        m_translation = translation;
        m_rotation = rotation.normalized();
        m_scale = scale;
        updateMatrix();
    }

    Eigen::Vector3d Transform::localToWorld(const Eigen::Vector3d& localPoint) const
    {
        return m_translation + m_rotation * localPoint;
    }

    Eigen::Vector3d Transform::worldToLocal(const Eigen::Vector3d& point) const
    {
        return m_rotation.conjugate() * (point - m_translation);
    }

    Eigen::Vector3d Transform::transformPointToWorld(const Eigen::Vector3d& localPoint) const
    {
        const Eigen::Vector4d local(localPoint.x(), localPoint.y(), localPoint.z(), 1.0);
        return (m_localToWorld * local).head<3>();
    }

    Eigen::Vector3d Transform::transformPointToLocal(const Eigen::Vector3d& worldPoint) const
    {
        const Eigen::Vector4d world(worldPoint.x(), worldPoint.y(), worldPoint.z(), 1.0);
        return (m_worldToLocal * world).head<3>();
    }

    void Transform::updateMatrix()
    {
        Eigen::Matrix4d S = Eigen::Matrix4d::Identity();
        S(0, 0) = m_scale.x();
        S(1, 1) = m_scale.y();
        S(2, 2) = m_scale.z();

        Eigen::Matrix4d R = Eigen::Matrix4d::Identity();
        R.block<3, 3>(0, 0) = m_rotation.toRotationMatrix();

        Eigen::Matrix4d T = Eigen::Matrix4d::Identity();
        T(0, 3) = m_translation.x();
        T(1, 3) = m_translation.y();
        T(2, 3) = m_translation.z();

        m_localToWorld = T * R * S;  // 顺序：缩放 -> 旋转 -> 平移
        m_worldToLocal = m_localToWorld.inverse();
    }
}
