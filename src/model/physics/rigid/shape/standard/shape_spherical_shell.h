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
#ifndef EGRET_PHYSICS_SHAPE_SPHERICAL_SHELL_H
#define EGRET_PHYSICS_SHAPE_SPHERICAL_SHELL_H
#include "shape_base.h"
#include "shape_sphere.h"

namespace egret {

/**
 * @brief       球壳形状类，表示球壳形刚体的碰撞形状。
 * @details     ShapeSphericalShell 继承自 ShapeSphere，表示一个以原点为中心的
 *              球壳形状，用于物理碰撞检测和转动惯量计算。
 *              球壳是球体的空心版本，质量分布在表面而非内部。
 *              球壳的转动惯量与实心球体不同（I = 2/3 mr² vs 2/5 mr²）。
 * 
 * @invariant   m_radius > 0，半径必须为正值
 * @invariant   typeId() 返回 ShapeType::SphericalShell
 * @invariant   质心位于原点 (0, 0, 0)
 * @remark      球壳是空心形状，适合模拟空心球形物体如气球、空心球
 * @see         ShapeSphere, ShapeBase, Transform
 */
class ShapeSphericalShell: public ShapeSphere {
public:
    explicit ShapeSphericalShell(const double radius): ShapeSphere(radius) {}

    [[nodiscard]] ShapeType getType() const override;

    [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

};

} // egret

#endif //EGRET_PHYSICS_SHAPE_SPHERICAL_SHELL_H
