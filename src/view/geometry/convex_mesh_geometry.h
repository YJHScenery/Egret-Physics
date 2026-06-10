/**
 * @file        convex_mesh_geometry.h
 * @brief       凸体网格几何体头文件，将 TessellatedMesh 转换为 Qt Quick3D 可渲染的网格。
 * @details     定义 ConvexMeshGeometry 类，继承 QQuick3DGeometry，
 *              接收一个 TessellatedMesh 引用，将其三角形网格数据
 *              转换为 Qt Quick3D 的顶点/索引缓冲区，供 QML 的 Model 组件渲染。
 *
 *              使用方式（C++）：
 *              @code
 *                  auto* geom = new ConvexMeshGeometry();
 *                  geom->setMesh(tessellatedMesh);
 *                  // 将 geom 暴露给 QML 上下文...
 *              @endcode
 *
 *              使用方式（QML + C++ 上下文属性）：
 *              @code
 *                  Model {
 *                      geometry: convexMeshGeometry
 *                      materials: [ PrincipledMaterial { cullMode: Material.NoCulling } ]
 *                  }
 *              @endcode
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-30
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 */

#ifndef EGRET_PHYSICS_CONVEX_MESH_GEOMETRY_H
#define EGRET_PHYSICS_CONVEX_MESH_GEOMETRY_H

#include <QtQuick3D/qquick3dgeometry.h>

#include "tessellated_mesh.h"

namespace egret
{
    /**
     * @brief       凸体网格几何体类，将 TessellatedMesh 转换为 Qt Quick3D 可渲染的三角形网格。
     * @details     ConvexMeshGeometry 继承自 QQuick3DGeometry，接收一个 TessellatedMesh
     *              引用并通过其顶点/索引数据构建渲染几何体。
     *
     *              顶点格式：
     *              - 位置（PositionSemantic, F32Type, 3 分量）
     *              - 法线（NormalSemantic, F32Type, 3 分量）
     *
     *              索引格式：std::uint32_t 三角形索引。
     *
     * @invariant   m_hasMesh 为 false 时不生成几何体
     * @see         QQuick3DGeometry, TessellatedMesh, ConvexMesh
     */
    class ConvexMeshGeometry : public QQuick3DGeometry
    {
        Q_OBJECT

    public:
        /**
         * @brief 构造凸体网格几何体。
         * @param parent Qt Quick3D 父对象。
         */
        explicit ConvexMeshGeometry(QQuick3DObject* parent = nullptr);

        ~ConvexMeshGeometry() override = default;

        /**
         * @brief 设置要渲染的细分网格数据。
         * @details 内部复制传入的网格数据，不持有传入引用的所有权。
         *          网格变化后需重新调用此方法以更新几何体。
         * @param mesh 细分网格数据的常量引用。
         */
        void setMesh(const TessellatedMesh& mesh);

        /**
         * @brief 清空几何体。
         */
        void clearMesh();

    private:
        /**
         * @brief 重建几何体数据。
         * @details 将 m_mesh 的顶点/法线/索引数据转换为 QQuick3DGeometry 缓冲区格式。
         *          同时完成物理坐标系到渲染坐标系的转换。
         */
        void rebuild();

        /** 细分网格数据的内部副本。 */
        TessellatedMesh m_mesh;

        /** 是否有有效网格数据。 */
        bool m_hasMesh{false};
    };
} // egret

#endif // EGRET_PHYSICS_CONVEX_MESH_GEOMETRY_H
