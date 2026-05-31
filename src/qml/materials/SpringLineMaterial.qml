/**
 * @file        SpringLineMaterial.qml
 * @brief       弹簧螺旋线条自定义材质
 * @details     使用 Unshaded 模式渲染 LineStrip 图元，避免标准材质对线图元的错误光照计算。
 *              通过 lineColor 属性控制线条颜色。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-15
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 */

import QtQuick3D
import QtQuick

CustomMaterial {
    id: material

    /** 线条颜色，默认亮蓝色。 */
    property color lineColor: "#4a9eff"

    shadingMode: CustomMaterial.Unshaded

    vertexShader: "qrc:/shaders/shaders/spring_line.vert"
    fragmentShader: "qrc:/shaders/shaders/spring_line.frag"
}
