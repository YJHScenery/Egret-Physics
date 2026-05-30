/**
 * @file        shape_ring.cpp
 * @brief       圆环形刚体形状实现文件，定义圆环形状。
 * @details     实现 ShapeRing 类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-26
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "shape_ring.h"

#include "world_scene_manager.h"

namespace egret
{
    ShapeRing::ShapeRing(const double radius) : ShapeDisk(radius)
    {
    }

    ShapeType ShapeRing::getType() const
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
        info.typeId = static_cast<std::uint32_t>(getType());
        info.numberParams["radius"] = std::vector<double>{getRadius()};
        return info;
    }

} // egret