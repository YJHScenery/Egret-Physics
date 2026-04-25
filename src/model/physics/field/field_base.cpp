//
// Created by jehor on 2026/4/25.
//

#include "field_base.h"

namespace egret
{
    FieldBase::FieldBase(): m_id(generateID(FieldType::Base))
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
        return static_cast<std::uint64_t>(fieldType) | m_createCount_static;
    }
} // egret