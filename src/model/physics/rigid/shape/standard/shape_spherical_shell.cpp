//
// Created by jehor on 2026/4/24.
//

#include "shape_spherical_shell.h"

#include "constants.h"
#include "world_scene_manager.h"

namespace egret
{
    const std::string& ShapeSphericalShell::typeId() const
    {
        static std::string typeId {TYPE_ID_STANDARD_SPHERICAL_SHELL};
        return typeId;
    }

    Eigen::Matrix3d ShapeSphericalShell::getInertiaTensor(double mass) const
    {
        const double inertia {2.0 / 3.0 * mass * pow(m_radius, 2)};
        return Eigen::Matrix3d::Identity() * inertia;
    }

    SceneRenderItem ShapeSphericalShell::getBasicRenderInfo(const Eigen::Vector3d& position) const
    {
        SceneRenderItem item = ShapeSphere::getBasicRenderInfo(position);
        item.kind = this->typeId();
        return item;
    }

} // egret