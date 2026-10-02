/**
 * @file        ConvexMeshDelegate.qml
 * @brief       视口凸体网格渲染委托
 * @details     对应 SceneConvexMeshModel 的单个凸体网格条目。网格数据由
 *              ConvexMeshGeometry::rebuild() 生成，坐标转换已完成，QML 中直接使用。
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

Model {
    geometry: convexMeshGeometry

    position: convexMeshPosition
    scale: convexMeshScale
    rotation: convexMeshRotation

    materials: PrincipledMaterial {
        cullMode: Material.NoCulling
        baseColor: convexMeshColor
        roughness: 0.3
        metalness: 0.1
        opacity: 1
        alphaMode: PrincipledMaterial.Opaque
    }
}
