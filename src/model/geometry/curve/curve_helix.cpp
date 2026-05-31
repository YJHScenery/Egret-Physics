/**
 * @file        curve_helix.cpp
 * @brief       圆柱螺旋线实现文件。
 * @details     提供 CurveHelix 类的所有方法实现。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 */

#include "curve_helix.h"
#include <cmath>
#include <stdexcept>

namespace egret
{
    CurveHelix::CurveHelix(
        const double radius,
        const double axialSpeed,
        const double endT)
        : m_radius(radius)
        , m_axialSpeed(axialSpeed)
        , m_endT(endT)
    {
        if (m_radius <= 0.0) {
            throw std::invalid_argument(
                "CurveHelix: 旋转半径必须大于 0");
        }
    }

    std::unique_ptr<CurveBase> CurveHelix::clone() const
    {
        return std::make_unique<CurveHelix>(m_radius, m_axialSpeed, m_endT);
    }

    CurveType CurveHelix::getType() const
    {
        return CurveType::Helix;
    }

    Eigen::Vector3d CurveHelix::evaluate(const double t) const
    {
        // C(t) = (R·cos(t), R·sin(t), c·t)
        return {
            m_radius * std::cos(t),
            m_radius * std::sin(t),
            m_axialSpeed * t
        };
    }

    Eigen::Vector3d CurveHelix::derivative(const double t) const
    {
        // C'(t) = (-R·sin(t), R·cos(t), c)
        return {
            -m_radius * std::sin(t),
             m_radius * std::cos(t),
             m_axialSpeed
        };
    }
} // egret
