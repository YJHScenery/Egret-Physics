/**
 * @file        curve_base.h
 * @brief       空间曲线抽象基类头文件，定义所有空间曲线的公共接口。
 * @details     定义 CurveType 枚举和 CurveBase 抽象基类，
 *              提供曲线求值、导数、参数范围等通用接口。
 *              曲线是三维空间中的参数曲线，作为柱面曲面的基础几何元素。
 *              派生类需要实现 evaluate、derivative、getStartT、getEndT 等虚函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-31
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    Curve (空间曲线)
 */

#ifndef EGRET_PHYSICS_CURVE_BASE_H
#define EGRET_PHYSICS_CURVE_BASE_H

#include <cstdint>
#include <memory>

#include <Eigen/Dense>

#include <magic_enum.hpp>

namespace egret
{
    /**
     * @brief       曲线类型枚举。
     * @details     标识不同的空间曲线几何类型，用于运行时类型识别。
     *              每个派生类应返回唯一的枚举值。
     */
    enum class CurveType : std::uint64_t
    {
        Unknown = 0,

        /** N 阶贝塞尔曲线。 */
        Bezier,

        /** 圆锥曲线（椭圆/抛物线/双曲线）。 */
        ConicSection,

        /** 圆柱螺旋线。 */
        Helix,

        Other = 255,
    };
}

namespace magic_enum::customize
{
    template <>
    struct enum_range<egret::CurveType>
    {
        static constexpr int min = 0;
        static constexpr int max = 255;
    };
}

namespace egret
{
    /**
     * @brief       空间曲线抽象基类，定义所有曲线的公共接口。
     * @details     CurveBase 是所有空间曲线的抽象基类，提供以下功能：
     *              - 统一的类型枚举识别（getType）
     *              - 曲线上点的求值（evaluate），返回参数 t 对应的三维坐标
     *              - 一阶导（切向）计算（derivative），用于曲面法线计算
     *              - 参数范围查询（getStartT / getEndT），用于采样和渲染
     *
     *              曲线采用参数形式 C(t)，t 从 getStartT() 到 getEndT()。
     *              标准参数化约定 t ∈ [0, 1]，派生类可定义其他范围。
     *
     * @invariant   getType() 返回有效的 CurveType 枚举值
     * @remark      曲线类不包含位置变换信息，仅描述几何形状
     * @see         CurveBezier, CurveEllipticalArc, SurfaceCylindrical
     */
    class CurveBase
    {
    public:
        virtual ~CurveBase() = default;

        CurveBase(const CurveBase&) = default;
        CurveBase& operator=(const CurveBase&) = default;
        CurveBase(CurveBase&&) = default;
        CurveBase& operator=(CurveBase&&) = default;

        /**
         * @brief 返回曲线类型。
         * @return CurveType 枚举值。
         */
        [[nodiscard]] virtual CurveType getType() const = 0;

        /**
         * @brief 计算曲线上参数 t 处的点。
         * @param t 曲线参数，应在 [getStartT(), getEndT()] 范围内。
         * @return 三维空间中的点坐标。
         */
        [[nodiscard]] virtual Eigen::Vector3d evaluate(double t) const = 0;

        /**
         * @brief 计算曲线上参数 t 处的一阶导向量（切向量）。
         * @details 返回曲线 C(t) 在参数 t 处的切向量 dC/dt。
         *          用于柱面曲面的法线计算（与挤压方向叉积）。
         * @param t 曲线参数，应在 [getStartT(), getEndT()] 范围内。
         * @return 切向量（未归一化）。
         */
        [[nodiscard]] virtual Eigen::Vector3d derivative(double t) const = 0;

        /**
         * @brief 获取参数范围下限。
         * @return 参数 t 的最小有效值。
         */
        [[nodiscard]] virtual double getStartT() const = 0;

        /**
         * @brief 获取参数范围上限。
         * @return 参数 t 的最大有效值。
         */
        [[nodiscard]] virtual double getEndT() const = 0;

        /**
         * @brief 深度复制曲线对象。
         * @return 指向新曲线对象的 unique_ptr。
         */
        [[nodiscard]] virtual std::unique_ptr<CurveBase> clone() const = 0;

    protected:
        CurveBase() = default;
    };
} // egret

#endif // EGRET_PHYSICS_CURVE_BASE_H
