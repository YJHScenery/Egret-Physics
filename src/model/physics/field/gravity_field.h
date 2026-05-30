/**
* @file        gravity_field.h
 * @brief       重力场头文件
 * @details     定义GravityField类，提供均匀重力场功能，
 *              可以设置重力加速度和参考点。
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

#ifndef EGRET_PHYSICS_GRAVITY_FIELD_H
#define EGRET_PHYSICS_GRAVITY_FIELD_H
#include "field_base.h"
#include "constants.h"

namespace egret {

/**
 * @brief       重力场类，实现均匀重力场。
 * @details     GravityField 继承自 FieldBase，提供均匀重力场功能，
 *              可以设置重力加速度和参考点。
 *              重力场是保守场，势能只与高度相关。
 *              重力场在空间中各点的场向量相同（均匀场）。
 * 
 * @invariant   m_gravity 是重力加速度向量（通常向下）
 * @invariant   m_zeroReferencePoint 是势能零点参考位置
 * @remark      重力场是均匀场，场向量处处相同，适合模拟地面附近的物理场景
 * @see         FieldBase, GravitationalField, PhysicalEntity
 */
class GravityField: public FieldBase {
public:
    GravityField();

    ~GravityField() override = default;

    explicit GravityField(Eigen::Vector3d  gravity, Eigen::Vector3d  referencePoint);

    explicit GravityField(double gravity, double referenceZ);

    Eigen::Vector3d sample(const Eigen::Vector3d& position) override;

    double potential(const Eigen::Vector3d& position) override;

    void applyToEntity(PhysicalEntity* entity) override;

private:
    // static constexpr char FIELD_FLAG_GRAVITY[] {"FIELD_BASE"};

    Eigen::Vector3d m_gravity{0, 0, -STANDARD_GRAVITY};

    Eigen::Vector3d m_zeroReferencePoint{};
};

} // egret

#endif //EGRET_PHYSICS_GRAVITY_FIELD_H
