/**
* @file        field_base.cpp
 * @brief       物理场基类实现文件
 * @details     实现FieldBase类的各项成员函数。
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

#include "field_base.h"

namespace egret
{
    FieldBase::FieldBase(): m_id(generateID(FieldType::Abstract))
    {
        ++m_createCount_static;
    }

    FieldBase::FieldBase(const std::uint64_t id): m_id(id)
    {
        ++m_createCount_static;
    }

    void FieldBase::setEnabled(const bool enable)
    {
        m_enabled = enable;
    }

    std::uint64_t FieldBase::generateID(FieldType fieldType)
    {
        return static_cast<std::uint64_t>(fieldType) << 16 | m_createCount_static;
    }
} // egret