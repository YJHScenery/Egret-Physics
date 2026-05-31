/**
 * @file        bezier_curve_geometry.cpp
 * @brief       贝塞尔曲线几何体实现
 * @details     生成二次贝塞尔曲线的 LineStrip 顶点数据（世界空间坐标），
 *              配合 EgretLineMaterial 使用以控制线条颜色和效果。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-15
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 */

#include "bezier_curve_geometry.h"

namespace egret
{
    BezierCurveGeometry::BezierCurveGeometry(QQuick3DObject* parent)
        : QQuick3DGeometry(parent)
    {
        rebuild();
    }

    void BezierCurveGeometry::setPointA(const QVector3D& pt)
    {
        if (m_pointA != pt) {
            m_pointA = pt;
            rebuild();
            emit pointAChanged();
        }
    }

    void BezierCurveGeometry::setControlPoint(const QVector3D& pt)
    {
        if (m_controlPoint != pt) {
            m_controlPoint = pt;
            rebuild();
            emit controlPointChanged();
        }
    }

    void BezierCurveGeometry::setPointB(const QVector3D& pt)
    {
        if (m_pointB != pt) {
            m_pointB = pt;
            rebuild();
            emit pointBChanged();
        }
    }

    void BezierCurveGeometry::setSegments(int segs)
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

    void BezierCurveGeometry::rebuild()
    {
        // 顶点数 = segments + 1（包含两个端点）
        const int totalPoints{m_segments + 1};
        if (totalPoints < 3) {
            return;
        }

        // 顶点格式：仅位置 (3 floats)
        constexpr int stride{3 * static_cast<int>(sizeof(float))};

        QByteArray vertexData;
        vertexData.resize(totalPoints * stride);
        auto* vertexPtr{reinterpret_cast<float*>(vertexData.data())};

        // 二次贝塞尔曲线：P(t) = (1-t)²·A + 2(1-t)t·C + t²·B，t ∈ [0, 1]
        // LineStrip 无索引时按顶点顺序自动连线
        const float invSegments{1.0f / static_cast<float>(m_segments)};

        for (int i = 0; i <= m_segments; ++i) {
            const float t{static_cast<float>(i) * invSegments};
            const float invT{1.0f - t};

            const float coeffA{invT * invT};
            const float coeffC{2.0f * invT * t};
            const float coeffB{t * t};

            const QVector3D pt{m_pointA * coeffA + m_controlPoint * coeffC + m_pointB * coeffB};

            vertexPtr[i * 3 + 0] = pt.x();
            vertexPtr[i * 3 + 1] = pt.y();
            vertexPtr[i * 3 + 2] = pt.z();
        }

        // 配置 QQuick3DGeometry：LineStrip 图元，仅位置属性，无索引
        clear();
        setPrimitiveType(QQuick3DGeometry::PrimitiveType::LineStrip);
        addAttribute(QQuick3DGeometry::Attribute::PositionSemantic,
                     0,
                     QQuick3DGeometry::Attribute::F32Type);
        setStride(stride);
        setVertexData(vertexData);
        // 不设置索引数据——LineStrip 按顶点顺序自动连线
        update();
    }

} // egret
