/**
 * @file        EgretSeparator.qml
 * @brief       分隔线组件文件
 * @details     定义简单的水平分隔线组件，用于视觉上分隔UI元素。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-28
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

import QtQuick
import QtQuick.Controls

Rectangle {

    height: 2  // 线条高度
    color: theme.accent         // 淡蓝色
    opacity: 0.8               // 可选：稍微透明增加柔和感
}