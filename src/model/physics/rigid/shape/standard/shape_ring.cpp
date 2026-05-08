//
// Created by jehor on 2026/4/24.
//

#include "shape_ring.h"

namespace egret
{
    ShapeRing::ShapeRing(const double radius): ShapeDisk(radius)
    {
    }

    const std::string& ShapeRing::typeId() const
    {
        static std::string typeId {TYPE_ID_STANDARD_RING};
        return typeId;
    }

    Eigen::Matrix3d ShapeRing::getInertiaTensor(const double mass) const
    {
        return ShapeDisk::getInertiaTensor(mass) * 2.0;
    }

} // egret