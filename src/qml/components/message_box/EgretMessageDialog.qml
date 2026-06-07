/**
 * @file        EgretMessageDialog.qml
 * @brief       通用消息弹窗组件
 * @details     提供类似 QMessageBox 静态函数的消息弹窗，基于 Qt Quick Controls Dialog 实现。
 *              支持 Information、Warning、Critical、Question 四种等级，每种等级有独立的默认图标、
 *              标题和颜色标识。支持标准按钮标志组合、自定义按钮、可展开详细信息。
 *              兼容 Qt Quick Dialogs MessageDialog 的 API 设计。
 *
 *              典型用法：
 *              @code
 *              // 直接声明式使用
 *              EgretMessageDialog {
 *                  id: msgDialog
 *                  level: EgretMessageDialog.Level.Warning
 *                  text: "文件已被修改"
 *                  informativeText: "是否保存更改？"
 *                  buttons: EgretMessageDialog.StandardButton.Yes | EgretMessageDialog.StandardButton.No | EgretMessageDialog.StandardButton.Cancel
 *                  onButtonClicked: function(button, role) {
 *                      if (button === EgretMessageDialog.StandardButton.Yes) saveFile()
 *                  }
 *              }
 *              // msgDialog.open()
 *
 *              // 通过 EgretMessageBox 静态函数使用（推荐）
 *              EgretMessageBox.warning(this, "文件已被修改", "是否保存更改？", {
 *                  buttons: EgretMessageDialog.StandardButton.Yes | EgretMessageDialog.StandardButton.No,
 *                  onButtonClicked: function(button, role) { ... }
 *              })
 *              @endcode
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-07
 * @version     1.0.0
 *
 * @copyright   Copyright © 2026 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     View
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtMultimedia

import "qrc:/components/components/theme"

Dialog {
    id: root

    // ============================================================================
    // 枚举定义
    // ============================================================================

    /**
     * @enum Level
     * @brief 消息等级枚举
     */
    enum Level {
        Information,  ///< 信息提示
        Warning,      ///< 警告
        Critical,     ///< 严重错误
        Question      ///< 询问
    }

    /**
     * @enum ButtonRole
     * @brief 按钮角色枚举，与 QPlatformDialogHelper::ButtonRole 兼容
     */
    enum ButtonRole {
        InvalidRole,       ///< 无效角色
        AcceptRole,        ///< 接受（OK / Open / Save / Retry / Ignore）
        RejectRole,        ///< 拒绝（Cancel / Close / Abort）
        DestructiveRole,   ///< 破坏性操作（Discard）
        HelpRole,          ///< 帮助（Help）
        YesRole,           ///< 是（Yes / YesToAll）
        NoRole,            ///< 否（No / NoToAll）
        ApplyRole,         ///< 应用（Apply）
        ResetRole          ///< 重置（Reset / RestoreDefaults）
    }

    // ============================================================================
    // 标准按钮标志枚举（与 QPlatformDialogHelper::StandardButton 兼容）
    // 使用独立的 2 的幂值以避免冲突
    // ============================================================================

    /**
     * @enum StandardButton
     * @brief 标准按钮标志枚举（值与 QPlatformDialogHelper::StandardButton 完全一致）
     * @details 通过 EgretMessageDialog.StandardButton.Ok 形式访问。
     *          支持按位 OR 组合多个按钮。
     *
     *          Qt 原生存在两组值冲突（Reset==NoToAll, RestoreDefaults==Retry），
     *          枚举中以 NoToAll / Retry 为主名，Reset / RestoreDefaults 通过
     *          下方别名属性 standardButtonReset / standardButtonRestoreDefaults 访问。
     */
    enum StandardButton {
        NoButton       = 0x00000000,  ///< 无按钮
        Ok             = 0x00000400,  ///< 确定
        Save           = 0x00000800,  ///< 保存
        SaveAll        = 0x00001000,  ///< 全部保存
        Open           = 0x00002000,  ///< 打开
        Yes            = 0x00004000,  ///< 是
        YesToAll       = 0x00008000,  ///< 全部是
        No             = 0x00010000,  ///< 否
        NoToAll        = 0x00020000,  ///< 全部否（同值：Reset）
        Abort          = 0x00040000,  ///< 中止
        Retry          = 0x00080000,  ///< 重试（同值：RestoreDefaults）
        Ignore         = 0x00100000,  ///< 忽略
        Close          = 0x00200000,  ///< 关闭
        Cancel         = 0x00400000,  ///< 取消
        Discard        = 0x00800000,  ///< 放弃
        Help           = 0x01000000,  ///< 帮助
        Apply          = 0x02000000   ///< 应用
    }

    /**
     * @brief Reset 按钮别名（值与 NoToAll 相同，均为 0x00020000）
     */
    readonly property int standardButtonReset: 0x00020000

    /**
     * @brief RestoreDefaults 按钮别名（值与 Retry 相同，均为 0x00080000）
     */
    readonly property int standardButtonRestoreDefaults: 0x00080000

    // ============================================================================
    // 公有属性
    // ============================================================================

    /**
     * @brief 消息等级
     * @details 决定图标样式、默认标题和默认颜色强调
     */
    property int level: EgretMessageDialog.Level.Information

    /**
     * @brief 主要消息文本
     */
    property string text: ""

    /**
     * @brief 补充说明文本
     * @details 显示在主文本下方，通常用于进一步解释或提问
     */
    property string informativeText: ""

    /**
     * @brief 详细信息文本
     * @details 默认折叠，用户点击"Show Details..."后展开显示
     */
    property string detailedText: ""

    /**
     * @brief 标准按钮标志组合
     * @details 使用按位 OR 组合多个按钮，例如 \c EgretMessageDialog.StandardButton.Ok | EgretMessageDialog.StandardButton.Cancel
     */
    property int buttons: EgretMessageDialog.StandardButton.Ok

    /**
     * @brief 自定义对话框图标
     * @details 为空时使用等级默认图标
     */
    property url iconSource: ""

    /**
     * @brief 自定义标题
     * @details 为空时根据 level 自动生成默认标题
     */
    property string customTitle: ""

    /**
     * @brief 自定义提示音
     * @details 为空时则不执行
     */
    property string soundType: ""

    // ============================================================================
    // 对话框基础配置
    // ============================================================================

    modal: true
    width: 280
    height: Math.min(calculatedHeight, 700)
    anchors.centerIn: parent

    // 关闭按钮（窗口 X 按钮）行为
    // 当没有 Cancel/Close 按钮时，X 按钮仍可关闭对话框
    closePolicy: Popup.CloseOnEscape

    // 禁用 Dialog 内置 standardButtons，完全由自定义按钮接管
    standardButtons: Dialog.NoButton

    // ============================================================================
    // 内部状态
    // ============================================================================

    /// 详情展开状态
    property bool detailsExpanded: false

    /// 关闭动画进行中标志（防止重复触发）
    property bool closeAnimationActive: false

    /// 计算得出的总高度
    property int calculatedHeight: {
        var h = 140  // 基础：标题栏 + 按钮栏 + 内边距
        if (text !== "") h += Math.ceil(text.length / 50) * 24 + 20
        if (informativeText !== "") h += Math.ceil(informativeText.length / 55) * 18 + 12
        if (detailedText !== "" && detailsExpanded) h += 120
        return h
    }

    // ============================================================================
    // 打开 / 关闭动画（与 AboutDialog 保持一致的风格）
    // ============================================================================

    opacity: 0
    scale: 0.8

    onOpened: {
        openOpacityAnimator.start()
        openScaleAnimator.start()
        mediaHelper.playSound(soundType)
    }

    NumberAnimation {
        id: openOpacityAnimator
        target: root
        property: "opacity"
        from: 0
        to: 1
        duration: 300
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: openScaleAnimator
        target: root
        property: "scale"
        from: 0.8
        to: 1
        duration: 300
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: closeOpacityAnimator
        target: root
        property: "opacity"
        from: 1
        to: 0
        duration: 200
        easing.type: Easing.InCubic
        onRunningChanged: {
            if (!running && closeAnimationActive) {
                closeAnimationActive = false
                root.close()
            }
        }
    }

    NumberAnimation {
        id: closeScaleAnimator
        target: root
        property: "scale"
        from: 1
        to: 0.8
        duration: 200
        easing.type: Easing.InCubic
    }

    /**
     * @brief 带动画的关闭
     */
    function closeWithAnimation() {
        if (closeAnimationActive) return
        closeAnimationActive = true
        closeOpacityAnimator.start()
        closeScaleAnimator.start()
    }

    // ============================================================================
    // 信号
    // ============================================================================

    /**
     * @brief 按钮被点击时发出
     * @param button 被点击的按钮标志
     * @param role   按钮的角色
     * @details 兼容 Qt Quick Dialogs MessageDialog::buttonClicked 信号签名
     */
    signal buttonClicked(int button, int role)

    // ============================================================================
    // 内部辅助函数
    // ============================================================================

    /**
     * @brief 获取按钮对应的显示文本
     */
    function getButtonText(flag) {
        switch (flag) {
            case EgretMessageDialog.StandardButton.Ok:              return qsTr("OK")
            case EgretMessageDialog.StandardButton.Open:            return qsTr("Open")
            case EgretMessageDialog.StandardButton.Save:            return qsTr("Save")
            case EgretMessageDialog.StandardButton.Cancel:          return qsTr("Cancel")
            case EgretMessageDialog.StandardButton.Close:           return qsTr("Close")
            case EgretMessageDialog.StandardButton.Discard:         return qsTr("Discard")
            case EgretMessageDialog.StandardButton.Apply:           return qsTr("Apply")
            case EgretMessageDialog.StandardButton.Help:            return qsTr("Help")
            case EgretMessageDialog.StandardButton.SaveAll:         return qsTr("Save All")
            case EgretMessageDialog.StandardButton.Yes:             return qsTr("Yes")
            case EgretMessageDialog.StandardButton.YesToAll:        return qsTr("Yes to All")
            case EgretMessageDialog.StandardButton.No:              return qsTr("No")
            // NoToAll / Reset 同值 0x00020000（枚举中为 NoToAll，文本也以此为准）
            case EgretMessageDialog.StandardButton.NoToAll:         return qsTr("No to All")
            case EgretMessageDialog.StandardButton.Abort:           return qsTr("Abort")
            // Retry / RestoreDefaults 同值 0x00080000（枚举中为 Retry，文本也以此为准）
            case EgretMessageDialog.StandardButton.Retry:           return qsTr("Retry")
            case EgretMessageDialog.StandardButton.Ignore:          return qsTr("Ignore")
            default:                   return qsTr("Unknown")
        }
    }

    /**
     * @brief 获取按钮对应的角色
     */
    function getButtonRole(flag) {
        switch (flag) {
            case EgretMessageDialog.StandardButton.Ok:
            case EgretMessageDialog.StandardButton.Open:
            case EgretMessageDialog.StandardButton.Save:
            case EgretMessageDialog.StandardButton.SaveAll:
            case EgretMessageDialog.StandardButton.Retry:       // 含 RestoreDefaults（同值）
            case EgretMessageDialog.StandardButton.Ignore:
                return EgretMessageDialog.ButtonRole.AcceptRole
            case EgretMessageDialog.StandardButton.Cancel:
            case EgretMessageDialog.StandardButton.Close:
            case EgretMessageDialog.StandardButton.Abort:
                return EgretMessageDialog.ButtonRole.RejectRole
            case EgretMessageDialog.StandardButton.Discard:
                return EgretMessageDialog.ButtonRole.DestructiveRole
            case EgretMessageDialog.StandardButton.Help:
                return EgretMessageDialog.ButtonRole.HelpRole
            case EgretMessageDialog.StandardButton.Yes:
            case EgretMessageDialog.StandardButton.YesToAll:
                return EgretMessageDialog.ButtonRole.YesRole
            case EgretMessageDialog.StandardButton.No:
            case EgretMessageDialog.StandardButton.NoToAll:     // 含 Reset（同值）
                return EgretMessageDialog.ButtonRole.NoRole
            case EgretMessageDialog.StandardButton.Apply:
                return EgretMessageDialog.ButtonRole.ApplyRole
            default:
                return EgretMessageDialog.ButtonRole.InvalidRole
        }
    }

    /**
     * @brief 按钮排序权重（数字越小越靠左）
     * @details Windows 风格：Accept > Yes > No > Reject > Destructive > Apply > Reset > Help
     */
    function getButtonOrder(flag) {
        switch (flag) {
            case EgretMessageDialog.StandardButton.Ok:
            case EgretMessageDialog.StandardButton.Open:
            case EgretMessageDialog.StandardButton.Save:
            case EgretMessageDialog.StandardButton.SaveAll:
                return 0
            case EgretMessageDialog.StandardButton.Retry:       // 含 RestoreDefaults（同值）
            case EgretMessageDialog.StandardButton.Ignore:
                return 1
            case EgretMessageDialog.StandardButton.Yes:
            case EgretMessageDialog.StandardButton.YesToAll:
                return 2
            case EgretMessageDialog.StandardButton.No:
            case EgretMessageDialog.StandardButton.NoToAll:     // 含 Reset（同值）
                return 3
            case EgretMessageDialog.StandardButton.Cancel:
            case EgretMessageDialog.StandardButton.Close:
                return 4
            case EgretMessageDialog.StandardButton.Abort:
                return 5
            case EgretMessageDialog.StandardButton.Discard:
                return 6
            case EgretMessageDialog.StandardButton.Apply:
                return 7
            case EgretMessageDialog.StandardButton.Help:
                return 8
            default:
                return 9
        }
    }

    /**
     * @brief 将 buttons 标志解析为排序后的按钮列表
     * @returns JS 数组，元素为 { flag, text, role, order }
     */
    function getButtonList() {
        // 注意：Reset 与 NoToAll 同值 (0x00020000)，RestoreDefaults 与 Retry 同值 (0x00080000)
        // 枚举中已合并，此处仅列出唯一值
        var allFlags = [
            EgretMessageDialog.StandardButton.Ok, EgretMessageDialog.StandardButton.Open, EgretMessageDialog.StandardButton.Save, EgretMessageDialog.StandardButton.SaveAll,
            EgretMessageDialog.StandardButton.Cancel, EgretMessageDialog.StandardButton.Close, EgretMessageDialog.StandardButton.Discard,
            EgretMessageDialog.StandardButton.Apply, EgretMessageDialog.StandardButton.NoToAll,
            EgretMessageDialog.StandardButton.Help, EgretMessageDialog.StandardButton.Yes, EgretMessageDialog.StandardButton.YesToAll,
            EgretMessageDialog.StandardButton.No, EgretMessageDialog.StandardButton.Abort,
            EgretMessageDialog.StandardButton.Retry, EgretMessageDialog.StandardButton.Ignore
        ]
        var list = []
        for (var i = 0; i < allFlags.length; i++) {
            if (root.buttons & allFlags[i]) {
                list.push({
                    flag: allFlags[i],
                    text: getButtonText(allFlags[i]),
                    role: getButtonRole(allFlags[i]),
                    order: getButtonOrder(allFlags[i])
                })
            }
        }
        // 按 order 排序
        list.sort(function(a, b) { return a.order - b.order })
        return list
    }

    /**
     * @brief 根据 level 获取默认标题
     */
    function getDefaultTitle() {
        if (customTitle !== "") return customTitle
        if (root.title !== "" && root.title !== undefined) return root.title
        switch (level) {
            case EgretMessageDialog.Level.Information: return qsTr("Information")
            case EgretMessageDialog.Level.Warning:     return qsTr("Warning")
            case EgretMessageDialog.Level.Critical:    return qsTr("Critical")
            case EgretMessageDialog.Level.Question:    return qsTr("Question")
            default:                                   return qsTr("Message")
        }
    }

    /**
     * @brief 根据 level 获取图标背景色
     */
    function getLevelColor() {
        switch (level) {
            case EgretMessageDialog.Level.Information: return theme.accent      // #27B5FF
            case EgretMessageDialog.Level.Warning:     return theme.warning     // #FFB84D
            case EgretMessageDialog.Level.Critical:    return theme.danger      // #FF6D8A
            case EgretMessageDialog.Level.Question:    return theme.accent      // #27B5FF
            default:                                   return theme.accent
        }
    }

    /**
     * @brief 根据 level 获取图标符号
     */
    function getLevelSymbol() {
        switch (level) {
            case EgretMessageDialog.Level.Information: return "i"
            case EgretMessageDialog.Level.Warning:     return "!"
            case EgretMessageDialog.Level.Critical:    return "\u2715"  // ✕
            case EgretMessageDialog.Level.Question:    return "?"
            default:                                   return "i"
        }
    }

    /**
     * @brief 处理按钮点击
     */
    function handleButtonClick(flag, role) {
        root.buttonClicked(flag, role)
        closeWithAnimation()
    }

    // ============================================================================
    // 背景
    // ============================================================================

    background: Rectangle {
        color: theme.bg1
        border.width: 1
        border.color: theme.border
        radius: theme.radiusM
    }

    // ============================================================================
    // 标题栏
    // ============================================================================

    header: Rectangle {
        color: "transparent"
        height: 52

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 12
            spacing: 10

            // 标题左侧小型图标
            Rectangle {
                width: 24
                height: 24
                radius: 12
                color: getLevelColor()
                visible: iconSource == ""

                Text {
                    anchors.centerIn: parent
                    text: getLevelSymbol()
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    font.bold: true
                }
            }

            // 自定义图标
            Image {
                source: iconSource
                width: 24
                height: 24
                visible: iconSource != ""
                fillMode: Image.PreserveAspectFit
            }

            Text {
                text: getDefaultTitle()
                color: theme.textPrimary
                font.pixelSize: 16
                font.bold: true
                Layout.fillWidth: true
            }

            // 关闭按钮
            Rectangle {
                width: 28
                height: 28
                radius: 14
                color: closeBtnMouse.containsMouse ? theme.bg2 : "transparent"
                visible: closePolicy !== Popup.NoAutoClose

                Text {
                    anchors.centerIn: parent
                    text: "\u2715"
                    color: theme.textSecondary
                    font.pixelSize: 14
                }

                MouseArea {
                    id: closeBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: closeWithAnimation()
                }
            }
        }

        // 标题栏底部分隔线
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: theme.border
            opacity: 0.4
        }
    }

    // ============================================================================
    // 内容区域
    // ============================================================================

    contentItem: Rectangle {
        color: "transparent"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 0

            // 主内容行：图标 + 文本
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                // 等级图标
                Rectangle {
                    width: 40
                    height: 40
                    radius: 20
                    color: getLevelColor()
                    visible: iconSource == ""
                    Layout.alignment: Qt.AlignTop

                    Text {
                        anchors.centerIn: parent
                        text: getLevelSymbol()
                        color: "#FFFFFF"
                        font.pixelSize: 20
                        font.bold: true
                    }
                }

                // 自定义图标
                Image {
                    source: iconSource
                    width: 40
                    height: 40
                    visible: iconSource != ""
                    fillMode: Image.PreserveAspectFit
                    Layout.alignment: Qt.AlignTop
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    // 主文本
                    Text {
                        text: root.text
                        color: theme.textPrimary
                        font.pixelSize: 14
                        font.bold: true
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        visible: root.text !== ""
                    }

                    // 补充说明文本
                    Text {
                        text: root.informativeText
                        color: theme.textSecondary
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        visible: root.informativeText !== ""
                    }
                }
            }

            // 间距
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 12
                visible: root.detailedText !== ""
            }

            // 详细信息切换按钮 + 展开区域
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.detailedText !== ""
                spacing: 8

                // "Show Details..." / "Hide Details..." 按钮
                Rectangle {
                    Layout.preferredHeight: 28
                    Layout.fillWidth: true
                    color: "transparent"

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: detailsExpanded ? qsTr("Hide Details...") : qsTr("Show Details...")
                        color: theme.accent
                        font.pixelSize: 11
                        font.underline: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            detailsExpanded = !detailsExpanded
                        }
                    }
                }

                // 详细信息文本区域
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 100
                    color: theme.bg0
                    radius: theme.radiusS
                    border.width: 1
                    border.color: theme.border
                    opacity: 0.6
                    visible: detailsExpanded
                    clip: true

                    Flickable {
                        anchors.fill: parent
                        anchors.margins: 8
                        contentWidth: detailText.paintedWidth
                        contentHeight: detailText.paintedHeight
                        clip: true

                        Text {
                            id: detailText
                            text: root.detailedText
                            color: theme.textDim
                            font.pixelSize: 11
                            font.family: "Consolas, Courier New, monospace"
                            wrapMode: Text.WrapAnywhere
                            width: parent.width
                        }
                    }
                }
            }

            // 弹性占位
            Item {
                Layout.fillHeight: true
            }
        }
    }

    // ============================================================================
    // 按钮栏
    // ============================================================================

    footer: Rectangle {
        color: "transparent"
        height: 52

        // 顶部分隔线
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: theme.border
            opacity: 0.4
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 8
            // Help 按钮靠右
            layoutDirection: Qt.LeftToRight

            Item {
                Layout.fillWidth: true
            }

            // 使用 Repeater 动态渲染按钮
            Repeater {
                id: buttonRepeater
                model: getButtonList()

                delegate: Rectangle {
                    id: btnDelegate
                    property var btnData: modelData
                    property bool hovered: false

                    Layout.preferredWidth: Math.max(btnMinWidth, btnText.implicitWidth + 24)
                    Layout.preferredHeight: 32
                    radius: 6
                    color: {
                        if (!enabled) return "transparent"
                        if (btnPressed) return theme.accentSoft
                        if (hovered) return theme.bg2
                        return theme.bg1
                    }
                    border.width: 1
                    border.color: hovered ? theme.accent : theme.border
                    opacity: enabled ? 1.0 : 0.4

                    property bool btnPressed: false
                    property int btnMinWidth: 72

                    Text {
                        id: btnText
                        anchors.centerIn: parent
                        text: btnData.text
                        color: theme.textPrimary
                        font.pixelSize: 12
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: btnDelegate.hovered = true
                        onExited: {
                            btnDelegate.hovered = false
                            btnDelegate.btnPressed = false
                        }
                        onPressed: btnDelegate.btnPressed = true
                        onReleased: btnDelegate.btnPressed = false
                        onClicked: root.handleButtonClick(btnData.flag, btnData.role)
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }
        }
    }

}

