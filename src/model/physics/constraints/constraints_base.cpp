//
// Created by jehor on 2026/4/25.
//

#include "constraints_base.h"

namespace egret
{
    ConstraintsBase::ConstraintsBase()
    {
        ++m_createCount_static;
    }

    ConstraintsBase::ConstraintsBase(std::uint64_t id): m_id(id)
    {
        ++m_createCount_static;
    }

    ConstraintType ConstraintsBase::getType() const
    {
        return ConstraintType::ConnectingLine;
    }

    std::uint64_t ConstraintsBase::generateID(ConstraintType type)
    {
        return static_cast<std::uint64_t>(type) | m_createCount_static;
    }
} // egret