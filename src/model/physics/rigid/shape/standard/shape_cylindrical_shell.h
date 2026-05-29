//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_SHAPE_CYLINDRICAL_SHELL_H
#define EGRET_PHYSICS_SHAPE_CYLINDRICAL_SHELL_H
#include "shape_base.h"
#include "shape_cylinder.h"

namespace egret {

class ShapeCylindricalShell: public ShapeCylinder {
public:
    ShapeCylindricalShell(double radius, double mass);

    [[nodiscard]] ShapeID typeId() const override;

    [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

    [[nodiscard]] ShapeLoadInfo getLoadInfo() const override;



};

} // egret

#endif //EGRET_PHYSICS_SHAPE_CYLINDRICAL_SHELL_H
