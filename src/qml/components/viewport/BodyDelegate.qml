/**
 * @file        BodyDelegate.qml
 * @brief       视口刚体渲染委托
 * @details     对应 SceneBodyModel 的单个刚体条目：形状无关，具体网格由
 *              C++ 的 ResourceHelper::getSourceByShape(bodyKind) 派发。
 *              模型角色（bodyId / bodyKind / bodyPosition 等）由 Repeater3D
 *              以上下文属性注入，本文件直接引用即可。
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
    id: bodyRoot

    /** 当前被选中的实体 ID（由视口注入，用于高亮）。 */
    property string selectedBodyId: ""

    /** 是否处于选中状态。 */
    readonly property bool isSelected: selectedBodyId === bodyId

    objectName: "body-" + bodyId

    pickable: true
    receivesShadows: true

    source: resourceHelper.getSourceByShape(bodyKind)

    property var renderCenter: bodyPosition
    property var renderScale: bodyScale
    property var renderRotation: bodyRotation

    position: Qt.vector3d(renderCenter[0], renderCenter[1], renderCenter[2])
    scale: Qt.vector3d(renderScale[0], renderScale[1], renderScale[2])
    rotation: Qt.quaternion(renderRotation[0], renderRotation[1], renderRotation[2], renderRotation[3])

    materials: PrincipledMaterial {
        baseColor: bodyRoot.isSelected ? "#5078a0" : bodyColor
        roughness: 0.18
        specularAmount: 0.7
        emissiveFactor: bodyRoot.isSelected ? "#1A6AA6" : "#000000"
    }
}
