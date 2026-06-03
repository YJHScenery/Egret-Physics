/*
qt3d-transform-gizmo
Copyright (C) 2020  Federico Ferri

Adapted to QtQuick3D.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick
import QtQuick3D

Node {
    id: root

    // ── Public properties ──────────────────────────────────────────────

    property real size: 1
    readonly property real beamRadius: size * 0.035

    property View3D view3d
    property var camera
    property var cameraController

    property var targetNode: null       // the Model/Node the gizmo is attached to

    property real linearSpeed: 0.01
    property real angularSpeed: 2.0

    property bool visible: false        // overrides Node.visible

    property vector3d absolutePosition: Qt.vector3d(0, 0, 0)

    property real hoverHilightFactor: 1.44
    property real hoverZoomFactor: 1.5

    property int mode: TransformGizmo.Mode.Translation
    property bool canTranslate: true
    property bool canRotate: true
    property bool canScale: false

    property int hoverElement: TransformGizmo.UIElement.None
    property int activeElement: TransformGizmo.UIElement.None

    // ── Enums ──────────────────────────────────────────────────────────

    enum Mode {
        Translation,
        Rotation,
        Scale
    }

    enum UIElement {
        None,
        ModeSwitcher,
        BeamX,
        BeamY,
        BeamZ,
        PlaneXY,
        PlaneXZ,
        PlaneYZ
    }

    // ── Track hover state (called by Main.qml MouseArea) ───────────────

    // Internal: maps objectName → UIElement
    function _nameToElement(name) {
        switch (name) {
        case "gizmo_modeSwitcher": return TransformGizmo.UIElement.ModeSwitcher
        case "gizmo_beamX":       return TransformGizmo.UIElement.BeamX
        case "gizmo_beamY":       return TransformGizmo.UIElement.BeamY
        case "gizmo_beamZ":       return TransformGizmo.UIElement.BeamZ
        case "gizmo_planeXY":     return TransformGizmo.UIElement.PlaneXY
        case "gizmo_planeXZ":     return TransformGizmo.UIElement.PlaneXZ
        case "gizmo_planeYZ":     return TransformGizmo.UIElement.PlaneYZ
        default:                  return TransformGizmo.UIElement.None
        }
    }

    function handleHover(pickResult) {
        if (!pickResult || !pickResult.objectHit) {
            hoverElement = TransformGizmo.UIElement.None
            return
        }
        hoverElement = _nameToElement(pickResult.objectHit.objectName)
    }

    function handlePress(pickResult) {
        if (hoverElement === TransformGizmo.UIElement.None) return
        if (hoverElement === TransformGizmo.UIElement.ModeSwitcher) {
            switchMode()
            return
        }
        if (cameraController) cameraController.enabled = false
        activeElement = hoverElement
    }

    function handleDrag(dx, dy) {
        if (activeElement === TransformGizmo.UIElement.None) return
        var d = projectMotion(dx, -dy)

        switch (activeElement) {
        case TransformGizmo.UIElement.BeamX:
        case TransformGizmo.UIElement.BeamY:
        case TransformGizmo.UIElement.BeamZ:
            var x = activeElement === TransformGizmo.UIElement.BeamX
            var y = activeElement === TransformGizmo.UIElement.BeamY
            var z = activeElement === TransformGizmo.UIElement.BeamZ
            switch (mode) {
            case TransformGizmo.Mode.Translation: translate(x ? d.x : 0, y ? d.y : 0, z ? d.z : 0); break
            case TransformGizmo.Mode.Rotation:    rotate(x ? d.x : 0, y ? d.y : 0, z ? d.z : 0); break
            case TransformGizmo.Mode.Scale:       scale(x ? d.x : 0, y ? d.y : 0, z ? d.z : 0); break
            }
            break
        case TransformGizmo.UIElement.PlaneXY: translate(d.x, d.y, 0); break
        case TransformGizmo.UIElement.PlaneXZ: translate(d.x, 0, d.z); break
        case TransformGizmo.UIElement.PlaneYZ: translate(0, d.y, d.z); break
        }
    }

    function handleRelease() {
        if (activeElement === TransformGizmo.UIElement.None) return
        if (cameraController) cameraController.enabled = true
        activeElement = TransformGizmo.UIElement.None
    }

    // ── Attach / Detach ────────────────────────────────────────────────

    function attachTo(target) {
        if (!target) return
        targetNode = target
        visible = true
        syncPosition()
    }

    function detach() {
        targetNode = null
        visible = false
        absolutePosition = Qt.vector3d(0, 0, 0)
    }

    function syncPosition() {
        if (!targetNode) return
        var m = targetNode.sceneTransform
        absolutePosition = Qt.vector3d(m.m14, m.m24, m.m34)
        root.position = absolutePosition
    }

    // Keep in sync when target moves
    Connections {
        target: root.targetNode
        enabled: root.targetNode !== null
        function onSceneTransformChanged() { syncPosition() }
    }

    // ── Matrix helpers ─────────────────────────────────────────────────

    function getAbsoluteMatrix() {
        // Gizmo origin in world space (world-aligned, no rotation)
        var m = Qt.matrix4x4()
        m.m14 = root.position.x
        m.m24 = root.position.y
        m.m34 = root.position.z
        return m
    }

    function computeViewMatrix() {
        return camera.sceneTransform.inverted()
    }

    function computeProjectionMatrix() {
        var fovRad = camera.fieldOfView * Math.PI / 180.0
        var aspect = view3d.width / Math.max(view3d.height, 1)
        var near = camera.clipNear
        var far = camera.clipFar
        var f = 1.0 / Math.tan(fovRad / 2.0)

        var m = Qt.matrix4x4()
        m.m11 = f / aspect
        m.m22 = f
        m.m33 = (far + near) / (near - far)
        m.m34 = 2.0 * far * near / (near - far)
        m.m43 = -1.0
        m.m44 = 0.0
        return m
    }

    // Ported from qtbase/src/gui/math3d/qvector3d.cpp
    function project(v, modelView, projection, viewport) {
        var tmp = Qt.vector4d(v.x, v.y, v.z, 1)
        tmp = projection.times(modelView).times(tmp)
        if (Math.abs(tmp.w) < 0.00001)
            tmp.w = 1
        tmp = tmp.times(1.0 / tmp.w)

        tmp = tmp.times(0.5).plus(Qt.vector4d(0.5, 0.5, 0.5, 0.5))
        tmp.x = tmp.x * viewport.width + viewport.x
        tmp.y = tmp.y * viewport.height + viewport.y
        return Qt.vector3d(tmp.x, tmp.y, tmp.z)
    }

    function projectMotion(dx, dy) {
        var mtx = getAbsoluteMatrix()
        var mv = computeViewMatrix().times(mtx)
        var p = computeProjectionMatrix()
        var v = Qt.rect(0, 0, view3d.width, view3d.height)

        var s0 = project(Qt.vector3d(0, 0, 0), mv, p, v)
        var sx = project(Qt.vector3d(1, 0, 0), mv, p, v).minus(s0)
        var sy = project(Qt.vector3d(0, 1, 0), mv, p, v).minus(s0)
        var sz = project(Qt.vector3d(0, 0, 1), mv, p, v).minus(s0)
        sx.z = 0; sy.z = 0; sz.z = 0
        sx = sx.normalized()
        sy = sy.normalized()
        sz = sz.normalized()

        var d = Qt.vector3d(dx, dy, 0)
        var px = d.dotProduct(sx)
        var py = d.dotProduct(sy)
        var pz = d.dotProduct(sz)
        return Qt.vector3d(px, py, pz)
    }

    // ── Quaternion helpers ─────────────────────────────────────────────

    function angleAxisToQuat(angle, x, y, z) {
        var a = angle * Math.PI / 180.0
        var s = Math.sin(a * 0.5)
        var c = Math.cos(a * 0.5)
        return Qt.quaternion(c, x * s, y * s, z * s)
    }

    function multiplyQuaternion(q1, q2) {
        return Qt.quaternion(
            q1.scalar * q2.scalar - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z,
            q1.scalar * q2.x + q1.x * q2.scalar + q1.y * q2.z - q1.z * q2.y,
            q1.scalar * q2.y + q1.y * q2.scalar + q1.z * q2.x - q1.x * q2.z,
            q1.scalar * q2.z + q1.z * q2.scalar + q1.x * q2.y - q1.y * q2.x
        )
    }

    // ── Mode switching ─────────────────────────────────────────────────

    function switchMode() {
        var modes = []
        if (canTranslate) modes.push(TransformGizmo.Mode.Translation)
        if (canRotate)    modes.push(TransformGizmo.Mode.Rotation)
        if (canScale)     modes.push(TransformGizmo.Mode.Scale)
        if (modes.length === 0) return
        var idx = modes.indexOf(mode)
        mode = modes[(idx + 1) % modes.length]
    }

    // ── Transform operations on target ─────────────────────────────────

    function translate(dx, dy, dz) {
        if (!targetNode) return
        targetNode.position.x += linearSpeed * dx
        targetNode.position.y += linearSpeed * dy
        targetNode.position.z += linearSpeed * dz
    }

    function rotate(dx, dy, dz) {
        if (!targetNode) return
        targetNode.rotation = multiplyQuaternion(
            angleAxisToQuat(angularSpeed * dx, 1, 0, 0), targetNode.rotation)
        targetNode.rotation = multiplyQuaternion(
            angleAxisToQuat(angularSpeed * dy, 0, 1, 0), targetNode.rotation)
        targetNode.rotation = multiplyQuaternion(
            angleAxisToQuat(angularSpeed * dz, 0, 0, 1), targetNode.rotation)
    }

    function scale(dx, dy, dz) {
        if (!targetNode) return
        targetNode.scale.x += linearSpeed * dx
        targetNode.scale.y += linearSpeed * dy
        targetNode.scale.z += linearSpeed * dz
    }

    // ═══════════════════════════════════════════════════════════════════
    // VISUAL HIERARCHY
    // ═══════════════════════════════════════════════════════════════════

    // Our visuals live under this Node, offset to the target position.
    // This Node stays at identity rotation (gizmo is world-aligned).
    Node {
        id: gizmoVisuals
        visible: root.visible

        // ── Mode switcher sphere ───────────────────────────────────

        readonly property real modeSwitcherRadius0: root.beamRadius * 2
        readonly property real modeSwitcherRadius: modeSwitcherHilighted
            ? root.hoverZoomFactor * modeSwitcherRadius0
            : modeSwitcherRadius0
        readonly property bool modeSwitcherHilighted:
            root.activeElement === TransformGizmo.UIElement.ModeSwitcher
            || (root.activeElement === TransformGizmo.UIElement.None
                && root.hoverElement === TransformGizmo.UIElement.ModeSwitcher)
        readonly property color modeSwitcherColor: "#333"

        Model {
            objectName: "gizmo_modeSwitcher"
            source: "#Sphere"
            pickable: true
            scale: Qt.vector3d(
                0.01 * gizmoVisuals.modeSwitcherRadius * 2,
                0.01 * gizmoVisuals.modeSwitcherRadius * 2,
                0.01 * gizmoVisuals.modeSwitcherRadius * 2)
            materials: [
                PrincipledMaterial {
                    baseColor: gizmoVisuals.modeSwitcherHilighted
                        ? Qt.lighter(gizmoVisuals.modeSwitcherColor, root.hoverHilightFactor)
                        : gizmoVisuals.modeSwitcherColor
                    metalness: 0.3
                    roughness: 0.4
                }
            ]
        }

        // ── Beams (X, Y, Z) ────────────────────────────────────────

        readonly property real beamLineLength: root.size * 0.8
        readonly property real beamTipTranslateLength: root.size * 0.2
        readonly property real beamTipRotateLength: root.beamRadius * 2
        readonly property real beamTipScaleSize: root.beamRadius * 3

        readonly property list<QtObject> beamModel: [
            QtObject {
                readonly property vector3d rot: Qt.vector3d(0, 0, -90)
                readonly property vector3d vec: Qt.vector3d(1, 0, 0)
                readonly property color col: "#f33"
                readonly property int elem: TransformGizmo.UIElement.BeamX
                readonly property string objName: "beamX"
            },
            QtObject {
                readonly property vector3d rot: Qt.vector3d(0, 0, 0)
                readonly property vector3d vec: Qt.vector3d(0, 1, 0)
                readonly property color col: "#3f3"
                readonly property int elem: TransformGizmo.UIElement.BeamY
                readonly property string objName: "beamY"
            },
            QtObject {
                readonly property vector3d rot: Qt.vector3d(90, 0, 0)
                readonly property vector3d vec: Qt.vector3d(0, 0, 1)
                readonly property color col: "#33f"
                readonly property int elem: TransformGizmo.UIElement.BeamZ
                readonly property string objName: "beamZ"
            }
        ]

        Repeater3D {
            model: gizmoVisuals.beamModel
            delegate: Node {
                id: beamDel
                required property QtObject modelData
                required property int index

                // Offset from gizmo origin along the beam's axis
                position: modelData.vec.times(gizmoVisuals.modeSwitcherRadius0 * 1.1)
                eulerRotation: modelData.rot

                readonly property bool hilighted:
                    root.activeElement === modelData.elem
                    || (root.activeElement === TransformGizmo.UIElement.None
                        && root.hoverElement === modelData.elem)
                readonly property color beamColor: modelData.col

                // Shared material for all beam parts
                PrincipledMaterial {
                    id: beamMat
                    baseColor: beamDel.hilighted
                        ? Qt.lighter(beamDel.beamColor, root.hoverHilightFactor)
                        : beamDel.beamColor
                    metalness: 0.0
                    roughness: 0.3
                }

                // ── Beam line (cylinder) ──
                Model {
                    objectName: "gizmo_" + modelData.objName
                    source: "#Cylinder"
                    pickable: true
                    position: Qt.vector3d(0, gizmoVisuals.beamLineLength / 2, 0)
                    scale: Qt.vector3d(
                        0.01 * root.beamRadius * 2,
                        0.01 * gizmoVisuals.beamLineLength,
                        0.01 * root.beamRadius * 2)
                    materials: [beamMat]
                }

                // ── Translate tip (cone) ──
                Model {
                    objectName: "gizmo_" + modelData.objName
                    source: "#Cone"
                    visible: root.visible
                        && root.mode === TransformGizmo.Mode.Translation
                    pickable: true
                    // position: Qt.vector3d(0,
                    //     gizmoVisuals.beamLineLength
                    //     + gizmoVisuals.beamTipTranslateLength / 2, 0)
                    position: Qt.vector3d(0,
                        gizmoVisuals.beamLineLength, 0)
                    scale: Qt.vector3d(
                        0.01 * root.beamRadius * 4,
                        0.01 * gizmoVisuals.beamTipTranslateLength,
                        0.01 * root.beamRadius * 4)
                    materials: [beamMat]
                }

                // ── Rotate tip (cylinder) ──
                Model {
                    objectName: "gizmo_" + modelData.objName
                    source: "#Cylinder"
                    visible: root.visible
                        && root.mode === TransformGizmo.Mode.Rotation
                    pickable: true
                    position: Qt.vector3d(0,
                        gizmoVisuals.beamLineLength
                        + gizmoVisuals.beamTipRotateLength / 2, 0)
                    scale: Qt.vector3d(
                        0.01 * root.beamRadius * 4,
                        0.01 * gizmoVisuals.beamTipRotateLength,
                        0.01 * root.beamRadius * 4)
                    materials: [beamMat]
                }

                // ── Scale tip (cube) ──
                Model {
                    objectName: "gizmo_" + modelData.objName
                    source: "#Cube"
                    visible: root.visible
                        && root.mode === TransformGizmo.Mode.Scale
                    pickable: true
                    position: Qt.vector3d(0,
                        gizmoVisuals.beamLineLength
                        + gizmoVisuals.beamTipScaleSize / 2, 0)
                    scale: Qt.vector3d(
                        0.01 * gizmoVisuals.beamTipScaleSize,
                        0.01 * gizmoVisuals.beamTipScaleSize,
                        0.01 * gizmoVisuals.beamTipScaleSize)
                    materials: [beamMat]
                }
            }
        }

        // ── Planes (XY, XZ, YZ) ────────────────────────────────────

        readonly property real planeSquareSize: root.size * 0.3
        readonly property real planeSquareThickness: root.beamRadius * 0.5

        readonly property list<QtObject> planeModel: [
            QtObject {
                readonly property vector3d vec: Qt.vector3d(1, 1, 0)
                readonly property int elem: TransformGizmo.UIElement.PlaneXY
                readonly property string objName: "planeXY"
            },
            QtObject {
                readonly property vector3d vec: Qt.vector3d(1, 0, 1)
                readonly property int elem: TransformGizmo.UIElement.PlaneXZ
                readonly property string objName: "planeXZ"
            },
            QtObject {
                readonly property vector3d vec: Qt.vector3d(0, 1, 1)
                readonly property int elem: TransformGizmo.UIElement.PlaneYZ
                readonly property string objName: "planeYZ"
            }
        ]

        Repeater3D {
            model: gizmoVisuals.planeModel
            delegate: Node {
                id: planeDel
                required property QtObject modelData
                required property int index

                readonly property bool hilighted:
                    root.activeElement === modelData.elem
                    || (root.activeElement === TransformGizmo.UIElement.None
                        && root.hoverElement === modelData.elem)
                readonly property color planeColor: "#dd6"

                position: modelData.vec.times(
                    root.beamRadius + root.size * 0.025
                    + gizmoVisuals.planeSquareSize / 2)

                Model {
                    objectName: "gizmo_" + modelData.objName
                    source: "#Cube"
                    pickable: true
                    scale: Qt.vector3d(
                        modelData.vec.x ? 0.01 * gizmoVisuals.planeSquareSize
                                        : 0.01 * gizmoVisuals.planeSquareThickness,
                        modelData.vec.y ? 0.01 * gizmoVisuals.planeSquareSize
                                        : 0.01 * gizmoVisuals.planeSquareThickness,
                        modelData.vec.z ? 0.01 * gizmoVisuals.planeSquareSize
                                        : 0.01 * gizmoVisuals.planeSquareThickness)
                    materials: [
                        PrincipledMaterial {
                            baseColor: planeDel.hilighted
                                ? Qt.lighter(planeDel.planeColor, root.hoverHilightFactor)
                                : planeDel.planeColor
                            metalness: 0.0
                            roughness: 0.4
                        }
                    ]
                }
            }
        }
    }
}
