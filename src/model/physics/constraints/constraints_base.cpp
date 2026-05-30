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

namespace egret
{
    ConstraintsBase::ConstraintsBase() : m_id(0)
    {
    }

    ConstraintsBase::ConstraintsBase(std::uint64_t id) : m_id(id)
    {
    }

    ConstraintType ConstraintsBase::getType() const
    {
        return ConstraintType::ConnectingLine;
    }

    std::vector<PhysicalEntity*> ConstraintsBase::getConstrainedEntities()
    {
        return m_physicalEntities;
    }

    const std::vector<PhysicalEntity*>& ConstraintsBase::getConstrainedEntities() const
    {
        return m_physicalEntities;
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
