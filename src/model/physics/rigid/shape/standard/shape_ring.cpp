//
// Created by jehor on 2026/4/24.
//

#include "shape_ring.h"

#include "world_scene_manager.h"

namespace egret
{
    ShapeRing::ShapeRing(const double radius): ShapeDisk(radius)
    {
    }

    ShapeType ShapeRing::typeId() const
    {
        return ShapeType::Ring;
    }

    Eigen::Matrix3d ShapeRing::getInertiaTensor(const double mass) const
    {
        return ShapeDisk::getInertiaTensor(mass) * 2.0;
    }

    ShapeLoadInfo ShapeRing::getLoadInfo() const
    {
        ShapeLoadInfo info{};
        info.typeId = static_cast<std::uint32_t>(typeId());
        info.numberParams["radius"] = std::vector<double>{getRadius()};
        return info;
    }


} // egret