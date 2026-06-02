/**
 * @file        curve_geometry.cpp
 * @brief       曲线几何体实现文件。
 * @details     将解析曲线（CurveBase）离散化为 Qt Quick3D 可渲染的
 *              LineStrip 顶点数据（仅位置），供 EgretLineMaterial 渲染线条。
 *
 * @ingroup     View
 */

#include "curve_geometry.h"

namespace egret
{
    CurveGeometry::CurveGeometry(QQuick3DObject* parent)
        : QQuick3DGeometry(parent)
    {
    }

    void CurveGeometry::setCurve(const std::shared_ptr<CurveBase>& curve)
    {
        if (!curve) {
            m_curve.reset();
            rebuild();
            return;
        }
        m_curve = curve->clone();
        rebuild();
    }

    void CurveGeometry::setCurve(const CurveBase* curve)
    {
        if (!curve) {
            m_curve.reset();
            rebuild();
            return;
        }
        m_curve = curve->clone();
        rebuild();
    }

    void CurveGeometry::setSegments(int segs)
    {
        if (segs < 2) {
            segs = 2;
        }
        if (m_segments != segs) {
            m_segments = segs;
            rebuild();
            emit segmentsChanged();
        }
    }

    void CurveGeometry::rebuild()
    {
        if (!m_curve) {
            clear();
            return;
        }

        // 调用曲线采样接口获取顶点序列
        SampleConfig config;
        config.segments = m_segments;
        CurvePoints points = m_curve->sample(config);

        if (!points.isValid()) {
            clear();
            return;
        }

        // 坐标转换：从物理坐标系 (z-up, 右手系) 到 Quick3D 渲染坐标系 (y-up, 右手系)
        // 映射规则：(x, y, z) → (y, z, x)
        // 与 SurfaceGeometry::rebuild() 中的转换保持一致
        for (auto& vertex : points.vertices) {
            const double x = vertex.x();
            const double y = vertex.y();
            const double z = vertex.z();
            vertex = Eigen::Vector3d(y, z, x);
        }

        // 顶点格式：仅位置 (3 floats)
        const std::size_t vertexCount = points.vertexCount();
        constexpr int stride = 3 * static_cast<int>(sizeof(float));

        QByteArray vertexData;
        vertexData.resize(static_cast<qsizetype>(vertexCount * stride));
        auto* vertexPtr = reinterpret_cast<float*>(vertexData.data());

        for (std::size_t i = 0; i < vertexCount; ++i) {
            const auto& pos = points.vertices[i];
            vertexPtr[i * 3 + 0] = static_cast<float>(pos.x());
            vertexPtr[i * 3 + 1] = static_cast<float>(pos.y());
            vertexPtr[i * 3 + 2] = static_cast<float>(pos.z());
        }

        // 配置 QQuick3DGeometry：LineStrip 图元，仅位置属性，无索引
        clear();
        setPrimitiveType(QQuick3DGeometry::PrimitiveType::LineStrip);

        addAttribute(QQuick3DGeometry::Attribute::PositionSemantic,
                     0,
                     QQuick3DGeometry::Attribute::F32Type);

        setStride(stride);
        setVertexData(vertexData);
        update();
    }

} // egret
