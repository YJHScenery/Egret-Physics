/**
 * @file        curve_conic_section.cpp
 * @brief       圆锥曲线实现文件。
 * @details     提供 CurveConicSection 类的所有方法实现，
 *              支持椭圆、抛物线和双曲线三种类型的求值和求导。
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

#include "curve_conic_section.h"
#include <cmath>

namespace egret
{
    CurveConicSection::CurveConicSection(
        const ConicType type,
        const Eigen::Vector3d& center,
        const Eigen::Vector3d& axisU,
        const Eigen::Vector3d& axisV,
        const double startParam,
        const double sweepParam)
        : m_conicType(type)
        , m_center(center)
        , m_axisU(axisU)
        , m_axisV(axisV)
        , m_startParam(startParam)
        , m_sweepParam(sweepParam)
    {
    }

    std::unique_ptr<CurveBase> CurveConicSection::clone() const
    {
        return std::make_unique<CurveConicSection>(
            m_conicType, m_center, m_axisU, m_axisV,
            m_startParam, m_sweepParam);
    }

    CurveType CurveConicSection::getType() const
    {
        return CurveType::ConicSection;
    }

    Eigen::Vector3d CurveConicSection::evaluate(const double t) const
    {
        // t ∈ [0, 1] → actualParam ∈ [startParam, startParam + sweepParam]
        const double p = m_startParam + t * m_sweepParam;

        switch (m_conicType) {
        case ConicType::Ellipse:
        {
            // C(p) = center + cos(p)·u + sin(p)·v
            const double c = std::cos(p);
            const double s = std::sin(p);
            return m_center + c * m_axisU + s * m_axisV;
        }
        case ConicType::Parabola:
        {
            // C(p) = center + p²·u + p·v
            return m_center + (p * p) * m_axisU + p * m_axisV;
        }
        case ConicType::Hyperbola:
        {
            // C(p) = center + cosh(p)·u + sinh(p)·v
            const double c = std::cosh(p);
            const double s = std::sinh(p);
            return m_center + c * m_axisU + s * m_axisV;
        }
        default:
            return m_center;
        }
    }

    Eigen::Vector3d CurveConicSection::derivative(const double t) const
    {
        // p = startParam + t * sweepParam
        // dp/dt = sweepParam
        const double p = m_startParam + t * m_sweepParam;

        switch (m_conicType) {
        case ConicType::Ellipse:
        {
            // dC/dp = -sin(p)·u + cos(p)·v
            // dC/dt = dC/dp * dp/dt
            const double c = std::cos(p);
            const double s = std::sin(p);
            const Eigen::Vector3d dCdp = -s * m_axisU + c * m_axisV;
            return m_sweepParam * dCdp;
        }
        case ConicType::Parabola:
        {
            // dC/dp = 2p·u + v
            // dC/dt = dC/dp * dp/dt
            const Eigen::Vector3d dCdp = (2.0 * p) * m_axisU + m_axisV;
            return m_sweepParam * dCdp;
        }
        case ConicType::Hyperbola:
        {
            // dC/dp = sinh(p)·u + cosh(p)·v
            // dC/dt = dC/dp * dp/dt
            const double c = std::cosh(p);
            const double s = std::sinh(p);
            const Eigen::Vector3d dCdp = s * m_axisU + c * m_axisV;
            return m_sweepParam * dCdp;
        }
        default:
            return Eigen::Vector3d::Zero();
        }
    }
} // egret
