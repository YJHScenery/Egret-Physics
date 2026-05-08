//
// Created by jehor on 2026/4/24.
//

#include "shape_cylinder.h"
#include "constants.h"

namespace egret
{
    ShapeCylinder::ShapeCylinder(double radius, double height): m_radius(radius), m_height(height)
    {
    }

    const std::string& ShapeCylinder::typeId() const
    {
        static std::string typeId = TYPE_ID_STANDARD_CYLINDER;
        return typeId;
    }

    double ShapeCylinder::getVolume() const
    {
        return m_height * pow(m_radius, 2) * PI;
    }

    Eigen::Vector3d ShapeCylinder::getCenterOfMass() const
    {
        return {0.0, 0.0, 0.0};
    }

    Eigen::Matrix3d ShapeCylinder::getInertiaTensor(double mass) const
    {
        const double i_xx{1.0 / 12.0 * (3 * pow(m_radius, 2) + pow(m_height, 2)) * mass};
        const double i_yy{i_xx};

        const double i_zz{0.5 * pow(m_radius, 2) * mass};

        Eigen::Matrix3d inertiaTensor;
        inertiaTensor(0, 0) = i_xx;
        inertiaTensor(1, 1) = i_yy;
        inertiaTensor(2, 2) = i_zz;
        return inertiaTensor;
    }

    AABB ShapeCylinder::getAABB(const Transform& transform) const
    {
        // Eigen::Matrix3d rotationMat{};
        const std::function<double(double)> squareD{[](const double x) { return x * x; }};

        // const Eigen::Quaterniond& rQuat{transform.getRotation()};
        // const double x{rQuat.x()};
        // const double y{rQuat.y()};
        // const double z{rQuat.z()};
        // const double w{rQuat.w()};

        const Eigen::Vector3d& sVec{transform.getScale()};

        const Eigen::Vector3d tVec{transform.getTranslation()};

        // rotationMat << 1.0 - 2.0 * (squareD(y) + squareD(z)), 2.0 * (x * y - w * z), 2.0 * (x * z + w * y),
        //     2.0 * (x * y + w * z), 1.0 - 2.0 * (squareD(x) + squareD(z)), 2.0 * (y * z - w * x),
        //     2.0 * (x * z - w * y), 2.0 * (y * z + w * x), 1.0 - 2.0 * (squareD(x) + squareD(y));

        std::array<double, 3> bias{};
        for (int i = 0; i < 3; i++) {
            const Eigen::Vector3d uVec{transform.getRotationMatrix().row(i)};
            bias[i] = m_radius * sqrt(
                squareD(sVec.x() * uVec.x()) + squareD(sVec.y() * uVec.y())
            ) + m_height / 2.0 * std::abs(sVec.z() * uVec.z());
        }


        const Eigen::Vector3d minVec{tVec.x() - bias[0], tVec.y() - bias[1], tVec.z() - bias[2]};
        const Eigen::Vector3d maxVec{tVec.x() + bias[0], tVec.y() + bias[1], tVec.z() + bias[2]};
        return AABB{minVec, maxVec};
    }

    void ShapeCylinder::setRadius(const double radius)
    {
        m_radius = radius;
    }

    void ShapeCylinder::setHeight(const double height)
    {
        m_height = height;
    }

} // egret
