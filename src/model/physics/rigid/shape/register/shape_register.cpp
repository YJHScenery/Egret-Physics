//
// Created by jehor on 2026/5/6.
//

#include "shape_register.h"
#include "shape_base.h"

namespace egret
{
    ShapeRegister& ShapeRegister::instance()
    {
        static ShapeRegister shape_register;
        return shape_register;
    }

    void ShapeRegister::registerJudge(std::uint32_t id1, std::uint32_t id2, CollideFunc func)
    {
        const auto key = std::make_pair(fmin(id1, id2), fmax(id1, id2));
        m_collideRegistry[key] = std::move(func);
    }

    void ShapeRegister::registerContinuousJudge(std::uint32_t id1, std::uint32_t id2, ContiCollideFunc func)
    {
        const auto key = std::make_pair(fmin(id1, id2), fmax(id1, id2));
        m_continuousJudge[key] = std::move(func);
    }

    CollideFunc* ShapeRegister::findJudge(std::uint32_t id1, std::uint32_t id2)
    {
        const auto key = std::make_pair(fmin(id1, id2), fmax(id1, id2));
        const auto it = m_collideRegistry.find(key);
        return it != m_collideRegistry.end() ? &it->second : nullptr;
    }

    ContiCollideFunc* ShapeRegister::findContinuousJudge(std::uint32_t id1, std::uint32_t id2)
    {
        const auto key = std::make_pair(fmin(id1, id2), fmax(id1, id2));
        const auto it = m_continuousJudge.find(key);
        return it != m_continuousJudge.end() ? &it->second : nullptr;
    }
} // egret