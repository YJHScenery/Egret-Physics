/**
 * @file        surface_geometry.h
 * @brief       曲面几何体头文件，将 SurfaceBase 曲面离散化为 Qt Quick3D 可渲染的网格。
 * @details     定义 SurfaceGeometry 类，继承 QQuick3DGeometry，
 *              接收一个 SurfaceBase 指针，通过 tessellate() 接口将解析曲面
 *              转换为三角形网格顶点/索引数据，供 QML 的 Model 组件渲染。
 *
 *              使用方式（C++）：
 *              @code
 *                  auto sphere = std::make_shared<SurfaceSphere>(50.0);
 *                  auto* geom = new SurfaceGeometry();
 *                  geom->setSurface(sphere);
 *                  // 将 geom 暴露给 QML 上下文...
 *              @endcode
 *
 *              使用方式（QML + C++ 上下文属性）：
 *              @code
 *                  Model {
 *                      source: surfaceGeometry  // SurfaceGeometry 实例
 *                      materials: [ PrincipledMaterial { ... } ]
 *                  }
 *              @endcode
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-01
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 */

#ifndef EGRET_PHYSICS_SURFACE_GEOMETRY_H
#define EGRET_PHYSICS_SURFACE_GEOMETRY_H

#include <memory>

#include <QtQuick3D/qquick3dgeometry.h>

#include "surface_base.h"

namespace egret
{
    /**
     * @brief       曲面几何体类，将解析曲面转换为 Qt Quick3D 可渲染的三角形网格。
     * @details     SurfaceGeometry 继承自 QQuick3DGeometry，接收一个 SurfaceBase
     *              指针并通过其 tessellate() 方法生成三角形网格数据。
     *              支持通过 Q_PROPERTY 在 QML 中调整细分精度。
     *
     *              顶点格式：
     *              - 位置（PositionSemantic, F32Type, 3 分量）
     *              - 法线（NormalSemantic, F32Type, 3 分量）
     *
     *              索引格式：std::uint32_t 三角形索引。
     *
     * @invariant   m_surface 为 nullptr 时不生成几何体
     * @see         QQuick3DGeometry, SurfaceBase, TessellatedMesh
     */
    class SurfaceGeometry : public QQuick3DGeometry
    {
        Q_OBJECT

        Q_PROPERTY(int subdivisions READ subdivisions WRITE setSubdivisions
                   NOTIFY subdivisionsChanged)

        Q_PROPERTY(double planeExtent READ planeExtent WRITE setPlaneExtent
                   NOTIFY planeExtentChanged)

    public:
        /**
         * @brief 构造曲面几何体。
         * @param parent Qt Quick3D 父对象。
         */
        explicit SurfaceGeometry(QQuick3DObject* parent = nullptr);

        ~SurfaceGeometry() override = default;

        /**
         * @brief 设置要渲染的曲面。
         * @details 内部通过 clone() 创建曲面的副本，不持有传入指针的所有权。
         *          曲面变化后需重新调用此方法以更新几何体。
         * @param surface 指向曲面基类的共享指针。
         */
        void setSurface(const std::shared_ptr<SurfaceBase>& surface);

        /**
         * @brief 从原始指针设置曲面（便捷重载）。
         * @details 内部自动 clone，不持有传入指针的所有权。
         * @param surface 指向曲面基类的指针。
         */
        void setSurface(const SurfaceBase* surface);

        /** @brief 获取当前细分精度。 */
        [[nodiscard]] int subdivisions() const { return m_subdivisions; }

        /**
         * @brief 设置细分精度并重建几何体。
         * @param subs 细分精度值，最小值 2。
         */
        void setSubdivisions(int subs);

        /** @brief 获取无限大平面的显示范围。 */
        [[nodiscard]] double planeExtent() const { return m_planeExtent; }

        /**
         * @brief 设置无限大平面的显示范围并重建几何体。
         * @details 仅对 SurfacePlane 有效，其他曲面类型忽略此值。
         * @param extent 平面边长，最小值 1.0。
         */
        void setPlaneExtent(double extent);

    signals:
        /** 细分精度变化信号。 */
        void subdivisionsChanged();

        /** 平面范围变化信号。 */
        void planeExtentChanged();

    private:
        /**
         * @brief 重建几何体数据。
         * @details 调用 m_surface->tessellate() 获取网格数据，
         *          然后转换为 QQuick3DGeometry 所需的缓冲区格式。
         */
        void rebuild();

        /** 曲面对象的内部副本。 */
        std::unique_ptr<SurfaceBase> m_surface;

        /** 细分精度（默认 16）。 */
        int m_subdivisions{16};

        /**
         * 无限大平面的显示范围（边长），仅对 SurfacePlane 有效。
         * 默认值 200.0 与 TessellationConfig::planeExtent 的默认值一致。
         */
        double m_planeExtent{200.0};
    };
} // egret

#endif //EGRET_PHYSICS_SURFACE_GEOMETRY_H
