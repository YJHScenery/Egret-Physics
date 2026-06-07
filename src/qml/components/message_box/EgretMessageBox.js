/**
 * @file        EgretMessageBox.js
 * @brief       EgretMessageDialog 静态函数辅助库
 * @details     提供类似 QMessageBox::information / warning / critical / question 的
 *              静态函数式 API。通过动态创建 EgretMessageDialog 实例简化调用。
 *
 *              典型用法：
 *              @code
 *              import "qrc:/components/components/message_box/EgretMessageBox.js" as MessageBox
 *
 *              // 简单信息提示
 *              MessageBox.information(parentItem, "操作完成", "文件已成功保存。")
 *
 *              // 警告 + 自定义按钮
 *              MessageBox.warning(parentItem, "文件已修改", "是否保存更改？", {
 *                  buttons: EgretMessageDialog.StandardButton.Yes | EgretMessageDialog.StandardButton.No | EgretMessageDialog.StandardButton.Cancel,
 *                  onButtonClicked: function(button, role) {
 *                      if (button === EgretMessageDialog.StandardButton.Yes) saveFile()
 *                  }
 *              })
 *
 *              // 严重错误 + 详细信息
 *              MessageBox.critical(parentItem, "致命错误", "无法加载引擎核心模块。", {
 *                  detailedText: "Error: 0xC0000005\nModule: physics_core.dll\n...",
 *                  buttons: EgretMessageDialog.StandardButton.Abort | EgretMessageDialog.StandardButton.Retry | EgretMessageDialog.StandardButton.Ignore
 *              })
 *
 *              // 询问对话框
 *              MessageBox.question(parentItem, "确认删除", "此操作不可撤销，确定继续吗？", {
 *                  buttons: EgretMessageDialog.StandardButton.Yes | EgretMessageDialog.StandardButton.No,
 *                  onButtonClicked: function(button, role) {
 *                      if (button === EgretMessageDialog.StandardButton.Yes) deleteItem()
 *                  }
 *              })
 *              @endcode
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-13
 * @version     1.0.0
 *
 * @copyright   Copyright © 2026 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     View
 */

.pragma library

// ============================================================================
// 默认选项
// ============================================================================

/**
 * @brief 各等级的默认按钮配置
 */
var _defaultButtons = {
    "information": 0x00000400,                         // StandardButton.Ok
    "warning":     0x00400400,                         // StandardButton.Ok | StandardButton.Cancel
    "critical":    0x00000400,                         // StandardButton.Ok
    "question":    0x00014000                          // StandardButton.Yes | StandardButton.No
}

// EgretMessageDialog.StandardButton 枚举值参考（与 Qt QPlatformDialogHelper 一致）:
// Ok=0x00000400, Cancel=0x00400000, Yes=0x00004000, No=0x00010000

/**
 * @brief 各等级的默认标题
 */
function _defaultTitle(level) {
    switch (level) {
        case "information": return qsTr("Information")
        case "warning":     return qsTr("Warning")
        case "critical":    return qsTr("Critical")
        case "question":    return qsTr("Question")
        default:            return qsTr("Message")
    }
}

// ============================================================================
// 内部辅助
// ============================================================================

/**
 * @brief 获取或创建 EgretMessageDialog 组件缓存
 * @details 首次调用时编译组件并缓存，后续调用复用缓存的 Component 对象
 */
var _componentCache = null

function _getComponent() {
    if (_componentCache !== null && _componentCache.status === Component.Ready) {
        return _componentCache
    }
    _componentCache = Qt.createComponent("qrc:/components/components/message_box/EgretMessageDialog.qml")
    return _componentCache
}

/**
 * @brief 创建并显示弹窗
 * @param parentItem 父 Item（用于生命周期管理）
 * @param level      消息等级字符串
 * @param title      自定义标题（为空则使用默认）
 * @param text       主消息文本
 * @param options    可选配置对象
 * @returns 创建的 EgretMessageDialog 实例
 *
 * options 对象支持以下字段：
 *   - informativeText : string  补充说明文本
 *   - detailedText    : string  详细信息文本
 *   - buttons         : int     按钮标志组合
 *   - iconSource      : url     自定义图标
 *   - onButtonClicked : function(button, role)  按钮点击回调
 *   - onAccepted      : function()              接受回调
 *   - onRejected      : function()              拒绝回调
 */
function _showDialog(parentItem, level, title, text, options) {
    var component = _getComponent()

    if (component.status === Component.Error) {
        console.error("EgretMessageBox: Failed to load EgretMessageDialog.qml - " + component.errorString())
        return null
    }

    if (component.status === Component.Loading) {
        // 等待组件加载完成（同步场景通常不会发生）
        component.statusChanged.connect(function() {
            if (component.status === Component.Ready) {
                _createInstance(parentItem, component, level, title, text, options)
            }
        })
        return null
    }

    return _createInstance(parentItem, component, level, title, text, options)
}

/**
 * @brief 实际创建弹窗实例
 */
function _createInstance(parentItem, component, level, title, text, options) {
    // 构建初始化属性
    var properties = {
        level: _levelToInt(level),
        text: text || ""
    }

    // 标题
    if (title && title !== "") {
        properties.customTitle = title
    }

    // 合并 options
    if (options !== undefined && options !== null) {
        if (options.informativeText !== undefined) properties.informativeText = options.informativeText
        if (options.detailedText !== undefined)    properties.detailedText = options.detailedText
        if (options.buttons !== undefined)         properties.buttons = options.buttons
        if (options.iconSource !== undefined)      properties.iconSource = options.iconSource
    } else {
        // 使用默认按钮
        if (_defaultButtons[level] !== undefined) {
            properties.buttons = _defaultButtons[level]
        }
    }

    // 确定父对象
    // 如果 parentItem 有效则使用，否则尝试挂到根窗口的 contentItem
    var parentObj = null
    if (parentItem !== undefined && parentItem !== null) {
        parentObj = parentItem
    }

    // 创建实例
    var dialog = component.createObject(parentObj, properties)

    if (dialog === null) {
        console.error("EgretMessageBox: Failed to create EgretMessageDialog instance - " + component.errorString())
        return null
    }

    // 连接信号
    if (options !== undefined && options !== null) {
        if (typeof options.onButtonClicked === "function") {
            dialog.buttonClicked.connect(options.onButtonClicked)
        }
        if (typeof options.onAccepted === "function") {
            dialog.accepted.connect(options.onAccepted)
        }
        if (typeof options.onRejected === "function") {
            dialog.rejected.connect(options.onRejected)
        }
    }

    // 自动清理：关闭后销毁
    dialog.closed.connect(function() {
        dialog.destroy()
    })

    // 显示弹窗
    dialog.open()

    return dialog
}

/**
 * @brief 将等级字符串转为 EgretMessageDialog.Level 枚举值
 */
function _levelToInt(level) {
    switch (level) {
        case "information": return 0  // EgretMessageDialog.Level.Information
        case "warning":     return 1  // EgretMessageDialog.Level.Warning
        case "critical":    return 2  // EgretMessageDialog.Level.Critical
        case "question":    return 3  // EgretMessageDialog.Level.Question
        default:            return 0
    }
}

// ============================================================================
// 公开 API
// ============================================================================

/**
 * @brief 显示信息提示弹窗
 * @param parentItem 父 Item（如 this、Overlay.overlay 等），可为 null
 * @param title      弹窗标题
 * @param text       主消息文本
 * @param options    可选配置对象（参见 _showDialog 的 options 说明）
 * @returns EgretMessageDialog 实例
 */
function information(parentItem, title, text, options) {
    return _showDialog(parentItem, "information", title, text, options)
}

/**
 * @brief 显示警告弹窗
 * @param parentItem 父 Item
 * @param title      弹窗标题
 * @param text       主消息文本
 * @param options    可选配置对象
 * @returns EgretMessageDialog 实例
 */
function warning(parentItem, title, text, options) {
    return _showDialog(parentItem, "warning", title, text, options)
}

/**
 * @brief 显示严重错误弹窗
 * @param parentItem 父 Item
 * @param title      弹窗标题
 * @param text       主消息文本
 * @param options    可选配置对象
 * @returns EgretMessageDialog 实例
 */
function critical(parentItem, title, text, options) {
    return _showDialog(parentItem, "critical", title, text, options)
}

/**
 * @brief 显示询问弹窗
 * @param parentItem 父 Item
 * @param title      弹窗标题
 * @param text       主消息文本
 * @param options    可选配置对象
 * @returns EgretMessageDialog 实例
 */
function question(parentItem, title, text, options) {
    return _showDialog(parentItem, "question", title, text, options)
}
