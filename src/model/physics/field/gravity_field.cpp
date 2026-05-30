/**
* @file        gravity_field.cpp
 * @brief       重力场实现文件
 * @details     实现GravityField类的各项成员函数。
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

#include "gravity_field.h"

#include <utility>
#include "physical_entity.h"

namespace egret
{
    GravityField::GravityField(): FieldBase(generateID(FieldType::Gravity))
    {
    }

    GravityField::GravityField(Eigen::Vector3d  gravity, Eigen::Vector3d  referencePoint) :
        FieldBase(generateID(FieldType::Gravity)),
        m_gravity(std::move(gravity)), m_zeroReferencePoint(std::move(referencePoint))
    {
    }

    GravityField::GravityField(const double gravity, const double referenceZ) :
        FieldBase(generateID(FieldType::Gravity)),
        m_gravity({0, 0, -gravity}), m_zeroReferencePoint({0, 0, referenceZ})
    {
    }

    Eigen::Vector3d GravityField::sample(const Eigen::Vector3d& position)
    {
        std::ignore = position;
        return m_gravity;
    }

    double GravityField::potential(const Eigen::Vector3d& position)
    {
        return -m_gravity.dot(position - m_zeroReferencePoint);
    }

    void GravityField::applyToEntity(PhysicalEntity* entity)
    {
        if (entity == nullptr) {
            return;
        }

        const Force gravityForce{.force = entity->getMass() * m_gravity, .id = m_id};
        entity->upsertForce(gravityForce);

    }
} // egret
