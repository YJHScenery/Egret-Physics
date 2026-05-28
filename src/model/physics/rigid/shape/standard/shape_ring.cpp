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

    ShapeID ShapeRing::typeId() const
    {
        return ShapeID::Ring;
    }

    Eigen::Matrix3d ShapeRing::getInertiaTensor(const double mass) const
    {
        return ShapeDisk::getInertiaTensor(mass) * 2.0;
    }

    SceneRenderItem ShapeRing::getBasicRenderInfo(const Eigen::Vector3d& position) const
    {
        SceneRenderItem item = ShapeDisk::getBasicRenderInfo(position);
        item.kind = (std::uint32_t)this->typeId();
        return item;
    }

} // egret