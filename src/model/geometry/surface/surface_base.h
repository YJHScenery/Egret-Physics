/**
 * @file        surface_base.h
 * @brief       曲面抽象基类头文件，定义所有曲面的公共接口。
 * @details     定义 SurfaceType 枚举和 SurfaceBase 抽象基类，
 *              提供有符号距离、最近点、法线、碰撞检测等通用接口。
 *              曲面是解析几何体，用于支撑面约束器（SupportSurface）的碰撞参考面。
 *              派生类需要实现 signedDistance、closestPoint、normal、collide 等虚函数。
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

#ifndef EGRET_PHYSICS_SURFACE_BASE_H
#define EGRET_PHYSICS_SURFACE_BASE_H

#include <cstdint>
#include <memory>

#include <Eigen/Dense>

#include "physics_utils.h"
#include "transform.h"
#include "shape_base.h"
#include <magic_enum.hpp>

namespace egret
{
    /**
     * @brief       曲面类型枚举。
     * @details     标识不同的曲面几何类型，用于运行时类型识别。
     *              每个派生类应返回唯一的枚举值。
     */
    enum class SurfaceType : std::uint64_t
    {
        Unknown = 0,

        /** 无限大平面。 */
        Plane,

        /** 双三次 Bezier 曲面。 */
        Bezier,

        /** 柱面（以空间曲线为基线的扫略面）。 */
        Cylindrical,

        Abstract = 127,

        Other = 255,
    };
}

namespace magic_enum::customize
{
    template <>
    struct enum_range<egret::SurfaceType>
    {
        static constexpr int min = 0;
        static constexpr int max = 255;
    };
}

namespace egret
{
    /**
     * @brief       细分网格数据结构体。
     * @details     存储曲面离散化后的三角形网格数据，包括顶点位置、法线和三角形索引。
     *              由曲面类的 tessellate() 方法生成，供视图层渲染使用。
     *              顶点和法线为 double 精度（匹配物理引擎），
     *              视图层在填充 QQuick3DGeometry 时转换为 float。
     */
    struct TessellatedMesh
    {
        /** 顶点位置列表（世界空间坐标）。 */
        std::vector<Eigen::Vector3d> vertices;

        /** 顶点法线列表（单位向量，每个顶点一条）。 */
        std::vector<Eigen::Vector3d> normals;

        /** 三角形索引列表，每三个值构成一个三角形。 */
        std::vector<std::uint32_t> indices;

        /**
         * @brief 检查网格是否有效（包含顶点和索引）。
         * @return 如果包含有效数据则返回 true。
         */
        [[nodiscard]] bool isValid() const noexcept
        {
            return !vertices.empty() && !indices.empty() &&
                   indices.size() % 3 == 0;
        }

        /**
         * @brief 返回三角形数量。
         * @return 三角形数目。
         */
        [[nodiscard]] std::size_t triangleCount() const noexcept
        {
            return indices.size() / 3;
        }

        /**
         * @brief 返回顶点数量。
         * @return 顶点数目。
         */
        [[nodiscard]] std::size_t vertexCount() const noexcept
        {
            return vertices.size();
        }

        /** 清空所有数据。 */
        void clear() noexcept
        {
            vertices.clear();
            normals.clear();
            indices.clear();
        }
    };

    /**
     * @brief       细分配置参数结构体。
     * @details     控制曲面离散化时的精度和范围。
     *              不同的曲面类型按需解释这些参数：
     *              - 球体/圆柱：subdivisions 控制经线/纬线分段数
     *              -     平面：subdivisions 控制每边网格分段数，planeExtent 控制平面范围
     *              -   Bezier：subdivisions 控制 u/v 方向采样密度
     */
    struct TessellationConfig
    {
        /**
         * @brief 细分精度参数（默认 16）。
         * @details 值越大生成的三角形越多，网格越平滑。
         *          各子类的具体含义：
         *          - SurfaceSphere：经线数 = subdivisions，纬线数 = subdivisions / 2
         *          - SurfacePlane：每边网格分段数 = subdivisions
         *          - SurfaceCylinder：圆周分段数 = subdivisions，高度分段数 = subdivisions / 4
         *          - SurfaceBezier：u/v 方向采样数 = subdivisions + 1
         */
        int subdivisions{128};

        /**
         * @brief 无限大平面的显示范围（仅对 SurfacePlane 有效）。
         * @details 平面在锚点周围生成边长为 planeExtent 的方形网格。
         *          默认值 200.0 与 Qt Quick3D 内置原语 #Rectangle 的默认尺寸匹配。
         */
        double planeExtent{200.0};
    };

    /**
     * @brief       曲面抽象基类，定义所有曲面的公共接口。
     * @details     SurfaceBase 是所有曲面的抽象基类，提供以下功能：
     *              - 统一的类型枚举识别（getType）
     *              - 有符号距离计算（signedDistance），用于碰撞检测和穿透深度
     *              - 最近点查询（closestPoint），返回曲面上距离给定点最近的位置
     *              - 法线计算（normal），返回曲面上最近点处的单位法线
     *              - 碰撞检测（collide），判断形状是否与曲面相交并生成接触流形
     *              - 表面速度查询（velocityAt），用于移动曲面场景
     *
     *              曲面采用局部坐标系定义，通过内部 Transform 映射到世界空间。
     *              法线方向约定：指向曲面的"外部"（即物体应停留的一侧的反方向）。
     *
     * @invariant   getType() 返回有效的 SurfaceType 枚举值
     * @remark      曲面类不包含质量或运动状态，仅描述几何信息
     * @see         SurfaceSphere, SurfacePlane, SurfaceCylinder, SurfaceBezier, SupportSurface
     */
    class SurfaceBase
    {
    public:
        virtual ~SurfaceBase() = default;

        SurfaceBase(const SurfaceBase&) = default;
        SurfaceBase& operator=(const SurfaceBase&) = default;
        SurfaceBase(SurfaceBase&&) = default;
        SurfaceBase& operator=(SurfaceBase&&) = default;

        /**
         * @brief 返回曲面类型。
         * @return SurfaceType 枚举值。
         */
        [[nodiscard]] virtual SurfaceType getType() const = 0;

        /**
         * @brief 计算世界空间中某点到曲面的有符号距离。
         * @details 正值表示点在曲面的"外部"（法线方向侧），
         *          负值表示点在曲面的"内部"（法线反方向侧），
         *          零表示点在曲面上。
         * @param worldPoint 世界空间中的查询点。
         * @return 有符号距离值。
         */
        [[nodiscard]] virtual double signedDistance(const Eigen::Vector3d& worldPoint) const = 0;

        /**
         * @brief 返回曲面上距离 worldPoint 最近的点。
         * @param worldPoint 世界空间中的查询点。
         * @return 曲面上最近点的世界坐标。
         */
        [[nodiscard]] virtual Eigen::Vector3d closestPoint(const Eigen::Vector3d& worldPoint) const = 0;

        /**
         * @brief 返回曲面上距离 worldPoint 最近的点处的单位法线。
         * @details 法线方向指向曲面的"外部"。
         * @param worldPoint 世界空间中的查询点。
         * @return 单位法线向量。
         */
        [[nodiscard]] virtual Eigen::Vector3d normal(const Eigen::Vector3d& worldPoint) const = 0;

        /**
         * @brief 检测一个形状是否与曲面发生碰撞，并生成接触流形。
         * @details 默认实现使用支撑函数（support）+ 最近点法生成接触点。
         *          派生类可覆盖此方法以实现更高效的解析碰撞检测。
         * @param shape 待检测的形状指针。
         * @param shapeTransform 形状的世界变换。
         * @param manifold [输出] 接触流形，包含所有接触点。
         * @return 如果发生碰撞则返回 true，否则返回 false。
         */
        [[nodiscard]] virtual bool collide(const ShapeBase* shape,
                                           const Transform& shapeTransform,
                                           ContactManifold& manifold) const;

        /**
         * @brief 获取曲面上某点的表面速度。
         * @details 默认返回零向量（静止曲面）。
         *          对于移动曲面（如传送带表面），派生类可覆盖此方法。
         * @param worldPoint 曲面上点的世界坐标。
         * @return 该点的表面速度向量（m/s）。
         */
        [[nodiscard]] virtual Eigen::Vector3d velocityAt(const Eigen::Vector3d& worldPoint) const;

        virtual std::unique_ptr<SurfaceBase> clone() const = 0;

        /**
         * @brief 将曲面离散化为三角形网格，用于渲染。
         * @details 派生类需根据自身几何参数生成三角形网格：
         *          - 顶点位置（世界空间坐标）
         *          - 每个顶点的法线
         *          - 三角形索引列表
         *
         *          法线方向需与 normal() 方法保持一致（指向曲面外部）。
         * @param config 细分配置参数（精度、范围等）。
         * @return 包含顶点、法线和索引的细分网格。
         */
        [[nodiscard]] virtual TessellatedMesh tessellate(
            const TessellationConfig& config = TessellationConfig{}) const = 0;

    protected:
        SurfaceBase() = default;
    };
} // egret

#endif // EGRET_PHYSICS_SURFACE_BASE_H
