//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_SHAPE_SPHERE_H
#define EGRET_PHYSICS_SHAPE_SPHERE_H
#include "shape_base.h"

namespace egret
{
    class ShapeSphere : public ShapeBase
    {
    public:
        explicit ShapeSphere(const double radius): m_radius{radius} {}

        void setRadius(double radius);

        [[nodiscard]] const std::string& typeId() const override;

        [[nodiscard]] double getVolume() const override;

        [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override;

        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

        // [[nodiscard]] bool collideWith(const ShapeBase* other, const Transform& thisTrans, const Transform& otherTrans,
        //                                ContactManifold& manifold) const override;

        [[nodiscard]] AABB getAABB(const Transform& transform) const override;

        [[nodiscard]] double getRadius() const {return m_radius;}

    protected:
        double m_radius{};
    };
} // egret

#endif //EGRET_PHYSICS_SHAPE_SPHERE_H
