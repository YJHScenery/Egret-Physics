/**
* @file        constraints_base.cpp
 * @brief       约束器基类实现文件
 * @details     实现ConstraintsBase类的各项成员函数。
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

#include "constraints_base.h"

#include "physical_entity.h"

namespace egret
{
    ConstraintsBase::ConstraintsBase()
        : PhysicsAbstract(0)
    {
    }

    ConstraintsBase::ConstraintsBase(std::uint64_t id)
        : PhysicsAbstract(id)
    {
    }

    ConstraintType ConstraintsBase::getType() const
    {
        return ConstraintType::Abstract;
    }

    const std::unordered_set<PhysicalEntity*>& ConstraintsBase::getConstrainedEntities() const
    {
        return m_registeredEntities;
    }

    void ConstraintsBase::registerEntity(PhysicalEntity* entity)
    {
        if (entity == nullptr) {
            return;
        }
        const auto [it, inserted] = m_registeredEntities.insert(entity);
        if (inserted) {
            m_entityIdMap[entity->getId()] = entity;
        }
    }

    bool ConstraintsBase::unregisterEntity(std::uint64_t entityId)
    {
        auto it = m_entityIdMap.find(entityId);
        if (it == m_entityIdMap.end()) {
            return false;
        }
        m_registeredEntities.erase(it->second);
        m_entityIdMap.erase(it);
        return true;
    }

    bool ConstraintsBase::isRegistered(std::uint64_t entityId) const
    {
        return m_entityIdMap.find(entityId) != m_entityIdMap.end();
    }

    void ConstraintsBase::applyConstraintForces()
    {
        // 默认空实现，力型约束（如 Spring）重写此方法
    }

    void ConstraintsBase::applyConstraintTorques()
    {
        // 默认空实现
    }

    void ConstraintsBase::setEnabled(bool enabled)
    {
        m_enabled = enabled;
    }

    bool ConstraintsBase::isEnabled() const
    {
        return m_enabled;
    }

} // egret
