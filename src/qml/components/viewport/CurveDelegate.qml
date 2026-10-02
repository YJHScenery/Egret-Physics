/**
 * @file        CurveDelegate.qml
 * @brief       视口曲线轨道渲染委托
 * @details     对应 SceneCurveModel 的单个曲线条目。几何数据由 C++ 的
 *              CurveGeometry::rebuild() 生成，坐标转换已完成，QML 中直接使用。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 * @ingroup     View
 */

import QtQuick 2.15
import QtQuick3D 6.9

import "qrc:/materials/materials"

Model {
    geometry: curveGeometry

    materials: EgretLineMaterial {
        lineColor: curveColor
    }
}
