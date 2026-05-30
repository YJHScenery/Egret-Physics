/**
* @file        shape_cylindrical_shell.cpp
 * @brief       圆柱壳形刚体形状实现文件，定义圆柱壳形状。
 * @details     实现 ShapeCylindricalShell 类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-30
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "shape_cylindrical_shell.h"

#include "world_scene_manager.h"

namespace egret
{
    ShapeCylindricalShell::ShapeCylindricalShell(const double radius, const double mass) : ShapeCylinder(radius, mass)
    {
    }

    ShapeType ShapeCylindricalShell::typeId() const
    {
        return ShapeType::CylindricalShell;
    }

    Eigen::Matrix3d ShapeCylindricalShell::getInertiaTensor(double mass) const
    {
        const double i_xx{(0.5 * quickSquare(m_radius) + 1.0 / 12.0 * quickSquare(m_height)) * mass};
        const double i_yy{i_xx};

        const double i_zz{quickSquare(m_radius) * mass};

        Eigen::Matrix3d inertiaTensor;
        inertiaTensor(0, 0) = i_xx;
        inertiaTensor(1, 1) = i_yy;
        inertiaTensor(2, 2) = i_zz;
        return inertiaTensor;
    }

    ShapeLoadInfo ShapeCylindricalShell::getLoadInfo() const
    {
        ShapeLoadInfo info{};
        info.typeId = static_cast<std::uint32_t>(typeId());
        info.numberParams["radius"] = std::vector<double>{getRadius()};
        info.numberParams["height"] = std::vector<double>{getHeight()};
        return info;
    }
} // egret
