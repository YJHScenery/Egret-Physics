/**
 * @file        DeepBlueTheme.qml
 * @brief       深蓝色主题定义文件
 * @details     定义应用程序的深蓝色主题配色方案，包括背景色、面板色、强调色等。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-22
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

import QtQuick 2.15

QtObject {
    readonly property color bg0: "#050B1E"
    readonly property color bg1: "#0A1738"
    readonly property color bg2: "#0F2A59"
    readonly property color panel: "#10294CB8"
    readonly property color panelStrong: "#143663E6"
    readonly property color border: "#4F8AC0"
    readonly property color accent: "#27B5FF"
    readonly property color accentSoft: "#2B6DE0"
    readonly property color success: "#34D5C9"
    readonly property color warning: "#FFB84D"
    readonly property color danger: "#FF6D8A"
    readonly property color textPrimary: "#DCEAFF"
    readonly property color textSecondary: "#8DB3D9"
    readonly property color textDim: "#5F84AD"
    readonly property color textUnenabled: "#C0C0C0"

    readonly property int radiusL: 18
    readonly property int radiusM: 12
    readonly property int radiusS: 8
}