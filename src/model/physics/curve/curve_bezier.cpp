/**
 * @file        curve_bezier.cpp
 * @brief       N 阶贝塞尔曲线实现文件。
 * @details     提供 CurveBezier 类的所有方法实现，
 *              使用德卡斯特里奥算法进行数值稳定的求值。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-31
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 */

#include "curve_bezier.h"
#include <stdexcept>
#include <vector>

namespace egret
{
    CurveBezier::CurveBezier(std::vector<Eigen::Vector3d> controlPoints)
        : m_controlPoints(std::move(controlPoints))
    {
        if (m_controlPoints.size() < 2) {
            throw std::invalid_argument(
                "CurveBezier: 至少需要 2 个控制点");
        }
    }

    std::unique_ptr<CurveBase> CurveBezier::clone() const
    {
        return std::make_unique<CurveBezier>(m_controlPoints);
    }

    CurveType CurveBezier::getType() const
    {
        return CurveType::Bezier;
    }

    Eigen::Vector3d CurveBezier::evaluate(double t) const
    {
        const auto [point, _] = deCasteljau(t);
        return point;
    }

    Eigen::Vector3d CurveBezier::derivative(double t) const
    {
        const auto [_, tangent] = deCasteljau(t);
        return tangent;
    }

    std::pair<Eigen::Vector3d, Eigen::Vector3d>
    CurveBezier::deCasteljau(double t) const
    {
        const std::size_t n = m_controlPoints.size();
        const int order = static_cast<int>(n) - 1;

        // 构建德卡斯特里奥金字塔
        // pyramid[k][i] = 第 k 级插值的第 i 个点
        // pyramid[0]    = 原始控制点
        // pyramid[n-1][0] = 曲线点 C(t)
        std::vector<std::vector<Eigen::Vector3d>> pyramid(n);
        pyramid[0] = m_controlPoints;

        for (std::size_t k = 1; k < n; ++k) {
            pyramid[k].resize(n - k);
            for (std::size_t i = 0; i < n - k; ++i) {
                pyramid[k][i] = (1.0 - t) * pyramid[k - 1][i] +
                                 t * pyramid[k - 1][i + 1];
            }
        }

        const Eigen::Vector3d cp = pyramid[n - 1][0];

        // 导数：C'(t) = n * (Pⁿ⁻¹₁ - Pⁿ⁻¹₀)
        // 使用倒数第二级（level n-2）的前两个点
        Eigen::Vector3d tangent = Eigen::Vector3d::Zero();
        if (n >= 2) {
            const Eigen::Vector3d& p0 = pyramid[n - 2][0];
            const Eigen::Vector3d& p1 = pyramid[n - 2][1];
            tangent = static_cast<double>(order) * (p1 - p0);
        }

        return {cp, tangent};
    }
} // egret
