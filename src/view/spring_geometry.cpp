/**
 * @file        spring_geometry.cpp
 * @brief       弹簧螺旋线几何体实现
 * @details     生成沿 Z 轴从 0 到 1 的归一化螺旋线（LineStrip 图元），
 *              线圈半径恒为 1，通过 QML 的 scale 属性缩放匹配实际距离。
 *              配合 CustomMaterial 使用以控制线条颜色和效果。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-15
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 */

#include "spring_geometry.h"
#include <QtMath>
#include <numbers>

namespace egret
{
    SpringGeometry::SpringGeometry(QQuick3DObject* parent)
        : QQuick3DGeometry(parent)
    {
        rebuild();
    }

    void SpringGeometry::setCoilCount(int count)
    {
        if (count < 1) {
            count = 1;
        }
        if (m_coilCount != count) {
            m_coilCount = count;
            rebuild();
            emit coilCountChanged();
        }
    }

    void SpringGeometry::setSegmentsPerTurn(int segments)
    {
        if (segments < 2) {
            segments = 2;
        }
        if (m_segmentsPerTurn != segments) {
            m_segmentsPerTurn = segments;
            rebuild();
            emit segmentsPerTurnChanged();
        }
    }

    void SpringGeometry::rebuild()
    {
        const int totalPoints{m_coilCount * m_segmentsPerTurn};
        if (totalPoints < 2) {
            return;
        }

        // 顶点格式：仅位置 (3 floats)
        constexpr int stride{3 * static_cast<int>(sizeof(float))};

        QByteArray vertexData;
        vertexData.resize(totalPoints * stride);
        auto* vertexPtr{reinterpret_cast<float*>(vertexData.data())};

        // 螺旋线参数
        const float invSampleCount{1.0f / static_cast<float>(totalPoints)};
        const float twoPiN{2.0f * static_cast<float>(std::numbers::pi) * static_cast<float>(m_coilCount)};

        // 生成顶点：P(t) = (cos(2π·n·t), sin(2π·n·t), t)，t ∈ [0, 1]
        // LineStrip 无索引时按顶点顺序自动连线
        for (int i = 0; i < totalPoints; ++i) {
            const float t{static_cast<float>(i) * invSampleCount};
            const float angle{twoPiN * t};

            vertexPtr[i * 3 + 0] = std::cos(angle);
            vertexPtr[i * 3 + 1] = std::sin(angle);
            vertexPtr[i * 3 + 2] = t;
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
