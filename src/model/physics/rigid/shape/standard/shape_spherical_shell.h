//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_SHAPE_SPHERICAL_SHELL_H
#define EGRET_PHYSICS_SHAPE_SPHERICAL_SHELL_H
#include "shape_base.h"
#include "shape_sphere.h"

namespace egret {

class ShapeSphericalShell: public ShapeSphere {
public:
    explicit ShapeSphericalShell(const double radius): ShapeSphere(radius) {}

    [[nodiscard]] ShapeID typeId() const override;

    [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

};

} // egret

#endif //EGRET_PHYSICS_SHAPE_SPHERICAL_SHELL_H
