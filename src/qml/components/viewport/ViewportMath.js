/**
 * @file        ViewportMath.js
 * @brief       3D 视口通用数学辅助库
 * @details     从 SimulatorUnit 的 View3D 内部抽取出来的向量 / 四元数计算，
 *              供视口内的各类委托组件复用。
 *
 *              库内只做纯数值运算并返回普通数组：`.pragma library` 脚本无法访问
 *              Qt 全局对象，因此 Qt.quaternion / Qt.vector3d 的构造由调用方完成
 *              （参见 SimulationViewport.qml 与 ConstraintDelegate.qml）。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 * @ingroup     View
 */

.pragma library

/**
 * @brief  计算把默认 +Z 轴旋转到给定方向的四元数。
 * @param  x 目标方向向量 x 分量（无需预先归一化）。
 * @param  y 目标方向向量 y 分量。
 * @param  z 目标方向向量 z 分量。
 * @return 四元数分量数组 [w, x, y, z]；零向量返回单位四元数。
 */
function rotationFromZTo(x, y, z) {
    var len = Math.sqrt(x * x + y * y + z * z);
    if (len < 0.0001) {
        return [1, 0, 0, 0];
    }

    var dx = x / len;
    var dy = y / len;
    var dz = z / len;

    // forward = (0, 0, 1)，dot 即 dz
    var dot = dz;
    if (dot > 0.9999) {
        return [1, 0, 0, 0];
    }
    if (dot < -0.9999) {
        return [0, 0, 1, 0];
    }

    // axis = forward × dir = (-dy, dx, 0)
    var ax = -dy;
    var ay = dx;
    var aLen = Math.sqrt(ax * ax + ay * ay);
    ax /= aLen;
    ay /= aLen;

    var halfAngleRad = Math.acos(dot) * 0.5;
    var sinHalf = Math.sin(halfAngleRad);
    var cosHalf = Math.cos(halfAngleRad);
    return [cosHalf, ax * sinHalf, ay * sinHalf, 0.0];
}

/**
 * @brief  计算二维平面（渲染坐标系的 XY 平面）内与给定方向垂直的单位向量。
 * @param  x 方向向量 x 分量（无需预先归一化）。
 * @param  y 方向向量 y 分量。
 * @param  z 方向向量 z 分量。
 * @return 垂直单位向量分量数组 [x, y, z]；退化时返回 (0, 1, 0) 或 (0, 0, 1)。
 */
function perpendicular2D(x, y, z) {
    var len = Math.sqrt(x * x + y * y + z * z);
    if (len < 0.0001) {
        return [0, 1, 0];
    }

    var px = -y;
    var py = x;
    var pLen = Math.sqrt(px * px + py * py);
    if (pLen < 0.0001) {
        return [0, 0, 1];
    }
    return [px / pLen, py / pLen, 0.0];
}
