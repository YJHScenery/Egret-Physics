//
// Created by jehor on 2026/5/30.
//

#include "physics_abstract.h"

namespace egret
{
    void PhysicsAbstract::setId(const std::uint64_t id)
    {
        m_id = id;
    }

    std::uint64_t PhysicsAbstract::getId() const
    {
        return m_id;
    }
} // egret