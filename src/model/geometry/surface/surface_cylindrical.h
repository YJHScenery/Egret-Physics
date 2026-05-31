/**
 * @file        surface_cylindrical.h
 * @brief       柱面曲面头文件，定义以空间曲线为基线的柱面。
 * @details     定义 SurfaceCylindrical 类，继承自 SurfaceBase，
 *              表示以空间曲线为基线、沿给定方向矢量扫略形成的柱面。
 *              柱面方程为：
 *              S(t, s) = C(t) + s * n
 *              其中 C(t) 为基曲线，n 为方向矢量（大小 = 总宽度），
 *              t ∈ [t₀, t₁]（由曲线决定），s ∈ [0, 1]。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    Surface (曲面几何体)
 */

#ifndef EGRET_PHYSICS_SURFACE_CYLINDRICAL_H
#define EGRET_PHYSICS_SURFACE_CYLINDRICAL_H

#include "surface_base.h"
#include "curve_base.h"

namespace egret
{
    /**
     * @brief       柱面曲面类。
     * @details     SurfaceCylindrical 继承自 SurfaceBase，表示由一条空间曲线
     *              沿着给定方向矢量扫略形成的柱面。曲面方程为：
     *
     *              S(t, s) = C(t) + s * n
     *
     *              其中 C(t) 是基曲线（CurveBase），n 是方向矢量。
     *              n 的方向代表柱面延伸方向，n 的大小代表延伸宽度。
     *              s ∈ [0, 1] 沿 n 方向从起点到终点。
     *              t ∈ [getStartT(), getEndT()] 由基曲线定义。
     *
     *              法线方向：N(t) = C'(t) × n（归一化），与 s 无关。
     *              法线指向垂直于曲线切向和挤压方向的方向。
     *
     *              柱面不自带顶面和底面封盖，仅表示侧面。
     *              曲线可以是贝塞尔曲线、椭圆弧或将来扩展的其他曲线类型。
     *
     * @invariant   基曲线指针不为空
     * @invariant   n 不是零向量
     * @see         SurfaceBase, CurveBase, CurveBezier, CurveEllipticalArc
     */
    class SurfaceCylindrical : public SurfaceBase
    {
    public:
        /**
         * @brief 构造柱面曲面。
         * @param curve      基曲线（柱面的基线），会被深度复制。
         * @param direction  方向矢量。方向 = 柱面延伸方向，
         *                   大小 = 柱面延伸的总宽度。
         * @throws std::invalid_argument 如果 direction 为零向量。
         */
        explicit SurfaceCylindrical(
            std::unique_ptr<CurveBase> curve,
            const Eigen::Vector3d& direction);

        [[nodiscard]] std::unique_ptr<SurfaceBase> clone() const override;

        /** @brief 获取曲面类型，始终返回 SurfaceType::Cylindrical。 */
        [[nodiscard]] SurfaceType getType() const override;

        /**
         * @brief 计算世界点到柱面的有符号距离。
         * @details 通过最近点法计算。正值 = 点位于法线方向侧（外部），
         *          负值 = 位于法线反方向侧（内部）。
         * @param worldPoint 世界空间中的查询点。
         * @return 有符号距离值。
         */
        [[nodiscard]] double signedDistance(
            const Eigen::Vector3d& worldPoint) const override;

        /**
         * @brief 返回柱面上距离 worldPoint 最近的点。
         * @details 数值方法：先对 t 均匀采样找到近似解，
         *          再使用牛顿法迭代细化 s 和 t。
         * @param worldPoint 世界空间中的查询点。
         * @return 曲面上最近点的世界坐标。
         */
        [[nodiscard]] Eigen::Vector3d closestPoint(
            const Eigen::Vector3d& worldPoint) const override;

        /**
         * @brief 返回柱面上最近点处的单位法线。
         * @param worldPoint 世界空间中的查询点。
         * @return 单位法线向量。
         */
        [[nodiscard]] Eigen::Vector3d normal(
            const Eigen::Vector3d& worldPoint) const override;

        /**
         * @brief 检测形状与柱面的碰撞。
         * @details 使用最近点 + 支撑函数法。
         * @param shape 待检测形状。
         * @param shapeTransform 形状的世界变换。
         * @param manifold [输出] 接触流形。
         * @return 是否碰撞。
         */
        [[nodiscard]] bool collide(
            const ShapeBase* shape,
            const Transform& shapeTransform,
            ContactManifold& manifold) const override;

        /** @brief 获取基曲线（只读）。 */
        [[nodiscard]] const CurveBase& getCurve() const { return *m_curve; }

        /** @brief 获取方向矢量。 */
        [[nodiscard]] const Eigen::Vector3d& getDirection() const
        {
            return m_direction;
        }

        /**
         * @brief 将柱面离散化为三角形网格。
         * @details 沿 t 方向采样基曲线，沿 s 方向分段，生成四边形网格：
         *          - t 方向分为 subdivisions 段
         *          - s 方向分为 max(4, subdivisions/4) 段
         *          - 每个四边形拆为两个三角形
         * @param config 细分配置（subdivisions 控制 t 方向采样密度）。
         * @return 三角形网格数据。
         */
        [[nodiscard]] TessellatedMesh tessellate(
            const TessellationConfig& config) const override;

    private:
        /**
         * @brief 计算柱面上参数 (t, s) 对应的点。
         * @param t 曲线参数（由基曲线参数范围决定）。
         * @param s 挤压参数，s ∈ [0, 1]。
         * @return 曲面上的三维坐标。
         */
        [[nodiscard]] Eigen::Vector3d evaluate(double t, double s) const;

        /**
         * @brief 计算参数 t 处的曲面法线（未归一化）。
         * @details N(t) = C'(t) × m_direction（不依赖 s）。
         * @param t 曲线参数。
         * @return 未归一化的法线向量。
         */
        [[nodiscard]] Eigen::Vector3d evaluateRawNormal(double t) const;

        /**
         * @brief 查找曲面上距离 worldPoint 最近的参数对 (t, s)。
         * @details 先采样搜索，再用牛顿法迭代细化。
         * @param worldPoint 世界空间中的查询点。
         * @return (t, s) 参数对。
         */
        [[nodiscard]] std::pair<double, double> findClosestParameters(
            const Eigen::Vector3d& worldPoint) const;

        /** 基曲线的唯一所有权指针。 */
        std::unique_ptr<CurveBase> m_curve;

        /** 方向矢量（方向 = 延伸方向，大小 = 延伸宽度）。 */
        Eigen::Vector3d m_direction;

        /** 方向矢量的长度平方（缓存以加速计算）。 */
        double m_dirLenSq;

        /** 最近一次最近点查找到的 t 缓存。 */
        mutable double m_lastT{0.0};

        /** 最近一次最近点查找到的 s 缓存。 */
        mutable double m_lastS{0.0};
    };
} // egret

#endif // EGRET_PHYSICS_SURFACE_CYLINDRICAL_H
