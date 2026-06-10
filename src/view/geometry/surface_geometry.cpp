/**
 * @file        surface_geometry.cpp
 * @brief       曲面几何体实现文件。
 * @details     将解析曲面（SurfaceBase）离散化为 Qt Quick3D 可渲染的
 *              三角形网格数据（位置 + 法线 + 索引）。
 *
 * @ingroup     View
 */

#include "surface_geometry.h"

namespace egret
{
    SurfaceGeometry::SurfaceGeometry(QQuick3DObject* parent)
        : QQuick3DGeometry(parent)
    {
    }

    void SurfaceGeometry::setSurface(const std::shared_ptr<SurfaceBase>& surface)
    {
        if (!surface) {
            m_surface.reset();
            rebuild();
            return;
        }
        m_surface = surface->clone();
        rebuild();
    }

    void SurfaceGeometry::setSurface(const SurfaceBase* surface)
    {
        if (!surface) {
            m_surface.reset();
            rebuild();
            return;
        }
        m_surface = surface->clone();
        rebuild();
    }

    void SurfaceGeometry::setSubdivisions(int subs)
    {
        if (subs < 2) {
            subs = 2;
        }
        if (m_subdivisions != subs) {
            m_subdivisions = subs;
            rebuild();
            emit subdivisionsChanged();
        }
    }

    void SurfaceGeometry::setPlaneExtent(double extent)
    {
        if (extent < 1.0) {
            extent = 1.0;
        }
        if (m_planeExtent != extent) {
            m_planeExtent = extent;
            rebuild();
            emit planeExtentChanged();
        }
    }

    void SurfaceGeometry::rebuild()
    {
        if (!m_surface) {
            clear();
            return;
        }

        // 调用曲面细分接口获取网格数据
        TessellationConfig config;
        config.subdivisions = m_subdivisions;
        config.planeExtent = m_planeExtent;
        TessellatedMesh mesh = m_surface->tessellate(config);

        if (!mesh.isValid()) {
            clear();
            return;
        }

        // 坐标转换：从物理坐标系 (z-up, 右手系) 到 Quick3D 渲染坐标系 (y-up, 右手系)
        // 映射规则：(x, y, z) → (y, z, x)
        // 与 buildRenderItems() / buildConstraintRenderItems() 中的转换保持一致
        for (auto& vertex : mesh.vertices) {
            const double x = vertex.x();
            const double y = vertex.y();
            const double z = vertex.z();
            vertex = Eigen::Vector3d(y, z, x);
        }
        for (auto& normal : mesh.normals) {
            const double x = normal.x();
            const double y = normal.y();
            const double z = normal.z();
            normal = Eigen::Vector3d(y, z, x);
        }

        // 构建双面网格：tessellate() 输出顺时针绕向，Quick3D 默认使用逆时针为正面
        // - 正面（Forward faces）：逆时针绕向，原始法线
        // - 背面（Back faces）：  顺时针绕向，法线取反
        const std::size_t vertexCount = mesh.vertexCount();
        const std::size_t triangleCount = mesh.indices.size(); // 索引值总数（三角形数）

        constexpr int stride = 6 * static_cast<int>(sizeof(float));
        const std::size_t totalVertexCount = vertexCount * 2; // 正面 + 背面
        const std::size_t totalIndexCount = triangleCount * 2;

        QByteArray vertexData;
        vertexData.resize(static_cast<qsizetype>(totalVertexCount * stride));
        auto* vertexPtr = reinterpret_cast<float*>(vertexData.data());

        // 正面顶点：原始位置 + 原始法线
        for (std::size_t i = 0; i < vertexCount; ++i) {
            const auto& pos = mesh.vertices[i];
            const auto& nrm = mesh.normals[i];
            vertexPtr[i * 6 + 0] = static_cast<float>(pos.x());
            vertexPtr[i * 6 + 1] = static_cast<float>(pos.y());
            vertexPtr[i * 6 + 2] = static_cast<float>(pos.z());
            vertexPtr[i * 6 + 3] = static_cast<float>(nrm.x());
            vertexPtr[i * 6 + 4] = static_cast<float>(nrm.y());
            vertexPtr[i * 6 + 5] = static_cast<float>(nrm.z());
        }

        // 背面顶点：相同位置 + 法线取反
        for (std::size_t i = 0; i < vertexCount; ++i) {
            const auto& pos = mesh.vertices[i];
            const auto& nrm = mesh.normals[i];
            const std::size_t idx = vertexCount + i;
            vertexPtr[idx * 6 + 0] = static_cast<float>(pos.x());
            vertexPtr[idx * 6 + 1] = static_cast<float>(pos.y());
            vertexPtr[idx * 6 + 2] = static_cast<float>(pos.z());
            vertexPtr[idx * 6 + 3] = static_cast<float>(-nrm.x());
            vertexPtr[idx * 6 + 4] = static_cast<float>(-nrm.y());
            vertexPtr[idx * 6 + 5] = static_cast<float>(-nrm.z());
        }

        // 索引数据（uint32_t）
        QByteArray indexData;
        indexData.resize(static_cast<qsizetype>(totalIndexCount * sizeof(std::uint32_t)));
        auto* indexPtr = reinterpret_cast<std::uint32_t*>(indexData.data());

        // 正面索引：逆时针绕向（从原始顺时针交换第 1、2 个顶点）
        for (std::size_t i = 0; i < triangleCount; i ++) {
            // indexPtr[i]     = mesh.indices[i];       // 第 0 顶点不变
            // indexPtr[i + 1] = mesh.indices[i + 2];   // 交换
            // indexPtr[i + 2] = mesh.indices[i + 1];
            mesh.indices[i] = {mesh.indices[i].x(), mesh.indices[i].z(), mesh.indices[i].y()};
        }

        // 背面索引：顺时针绕向（原始绕向），引用背面顶点（偏移 vertexCount）
        const auto offset = static_cast<std::uint32_t>(vertexCount);

        for (std::size_t i = 0; i < triangleCount * 3; i += 3) {
            indexPtr[triangleCount + i] = mesh.indices[i].x() + offset;
            indexPtr[triangleCount + i + 1] = mesh.indices[i].y() + offset;
            indexPtr[triangleCount + i + 2] = mesh.indices[i].z() + offset;
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

        // 声明索引属性，否则索引缓冲被忽略，顶点被当作非索引三角形渲染
        addAttribute(QQuick3DGeometry::Attribute::IndexSemantic,
                     0,
                     QQuick3DGeometry::Attribute::U32Type);

        setStride(stride);
        setVertexData(vertexData);
        setIndexData(indexData);
        update();
    }

} // egret
