/**
 * @file        surface_plane.h
 * @brief       无限大平面曲面头文件，定义平面曲面。
 * @details     定义 SurfacePlane 类，继承自 SurfaceBase，
 *              表示无限大平面，由平面上一点和法线定义。
 *              所有计算均为解析解。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-01
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup     Surface (曲面几何体)
 */

#ifndef EGRET_PHYSICS_SURFACE_PLANE_H
#define EGRET_PHYSICS_SURFACE_PLANE_H

#include "surface_base.h"

namespace egret
{
    /**
     * @brief       无限大平面曲面类。
     * @details     SurfacePlane 继承自 SurfaceBase，表示由平面上一点和法线定义的
     *              无限大平面。平面方程为：
     *              dot(P - anchor, normal) = 0
     *              法线方向指向平面的"外部"（物体应停留的一侧）。
     *
     *              所有查询接口均为解析解，O(1) 时间复杂度。
     *
     * @invariant   m_normal 是单位向量
     * @see         SurfaceBase, SurfaceSphere, SurfaceCylinder, SurfaceBezier
     */
    class SurfacePlane : public SurfaceBase
    {
    public:
        /**
         * @brief 构造无限大平面。
         * @param anchor 平面上任意一点。
         * @param normal 平面法线（指向外部），会自动归一化。
         */
        explicit SurfacePlane(const Eigen::Vector3d& anchor,
                              const Eigen::Vector3d& normal);

        std::unique_ptr<SurfaceBase> clone() const override;

        /** @brief 获取曲面类型，始终返回 SurfaceType::Plane。 */
        [[nodiscard]] SurfaceType getType() const override;

        /**
         * @brief 计算世界点到平面的有符号距离。
         * @details signedDistance = dot(point - anchor, normal)
         * @param worldPoint 世界空间中的查询点。
         * @return 有符号距离。
         */
        [[nodiscard]] double signedDistance(const Eigen::Vector3d& worldPoint) const override;

        /**
         * @brief 返回平面上距离 worldPoint 最近的点。
         * @param worldPoint 世界空间中的查询点。
         * @return 平面上最近点的世界坐标。
         */
        [[nodiscard]] Eigen::Vector3d closestPoint(const Eigen::Vector3d& worldPoint) const override;

        /**
         * @brief 返回平面的法线。
         * @details 对于平面，法线恒定，与查询点无关。
         * @param worldPoint 世界空间中的查询点（被忽略）。
         * @return 平面法线（单位向量）。
         */
        [[nodiscard]] Eigen::Vector3d normal(const Eigen::Vector3d& worldPoint) const override;

        /**
         * @brief 检测形状与平面的碰撞。
         * @details 使用形状的支撑函数在 -normal 方向检测穿透。
         * @param shape 待检测形状。
         * @param shapeTransform 形状的世界变换。
         * @param manifold [输出] 接触流形。
         * @return 是否碰撞。
         */
        [[nodiscard]] bool collide(const ShapeBase* shape,
                                   const Transform& shapeTransform,
                                   ContactManifold& manifold) const override;

        /** @brief 获取平面上的锚点。 */
        [[nodiscard]] const Eigen::Vector3d& getAnchor() const { return m_anchor; }

        /** @brief 获取平面法线。 */
        [[nodiscard]] const Eigen::Vector3d& getNormal() const { return m_normal; }

        /**
         * @brief 将无限大平面离散化为有限矩形网格。
         * @details 在锚点周围生成一个边长为 planeExtent 的方形网格：
         *          - 每边分为 subdivisions 段
         *          - 网格在平面内对齐，法线方向保持不变
         *          - 无限大平面无法真正渲染，此方法提供一个可视化的有限表示
         * @param config 细分配置（subdivisions 控制网格密度，planeExtent 控制大小）。
         * @return 三角形网格数据。
         */
        [[nodiscard]] TessellatedMesh tessellate(
            const TessellationConfig& config) const override;

    private:
        /** 平面上任意一点。 */
        Eigen::Vector3d m_anchor;

        /** 平面单位法线（指向外部）。 */
        Eigen::Vector3d m_normal;
    };
} // egret

#endif // EGRET_PHYSICS_SURFACE_PLANE_H
