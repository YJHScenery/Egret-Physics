/**
 * @file        bezier_curve_geometry.cpp
 * @brief       贝塞尔曲线几何体实现
 * @details     持有 CurveBezier 成员，使用德卡斯特里奥算法生成任意阶贝塞尔曲线的
 *              LineStrip 顶点数据，配合 EgretLineMaterial 渲染线条。
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
        // 初始化为一条 3 点曲线（与旧版默认行为一致）
        std::vector<Eigen::Vector3d> defaults = {
            {0, 0, 0}, {0, 0, 0}, {0, 0, 0}
        };
        m_curve = std::make_unique<CurveBezier>(std::move(defaults));
        rebuild();
    }

    QVariantList BezierCurveGeometry::controlPoints() const
    {
        QVariantList list;
        if (!m_curve) {
            return list;
        }
        const auto& cps = m_curve->getControlPoints();
        list.reserve(cps.size());
        for (const auto& pt : cps) {
            list.append(QVariant::fromValue(
                QVector3D(static_cast<float>(pt.x()),
                          static_cast<float>(pt.y()),
                          static_cast<float>(pt.z()))));
        }
        return list;
    }

    void BezierCurveGeometry::setControlPoints(const QVariantList& pts)
    {
        if (pts.size() < 2) {
            return; // 不足 2 个控制点，不更新
        }

        // 转换为 Eigen::Vector3d 列表
        std::vector<Eigen::Vector3d> cps;
        cps.reserve(pts.size());
        for (const auto& v : pts) {
            if (!v.canConvert<QVector3D>()) {
                return; // 类型不匹配，放弃本次更新
            }
            const QVector3D qpt = v.value<QVector3D>();
            cps.emplace_back(qpt.x(), qpt.y(), qpt.z());
        }

        if (cps.size() < 2) {
            return;
        }

        // 只有在数据真正变化时才重建
        const auto& oldCps = m_curve->getControlPoints();
        if (oldCps.size() == cps.size() &&
            std::equal(oldCps.begin(), oldCps.end(), cps.begin(),
                       [](const Eigen::Vector3d& a, const Eigen::Vector3d& b) {
                           return a.isApprox(b, 1e-12);
                       })) {
            return;
        }

        m_curve = std::make_unique<CurveBezier>(std::move(cps));
        rebuild();
        emit controlPointsChanged();
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
        if (!m_curve) {
            return;
        }

        // 顶点数 = segments + 1（包含两个端点）
        const int totalPoints{m_segments + 1};

        // 顶点格式：仅位置 (3 floats)
        constexpr int stride{3 * static_cast<int>(sizeof(float))};

        QByteArray vertexData;
        vertexData.resize(totalPoints * stride);
        auto* vertexPtr{reinterpret_cast<float*>(vertexData.data())};

        // 沿曲线采样顶点
        // LineStrip 无索引时按顶点顺序自动连线
        const double invSegments = 1.0 / static_cast<double>(m_segments);

        for (int i = 0; i <= m_segments; ++i) {
            const double t = static_cast<double>(i) * invSegments;
            const Eigen::Vector3d pt = m_curve->evaluate(t);

            vertexPtr[i * 3 + 0] = static_cast<float>(pt.x());
            vertexPtr[i * 3 + 1] = static_cast<float>(pt.y());
            vertexPtr[i * 3 + 2] = static_cast<float>(pt.z());
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
