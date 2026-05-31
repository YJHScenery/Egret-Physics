/**
 * @file        curve_elliptical_arc.cpp
 * @brief       椭圆弧曲线实现文件。
 * @details     提供 CurveEllipticalArc 类的所有方法实现，
 *              使用椭圆参数方程的解析求值和求导。
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

#include "curve_elliptical_arc.h"
#include <cmath>

namespace egret
{
    CurveEllipticalArc::CurveEllipticalArc(
        const Eigen::Vector3d& center,
        const Eigen::Vector3d& semiMajorAxis,
        const Eigen::Vector3d& semiMinorAxis,
        const double startAngle,
        const double sweepAngle)
        : m_center(center)
        , m_semiMajorAxis(semiMajorAxis)
        , m_semiMinorAxis(semiMinorAxis)
        , m_startAngle(startAngle)
        , m_sweepAngle(sweepAngle)
    {
    }

    std::unique_ptr<CurveBase> CurveEllipticalArc::clone() const
    {
        return std::make_unique<CurveEllipticalArc>(
            m_center, m_semiMajorAxis, m_semiMinorAxis,
            m_startAngle, m_sweepAngle);
    }

    CurveType CurveEllipticalArc::getType() const
    {
        return CurveType::EllipticalArc;
    }

    Eigen::Vector3d CurveEllipticalArc::evaluate(const double t) const
    {
        // t ∈ [0, 1] → θ ∈ [startAngle, startAngle + sweepAngle]
        const double theta = m_startAngle + t * m_sweepAngle;
        const double cosTheta = std::cos(theta);
        const double sinTheta = std::sin(theta);

        return m_center + cosTheta * m_semiMajorAxis + sinTheta * m_semiMinorAxis;
    }

    Eigen::Vector3d CurveEllipticalArc::derivative(const double t) const
    {
        // θ = startAngle + t * sweepAngle
        // dθ/dt = sweepAngle
        //
        // d/dθ C(θ) = -sin(θ) * u + cos(θ) * v
        // d/dt C(θ(t)) = dC/dθ * dθ/dt = sweepAngle * (-sin(θ) * u + cos(θ) * v)
        const double theta = m_startAngle + t * m_sweepAngle;
        const double cosTheta = std::cos(theta);
        const double sinTheta = std::sin(theta);

        const Eigen::Vector3d dCdTheta =
            -sinTheta * m_semiMajorAxis + cosTheta * m_semiMinorAxis;

        return m_sweepAngle * dCdTheta;
    }
} // egret
