//
// Created by jehor on 2026/4/24.
//

#include "shape_cylindrical_shell.h"

#include "world_scene_manager.h"

namespace egret
{
    ShapeCylindricalShell::ShapeCylindricalShell(const double radius, const double mass) : ShapeCylinder(radius, mass)
    {
    }

    const std::string& ShapeCylindricalShell::typeId() const
    {
        static std::string typeId = TYPE_ID_STANDARD_CYLINDRICAL_SHELL;
        return typeId;
    }

    Eigen::Matrix3d ShapeCylindricalShell::getInertiaTensor(double mass) const
    {
        const double i_xx{(0.5 * pow(m_radius, 2) + 1.0 / 12.0 * pow(m_height, 2)) * mass};
        const double i_yy{i_xx};

        const double i_zz{pow(m_radius, 2) * mass};

        Eigen::Matrix3d inertiaTensor;
        inertiaTensor(0, 0) = i_xx;
        inertiaTensor(1, 1) = i_yy;
        inertiaTensor(2, 2) = i_zz;
        return inertiaTensor;
    }

    SceneRenderItem ShapeCylindricalShell::getBasicRenderInfo(const Eigen::Vector3d& position) const
    {
        SceneRenderItem item = ShapeCylinder::getBasicRenderInfo(position);
        item.kind = this->typeId();
        return item;
    }
} // egret
