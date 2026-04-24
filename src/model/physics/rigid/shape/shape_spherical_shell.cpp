//
// Created by jehor on 2026/4/24.
//

#include "shape_spherical_shell.h"

#include "constants.h"

namespace egret
{
    void ShapeSphericalShell::setRadius(double radius)
    {
        m_radius = radius;
    }

    double ShapeSphericalShell::getVolume() const
    {
        return 4.0 / 3.0 * pow(m_radius, 3) * PI;
    }

    Eigen::Vector3d ShapeSphericalShell::getCenterOfMass() const
    {
        return Eigen::Vector3d(0, 0, 0);
    }

    Eigen::Matrix3d ShapeSphericalShell::getInertiaTensor(double mass) const
    {
        const double inertia {2.0 / 3.0 * mass * pow(m_radius, 2)};
        return Eigen::Matrix3d::Identity() * inertia;
    }

    bool ShapeSphericalShell::collideWith(const ShapeBase* other, const Transform& thisTrans,
                                          const Transform& otherTrans, ContactManifold& manifold) const
    {
        const ShapeSphericalShell* shapeSphere = dynamic_cast<const ShapeSphericalShell*>(other);
        if (shapeSphere != nullptr) {
            const Eigen::Vector3d& thisGeomCenter{thisTrans.translation};
            const Eigen::Vector3d& otherGeomCenter{otherTrans.translation};
            const double distance {(thisGeomCenter - otherGeomCenter).norm()};
            const double sumRadius {m_radius + shapeSphere->getRadius()};
            if (distance < sumRadius) {
                return false;
            }
            const Eigen::Vector3d normal{(thisGeomCenter - otherGeomCenter).normalized()};
            const double penetration {sumRadius - distance};

            const Eigen::Vector3d contactPoint {thisGeomCenter + normal * (m_radius - penetration * 0.5)};
            const ContactPoint pointInfo{
                .position = contactPoint, .normal = normal, .penetration = penetration
            };
            manifold.push_back(pointInfo);
            return true;
        }
        return false;
    }

    AABB ShapeSphericalShell::getAABB(const Transform& transform) const
    {
        return AABB{
            .min = transform.translation - Eigen::Vector3d{}.setOnes() * m_radius,
            .max = transform.translation + Eigen::Vector3d{}.setOnes() * m_radius,
        };
    }
} // egret