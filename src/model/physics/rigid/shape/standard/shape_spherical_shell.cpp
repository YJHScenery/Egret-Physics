//
// Created by jehor on 2026/4/24.
//

#include "shape_spherical_shell.h"

#include "constants.h"
#include "world_scene_manager.h"

namespace egret
{
    ShapeID ShapeSphericalShell::typeId() const
    {
        return ShapeID::SphericalShell;
    }

    Eigen::Matrix3d ShapeSphericalShell::getInertiaTensor(double mass) const
    {
        const double inertia {2.0 / 3.0 * mass * quickSquare(m_radius)};
        return Eigen::Matrix3d::Identity() * inertia;
    }

    SceneRenderItem ShapeSphericalShell::getBasicRenderInfo(const Eigen::Vector3d& position) const
    {
        SceneRenderItem item = ShapeSphere::getBasicRenderInfo(position);
        item.kind = (std::uint32_t)this->typeId();
        return item;
    }

} // egret