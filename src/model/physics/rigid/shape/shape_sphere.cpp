//
// Created by jehor on 2026/4/24.
//

#include "shape_sphere.h"

#include "constants.h"

namespace egret
{
    void ShapeSphere::setRadius(const double radius)
    {
        m_radius = radius;
    }

    double ShapeSphere::getVolume() const
    {
        return 4.0 / 3.0 * pow(m_radius, 3) * PI;
    }

    Eigen::Vector3d ShapeSphere::getCenterOfMass() const
    {
        return {0, 0, 0};
    }

    Eigen::Matrix3d ShapeSphere::getInertiaTensor(double mass) const
    {
        const double inertia {0.4 * mass * pow(m_radius, 2)};
        return Eigen::Matrix3d::Identity() * inertia;
    }

    bool ShapeSphere::collideWith(const ShapeBase* other, const Transform& thisTrans, const Transform& otherTrans,
        ContactManifold& manifold) const
    {
        const auto* shapeSphere = dynamic_cast<const ShapeSphere*>(other);
        if (shapeSphere != nullptr) {
            const Eigen::Vector3d& thisGeomCenter{thisTrans.getTranslation()};
            const Eigen::Vector3d& otherGeomCenter{otherTrans.getTranslation()};
            const double distance {(thisGeomCenter - otherGeomCenter).norm()};
            const double sumRadius {m_radius + shapeSphere->getRadius()};
            if (distance >= sumRadius) {
                return false;
            }

            Eigen::Vector3d normal = otherGeomCenter - thisGeomCenter;
            if (normal.squaredNorm() < 1e-12) {
                normal = Eigen::Vector3d::UnitX();
            } else {
                normal.normalize();
            }
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

    AABB ShapeSphere::getAABB(const Transform& transform) const
    {
        const Eigen::Vector3d extent = Eigen::Vector3d::Ones() * m_radius;
        const Eigen::Vector3d& center = transform.getTranslation();
        return AABB{
            .min = center - extent,
            .max = center + extent,
        };
    }
} // egret