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

    void SurfaceGeometry::rebuild()
    {
        if (!m_surface) {
            clear();
            return;
        }

        // 调用曲面细分接口获取网格数据
        const TessellationConfig config{m_subdivisions};
        const TessellatedMesh mesh = m_surface->tessellate(config);

        if (!mesh.isValid()) {
            clear();
            return;
        }

        // 顶点格式：位置 (3 floats) + 法线 (3 floats)，交错排列
        constexpr int stride = 6 * static_cast<int>(sizeof(float));

        const std::size_t vertexCount = mesh.vertexCount();
        const std::size_t indexCount = mesh.indices.size();

        QByteArray vertexData;
        vertexData.resize(static_cast<qsizetype>(vertexCount * stride));
        auto* vertexPtr = reinterpret_cast<float*>(vertexData.data());

        // 将 double 精度的顶点和法线转换为 float 并交错排列
        for (std::size_t i = 0; i < vertexCount; ++i) {
            const auto& pos = mesh.vertices[i];
            const auto& nrm = mesh.normals[i];

            // 位置 (3 floats)
            vertexPtr[i * 6 + 0] = static_cast<float>(pos.x());
            vertexPtr[i * 6 + 1] = static_cast<float>(pos.y());
            vertexPtr[i * 6 + 2] = static_cast<float>(pos.z());

            // 法线 (3 floats)
            vertexPtr[i * 6 + 3] = static_cast<float>(nrm.x());
            vertexPtr[i * 6 + 4] = static_cast<float>(nrm.y());
            vertexPtr[i * 6 + 5] = static_cast<float>(nrm.z());
        }

        // 索引数据（uint32_t）
        QByteArray indexData;
        indexData.resize(static_cast<qsizetype>(indexCount * sizeof(std::uint32_t)));
        std::memcpy(indexData.data(), mesh.indices.data(),
                    indexCount * sizeof(std::uint32_t));

        // 配置 QQuick3DGeometry
        clear();
        setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);

        addAttribute(QQuick3DGeometry::Attribute::PositionSemantic,
                     0,
                     QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::NormalSemantic,
                     3 * static_cast<int>(sizeof(float)),
                     QQuick3DGeometry::Attribute::F32Type);

        setStride(stride);
        setVertexData(vertexData);
        setIndexData(indexData);
        update();
    }

} // egret
