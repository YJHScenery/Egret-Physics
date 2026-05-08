//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_SHAPE_CUBE_H
#define EGRET_PHYSICS_SHAPE_CUBE_H
#include "shape_base.h"

namespace egret
{
    class ShapeSphere;

    class ShapeBox : public ShapeBase
    {
    public:
        explicit ShapeBox(Eigen::Vector3d size);

        [[nodiscard]] const std::string& typeId() const override;


        ShapeBox(double length, double width, double height);

        void setSize(Eigen::Vector3d size);

        void setSize(double length, double width, double height);

        [[nodiscard]] Eigen::Vector3d getHalfSize() const;

        [[nodiscard]] Eigen::Vector3d getSize() const;

        [[nodiscard]] double getLength() const;

        [[nodiscard]] double getWidth() const;

        [[nodiscard]] double getHeight() const;

        [[nodiscard]] double getVolume() const override;

        [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override;

        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

        // [[nodiscard]] bool collideWith(const ShapeBase* other,
        //                                const Transform& thisTrans, const Transform& otherTrans,
        //                                ContactManifold& manifold) const override;

        [[nodiscard]] AABB getAABB(const Transform& transform) const override;

        static void getLocalAxes(const Transform& trans,
                                 Eigen::Vector3d& axisX,
                                 Eigen::Vector3d& axisY,
                                 Eigen::Vector3d& axisZ);
        //
        // static bool collideBoxSphere(const ShapeBox& box, const Transform& boxTrans,
        //                              const ShapeSphere& sphere, const Transform& sphereTrans,
        //                              ContactManifold& manifold);
        //
        // static bool collideBoxBox(const ShapeBox& boxA, const Transform& transA,
        //                           const ShapeBox& boxB, const Transform& transB,
        //                           ContactManifold& manifold);

    private:
        Eigen::Vector3d m_size{};
    };
} // egret

#endif //EGRET_PHYSICS_SHAPE_CUBE_H
