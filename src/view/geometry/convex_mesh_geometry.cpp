/**
 * @file        convex_mesh_geometry.cpp
 * @brief       凸体网格几何体实现文件。
 * @details     将 TessellatedMesh 转换为 Qt Quick3D 可渲染的
 *              三角形网格数据（位置 + 法线 + 索引）。
 *
 * @ingroup     View
 */

#include "convex_mesh_geometry.h"

namespace egret
{
    ConvexMeshGeometry::ConvexMeshGeometry(QQuick3DObject* parent)
        : QQuick3DGeometry(parent)
    {
    }

    void ConvexMeshGeometry::setMesh(const TessellatedMesh& mesh)
    {
        m_mesh = mesh;
        m_hasMesh = true;
        rebuild();
    }

    void ConvexMeshGeometry::clearMesh()
    {
        m_mesh.clear();
        m_hasMesh = false;
        clear();
        update();
    }

    void ConvexMeshGeometry::rebuild()
    {
        if (!m_hasMesh || !m_mesh.isValid()) {
            clear();
            return;
        }

        // 坐标转换：从物理坐标系 (z-up, 右手系) 到 Quick3D 渲染坐标系 (y-up, 右手系)
        // 映射规则：(x, y, z) → (y, z, x)
        // 与 buildRenderItems() / SurfaceGeometry 中的转换保持一致
        for (auto& vertex : m_mesh.vertices) {
            const double x = vertex.x();
            const double y = vertex.y();
            const double z = vertex.z();
            vertex = Eigen::Vector3d(y, z, x);
        }
        for (auto& normal : m_mesh.normals) {
            const double x = normal.x();
            const double y = normal.y();
            const double z = normal.z();
            normal = Eigen::Vector3d(y, z, x);
        }

        const std::size_t vertexCount = m_mesh.vertexCount();

        // 顶点数据（位置 + 法线，每顶点 6 个 float）
        constexpr int stride = 6 * static_cast<int>(sizeof(float));
        QByteArray vertexData;
        vertexData.resize(static_cast<qsizetype>(vertexCount * stride));
        auto* vertexPtr = reinterpret_cast<float*>(vertexData.data());

        for (std::size_t i = 0; i < vertexCount; ++i) {
            const auto& pos = m_mesh.vertices[i];
            const auto& nrm = m_mesh.normals[i];
            vertexPtr[i * 6 + 0] = static_cast<float>(pos.x());
            vertexPtr[i * 6 + 1] = static_cast<float>(pos.y());
            vertexPtr[i * 6 + 2] = static_cast<float>(pos.z());
            vertexPtr[i * 6 + 3] = static_cast<float>(nrm.x());
            vertexPtr[i * 6 + 4] = static_cast<float>(nrm.y());
            vertexPtr[i * 6 + 5] = static_cast<float>(nrm.z());
        }

        // 索引数据（uint32_t）
        const std::size_t indexCount = m_mesh.indices.size() * 3; // 每个三角形 3 个索引
        QByteArray indexData;
        indexData.resize(static_cast<qsizetype>(indexCount * sizeof(std::uint32_t)));
        auto* indexPtr = reinterpret_cast<std::uint32_t*>(indexData.data());

        std::size_t idx = 0;
        for (const auto& tri : m_mesh.indices) {
            indexPtr[idx + 0] = static_cast<std::uint32_t>(tri.x());
            indexPtr[idx + 1] = static_cast<std::uint32_t>(tri.y());
            indexPtr[idx + 2] = static_cast<std::uint32_t>(tri.z());
            idx += 3;
        }

        // 配置 QQuick3DGeometry
        clear();
        setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);

        addAttribute(QQuick3DGeometry::Attribute::PositionSemantic,
                     0,
                     QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::NormalSemantic,
                     3 * static_cast<int>(sizeof(float)),
                     QQuick3DGeometry::Attribute::F32Type);

        // 声明索引属性
        addAttribute(QQuick3DGeometry::Attribute::IndexSemantic,
                     0,
                     QQuick3DGeometry::Attribute::U32Type);

        setStride(stride);
        setVertexData(vertexData);
        setIndexData(indexData);
        update();
    }
} // egret
