/**
 * @file        CodeEditor.qml
 * @brief       深蓝主题的 C 代码编辑器：行号栏 + 语法高亮 + 可调字体字号。
 * @details     本组件是「C 代码控制」对话框的编辑核心，由以下部分组成：
 *              - 左侧行号栏：用只读 TextArea 绘制，因此与正文使用完全相同的字体度量，
 *                不会出现行号与代码逐行错位；行号随内容纵向滚动；
 *              - 正文：TextArea 放在 Flickable 中，NoWrap 时自动扩展内容宽度，
 *                Wrap 时随视口换行；
 *              - 语法高亮：C++ 侧 `CodeEditorHighlighter` 绑定 `textArea.textDocument`，
 *                颜色全部来自 DeepBlueTheme 派生色，因此与整体 UI 主题一致；
 *              - 字体：默认读取 `CodeEditorSettings`（JetBrains Mono / 14），
 *                外部也可以直接覆盖 fontFamily / fontSize 属性；
 *              - 深色滚动条：与 SceneControlPanel 中的 PanelScrollBar 保持同一视觉。
 *
 *              用法：
 *              - `text` / `readOnly` / `placeholderText` 直接转发给内部 TextArea；
 *              - 用户编辑时发出 `textEdited()`，程序设置 text 时不会发出该信号；
 *              - `wrapMode: TextEdit.Wrap` 可用于诊断输出等只读场景。
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "qrc:/components/components/theme"
import EditorManager 1.0
import "CIndent.js" as CIndent

Rectangle {
    id: root

    // ------------------------------------------------------------------
    // 对外属性
    // ------------------------------------------------------------------

    /** @brief 编辑器文本内容（读写），供绑定源码使用。 */
    property alias source: codeArea.text

    /** @brief 是否只读；只读时仍可选中与复制。 */
    property alias readOnly: codeArea.readOnly

    /** @brief 空文本时的占位提示。 */
    property alias placeholderText: codeArea.placeholderText

    /** @brief 字体族名称；默认跟随全局代码编辑器设置。 */
    property string fontFamily: CodeEditorSettings.fontFamily

    /** @brief 字号（像素）；默认跟随全局代码编辑器设置。 */
    property int fontSize: CodeEditorSettings.fontSize

    /** @brief 是否显示行号栏。 */
    property bool showLineNumbers: true

    /** @brief 换行模式：TextEdit.NoWrap 或 TextEdit.Wrap。 */
    property int wrapMode: TextEdit.NoWrap

    /** @brief 按 Tab 键插入的空格数。 */
    property int tabSize: 4

    /** @brief 是否启用语法高亮。 */
    property bool highlighting: true

    /** @brief 正文默认颜色。 */
    property color textColor: theme.textPrimary

    /** @brief 关键字颜色。 */
    property color keywordColor: theme.accent

    /** @brief 内建类型颜色。 */
    property color typeColor: "#7FD1FF"

    /** @brief 预处理指令颜色。 */
    property color preprocessorColor: "#C792EA"

    /** @brief 字符串颜色。 */
    property color stringColor: theme.warning

    /** @brief 字符常量颜色。 */
    property color charColor: "#FFD479"

    /** @brief 数值常量颜色。 */
    property color numberColor: "#F78C6C"

    /** @brief 注释颜色。 */
    property color commentColor: theme.textDim

    /** @brief 函数名颜色。 */
    property color functionColor: theme.success

    /** @brief 用户编辑文本时发出（程序化设置 source 不发出）。 */
    signal textEdited()

    /** @brief 用户编辑文本时携带最新内容发出，方便调用方直接回写模型。 */
    signal edited(string value)

    /** @brief 实际生效的字体族（fontFamily 为空时回退到全局设置）。 */
    readonly property string resolvedFontFamily: fontFamily.length > 0 ? fontFamily : CodeEditorSettings.fontFamily

    /** @brief 当前总行数。 */
    readonly property int lineCount: Math.max(1, codeArea.lineCount)

    /** @brief 行号栏显示的多行文本。 */
    readonly property string lineNumbersText: {
        const count = root.lineCount
        const numbers = new Array(count)
        for (let index = 0; index < count; ++index) {
            numbers[index] = String(index + 1)
        }
        return numbers.join("\n")
    }

    implicitWidth: 560
    implicitHeight: 260
    color: theme.bg0
    radius: 6
    border.width: 1
    border.color: codeArea.activeFocus ? theme.accent : theme.border
    clip: true

    DeepBlueTheme { id: theme }

    // main.cpp 已经用 QFontDatabase 注册字体；这里再加载一次属于幂等操作，
    // 用于兜底「QML 预览 / 字体注册顺序异常」的场景。
    FontLoader {
        id: regularFontLoader
        source: "qrc:/fonts/JetBrainsMono-Regular.ttf"
    }
    FontLoader {
        id: italicFontLoader
        source: "qrc:/fonts/JetBrainsMono-Italic.ttf"
    }

    // ------------------------------------------------------------------
    // 内部组件
    // ------------------------------------------------------------------

    /** @brief 与场景控制面板一致的深色滚动条。 */
    component CodeScrollBar: ScrollBar {
        id: bar
        policy: ScrollBar.AsNeeded
        visible: policy === ScrollBar.AlwaysOn || (policy !== ScrollBar.AlwaysOff && size < 1)
        contentItem: Rectangle {
            implicitWidth: 6
            implicitHeight: 6
            radius: 3
            color: bar.pressed ? theme.accent : (bar.hovered ? theme.border : theme.bg2)
        }
    }

    /** @brief 按当前字号测量行号栏宽度。 */
    TextMetrics {
        id: gutterMetrics
        font.family: root.resolvedFontFamily
        font.pixelSize: root.fontSize
        text: String(root.lineCount)
    }

    /** @brief 行号栏宽度：最长行号的像素宽 + 左右留白。 */
    readonly property real gutterWidth: Math.max(30, gutterMetrics.width + 20)

    /**
     * @brief 让光标（以及输入位置）保持在可视区域内。
     * @details 由 cursorRectangleChanged 触发，只在编辑器获得焦点时生效，
     *          避免程序化设置文本时光标跳动。
     */
    function ensureCursorVisible() {
        if (!codeArea.activeFocus) {
            return
        }
        const rect = codeArea.cursorRectangle
        if (!rect || rect.height <= 0) {
            return
        }
        const margin = 4
        if (rect.y < flick.contentY + margin) {
            flick.contentY = Math.max(0, rect.y - margin)
        } else if (rect.y + rect.height > flick.contentY + flick.height - margin) {
            flick.contentY = rect.y + rect.height - flick.height + margin
        }
        if (flick.contentWidth > flick.width) {
            if (rect.x < flick.contentX + margin) {
                flick.contentX = Math.max(0, rect.x - margin)
            } else if (rect.x + rect.width > flick.contentX + flick.width - margin) {
                flick.contentX = rect.x + rect.width - flick.width + margin
            }
        }
    }

    // ------------------------------------------------------------------
    // 布局
    // ------------------------------------------------------------------

    RowLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        /** @brief 行号栏：只读 TextArea，与正文共享字体度量。 */
        Item {
            id: gutter
            visible: root.showLineNumbers
            Layout.fillHeight: true
            Layout.preferredWidth: root.showLineNumbers ? root.gutterWidth : 0
            clip: true

            Rectangle {
                anchors.fill: parent
                color: theme.bg1
            }
            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: theme.bg2
            }

            Basic.TextArea {
                id: gutterArea
                width: gutter.width
                height: gutter.height
                y: -flick.contentY
                text: root.lineNumbersText
                readOnly: true
                selectByMouse: false
                activeFocusOnPress: false
                wrapMode: TextEdit.NoWrap
                color: theme.textDim
                font.family: root.resolvedFontFamily
                font.pixelSize: root.fontSize
                horizontalAlignment: Text.AlignRight
                leftPadding: 4
                rightPadding: 10
                topPadding: codeArea.topPadding
                bottomPadding: 0
                background: null
            }
        }

        /** @brief 正文视口：Flickable 承载 TextArea，自行管理滚动范围。 */
        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.AutoFlickDirection
            contentWidth: Math.max(width, codeArea.contentWidth + codeArea.leftPadding + codeArea.rightPadding)
            contentHeight: Math.max(height, codeArea.contentHeight + codeArea.topPadding + codeArea.bottomPadding)

            ScrollBar.vertical: CodeScrollBar {
                parent: flick
                x: flick.mirrored ? 0 : flick.width - width
                y: 0
                height: flick.height
            }
            ScrollBar.horizontal: CodeScrollBar {
                parent: flick
                orientation: Qt.Horizontal
                y: flick.mirrored ? 0 : flick.height - height
                x: 0
                width: flick.width
            }

            Basic.TextArea {
                id: codeArea
                textFormat: TextEdit.PlainText

                // NoWrap 时按内容撑开（横向可滚动），Wrap 时跟随视口宽度，
                // 避免 wrap 模式下「高度依赖内容、内容依赖宽度」的绑定循环。
                width: root.wrapMode === TextEdit.NoWrap
                    ? Math.max(flick.width, contentWidth + leftPadding + rightPadding)
                    : flick.width
                height: root.wrapMode === TextEdit.NoWrap
                    ? Math.max(flick.height, contentHeight + topPadding + bottomPadding)
                    : flick.height

                font.family: root.resolvedFontFamily
                font.pixelSize: root.fontSize
                color: root.textColor
                selectionColor: Qt.rgba(theme.accentSoft.r, theme.accentSoft.g, theme.accentSoft.b, 0.55)
                selectedTextColor: theme.textPrimary
                wrapMode: root.wrapMode
                selectByMouse: true
                persistentSelection: true
                leftPadding: 10
                rightPadding: 10
                topPadding: 8
                bottomPadding: 8
                background: null

                onTextChanged: {
                    // 只有用户获得焦点并实际编辑时才回写模型，重复赋相同内容不会触发。
                    if (activeFocus && !root.readOnly) {
                        root.textEdited()
                        root.edited(codeArea.text)
                    }
                }
                onCursorRectangleChanged: root.ensureCursorVisible()

                Keys.onPressed: function (event) {
                    if (root.readOnly) {
                        return
                    }
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                            && event.modifiers === Qt.NoModifier) {
                        root.insertNewline()
                        event.accepted = true
                        return
                    }
                    if (event.text === "}" && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))) {
                        const edit = CIndent.closingBrace(codeArea.text, codeArea.selectionStart, codeArea.selectionEnd)
                        if (edit) {
                            root.applyIndentEdit(edit)
                            event.accepted = true
                            return
                        }
                    }
                    if (event.key === Qt.Key_Tab) {
                        root.insertIndent()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Backtab) {
                        root.removeIndent()
                        event.accepted = true
                    }
                }
            }
        }
    }

    /** @brief 语法高亮：由 C++ 侧扫描词法，颜色来自本组件属性。 */
    CodeEditorHighlighter {
        id: highlighter
        textDocument: root.highlighting ? codeArea.textDocument : null
        keywordColor: root.keywordColor
        typeColor: root.typeColor
        preprocessorColor: root.preprocessorColor
        stringColor: root.stringColor
        charColor: root.charColor
        numberColor: root.numberColor
        commentColor: root.commentColor
        functionColor: root.functionColor
    }

    Component.onCompleted: {
        // 兜底：部分 Qt 版本在组件创建瞬间 TextArea.textDocument 可能尚未就绪。
        if (!highlighter.textDocument) {
            highlighter.textDocument = codeArea.textDocument
        }
        // 字体可能在单例注册之后才加载完成，这里刷新一次可等待字体列表。
        CodeEditorSettings.refreshFontFamilies()
    }

    // ------------------------------------------------------------------
    // 缩进辅助
    // ------------------------------------------------------------------

    /** @brief 在光标处插入 tabSize 个空格。 */
    function insertIndent() {
        if (codeArea.readOnly) {
            return
        }
        let indent = ""
        for (let index = 0; index < Math.max(1, root.tabSize); ++index) {
            indent += " "
        }
        codeArea.insert(codeArea.cursorPosition, indent)
    }

    /** @brief 删除当前行行首最多 tabSize 个空格（Shift+Tab）。 */
    function removeIndent() {
        if (codeArea.readOnly) {
            return
        }
        const content = codeArea.text
        const position = codeArea.cursorPosition
        const lineStart = content.lastIndexOf("\n", position - 1) + 1
        let count = 0
        while (count < root.tabSize && lineStart + count < position && content.charAt(lineStart + count) === " ") {
            ++count
        }
        if (count > 0) {
            codeArea.remove(lineStart, lineStart + count)
        }
    }
    /** @brief 插入按 C 代码块层级计算的换行，不在普通语句后逐次增加缩进。 */
    function insertNewline() {
        if (codeArea.readOnly) return
        applyIndentEdit(CIndent.newline(codeArea.text, codeArea.selectionStart, codeArea.selectionEnd))
    }

    /** @brief 应用局部替换并将光标放到新行的正确缩进位置。 */
    function applyIndentEdit(edit) {
        if (edit.end > edit.start) codeArea.remove(edit.start, edit.end)
        codeArea.insert(edit.start, edit.text)
        codeArea.cursorPosition = edit.cursor
    }
}
