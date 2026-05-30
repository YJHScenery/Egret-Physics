//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_SHAPE_RING_H
#define EGRET_PHYSICS_SHAPE_RING_H
#include "shape_base.h"
#include "shape_disk.h"

namespace egret {

class ShapeRing: public ShapeDisk {

public:
    explicit ShapeRing(double radius);

    [[nodiscard]] ShapeType typeId() const override;

    [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

    [[nodiscard]] ShapeLoadInfo getLoadInfo() const override;

};

} // egret

#endif //EGRET_PHYSICS_SHAPE_RING_H
