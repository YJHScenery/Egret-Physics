/**
* @file        filename.h
 * @brief       一句话概述文件的主要功能
 * @details     详细描述文件的用途、设计思路、使用注意事项等。
 *              可以写多行，说明模块在整个系统中的位置。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        创建日期 (如 2025-03-15)
 * @version     版本号 (如 1.0.0)
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     许可证 (如 GPL, MIT, BSD 等)
 *
 * @ingroup     所属模块组 (可选，用于分组)
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#include "shape_spherical_shell.h"

#include "constants.h"
#include "world_scene_manager.h"

namespace egret
{
    ShapeType ShapeSphericalShell::typeId() const
    {
        return ShapeType::SphericalShell;
    }

    Eigen::Matrix3d ShapeSphericalShell::getInertiaTensor(double mass) const
    {
        const double inertia {2.0 / 3.0 * mass * quickSquare(m_radius)};
        return Eigen::Matrix3d::Identity() * inertia;
    }


} // egret