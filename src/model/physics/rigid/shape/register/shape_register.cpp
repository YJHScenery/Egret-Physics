/**
* @file        shape_register.cpp
 * @brief       形状注册器实现文件
 * @details     实现ShapeRegister类的各项成员函数。
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
        const std::pair<std::uint32_t, std::uint32_t> key = std::make_pair(std::min(id1, id2), std::max(id1, id2));
        m_collideRegistry[key] = std::move(func);
    }

    void ShapeRegister::registerContinuousJudge(std::uint32_t id1, std::uint32_t id2, ContiCollideFunc func)
    {
        const std::pair<std::uint32_t, std::uint32_t> key = std::make_pair(std::min(id1, id2), std::max(id1, id2));
        m_continuousJudge[key] = std::move(func);
    }

    CollideFunc* ShapeRegister::findJudge(std::uint32_t id1, std::uint32_t id2)
    {
        const std::pair<std::uint32_t, std::uint32_t> key = std::make_pair(std::min(id1, id2), std::max(id1, id2));
        const auto it = m_collideRegistry.find(key);
        return it != m_collideRegistry.end() ? &it->second : nullptr;
    }

    ContiCollideFunc* ShapeRegister::findContinuousJudge(std::uint32_t id1, std::uint32_t id2)
    {
        const std::pair<std::uint32_t, std::uint32_t> key = std::make_pair(std::min(id1, id2), std::max(id1, id2));
        const auto it = m_continuousJudge.find(key);
        return it != m_continuousJudge.end() ? &it->second : nullptr;
    }
} // egret