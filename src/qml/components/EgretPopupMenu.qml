/**
 * @file        EgretPopupMenu.qml
 * @brief       可复用的多级弹出菜单组件
 * @details     独立的 Popup 菜单组件，支持多级子菜单、checkable 项、图标、快捷键显示
 *              和键盘导航。与 EgretMenuBar 使用相同的菜单项数据格式，可复用于任意位置。
 *
 *              菜单项数据格式：
 *              {
 *                  text: "菜单文字",          // 必填，支持 & 助记符
 *                  shortcut: "Ctrl+S",       // 可选，快捷键显示
 *                  icon: "qrc:/...",         // 可选，图标路径
 *                  checkable: true,          // 可选，是否可勾选
 *                  checked: false,           // 可选，勾选状态
 *                  enabled: true,            // 可选，默认 true
 *                  separator: true,          // 可选，是否为分隔线
 *                  submenu: [ ... ],         // 可选，子菜单项数组
 *                  onTriggered: function(){} // 可选，点击回调
 *              }
 *
 * @author       作者姓名 <作者邮箱>
 * @date         2026-06-05
 * @version      1.0.0
 *
 * @copyright    Copyright © 2025 公司名. All rights reserved.
 * @license      GPL v3.0
 *
 * @ingroup      View
 * @defgroup     组名
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Effects

Popup {
    id: popup

    // ============ 菜单数据 ============

    /** 菜单项数组，格式与 EgretMenuBar.menus[i].items 一致 */
    property var menuItems: []

    /**
     * 当前菜单在层级中的路径。
     * 根级菜单为空数组 []，第一级子菜单为 [父项索引]，
     * 更深层级为 [父项索引, 子项索引, ...]。
     */
    property var menuPath: []

    // ============ 样式属性（与 EgretMenuBar 保持一致） ============

    property int menuMinWidth: 180
    property int menuItemHeight: 28
    property int menuPadding: 8
    property int menuRadius: 10
    property int iconSize: 16
    property int iconSpacing: 8
    property int checkMarkWidth: 20
    property int subMenuArrowSize: 12
    property int subMenuOpenDelay: 300
    property int subMenuCloseDelay: 100
    property int subMenuMaxVisibleItems: 15
    property int subMenuScrollHeight: 420

    property color menuBackground: "#0E233F"
    property color menuBorderColor: "#2D5178"
    property color menuItemHoverBackground: "#1B3D63"
    property color menuItemPressedBackground: "#25507E"
    property color menuItemTextColor: "#D3E6FF"
    property color menuItemHoverTextColor: "#FFFFFF"
    property color menuSeparatorColor: "#2C4E74"
    property color textDisabledColor: "#7E98B6"
    property color subMenuArrowColor: "#D3E6FF"
    property color iconEffectColor: "#DDDDDD"
    property double iconEffectColorization: 1.0
    property double iconEffectBrightness: 0.75

    property bool forceCheckMarkVisible: false
    property bool forceIconVisible: false

    // ============ 信号 ============

    /**
     * 菜单项被触发（点击非子菜单项）。
     * @param path  从根菜单到被触发项的路径数组
     * @param itemData  被触发项的原始数据对象
     */
    signal menuTriggered(var path, var itemData)

    /**
     * checkable 菜单项的勾选状态改变。
     * @param path  从根菜单到该项的路径数组
     * @param itemData  该项的原始数据对象
     * @param checked  新的勾选状态
     */
    signal menuCheckedChanged(var path, var itemData, bool checked)

    /** 菜单关闭时发出 */
    signal menuClosed()

    // ============ 内部状态 ============

    /** 当前高亮的项索引 */
    property int highlightedIndex: -1

    /** 所有活跃的子菜单 Popup 列表 */
    property var childPopups: []

    /** 关闭计时器列表（用于延迟关闭子菜单） */
    property var closeTimers: []

    /** 弹出宽度（在 onOpened 中由 calculateMenuWidth() 计算并赋值） */
    property int popupWidth: menuMinWidth

    // ============ Popup 基础设置 ============

    modal: false
    focus: true
    padding: menuPadding
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: popup.menuBackground
        border.width: 1
        border.color: popup.menuBorderColor
        radius: popup.menuRadius
    }

    onOpened: {
        highlightedIndex = firstEnabledIndex();
        forceActiveFocus();
    }

    onClosed: {
        closeAllChildPopups();
        menuClosed();
    }

    // ============ 辅助函数 ============

    /** HTML 转义 */
    function escapeHtml(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    /** 解析 & 助记符信息 */
    function mnemonicInfo(text) {
        var display = "";
        var mnemonic = "";
        var index = -1;
        var outputIndex = 0;

        for (var i = 0; i < text.length; i++) {
            var ch = text[i];
            if (ch === "&") {
                if (i + 1 < text.length) {
                    var next = text[i + 1];
                    if (next === "&") {
                        display += "&";
                        i++;
                        outputIndex++;
                        continue;
                    }
                    if (index < 0) {
                        index = outputIndex;
                        mnemonic = next.toLowerCase();
                    }
                    display += next;
                    i++;
                    outputIndex++;
                    continue;
                }
            }
            display += ch;
            outputIndex++;
        }

        return {
            display: display,
            mnemonic: mnemonic,
            index: index
        };
    }

    /** 生成带下划线的富文本（助记符高亮） */
    function richTextWithUnderline(display, index) {
        if (index < 0 || index >= display.length) {
            return escapeHtml(display);
        }
        var before = escapeHtml(display.slice(0, index));
        var middle = escapeHtml(display.slice(index, index + 1));
        var after = escapeHtml(display.slice(index + 1));
        return before + "<u>" + middle + "</u>" + after;
    }

    /** 判断菜单项是否有子菜单 */
    function hasSubMenu(item) {
        return !!(item && item.submenu && item.submenu.length > 0);
    }

    /** 计算菜单所需宽度 */
    function calculateMenuWidth() {
        var maxWidth = menuMinWidth;
        for (var i = 0; i < menuItems.length; i++) {
            var item = menuItems[i];
            if (!item || item.separator) continue;

            var labelInfo = mnemonicInfo(item.text || "");
            menuLabelMetrics.text = labelInfo.display;
            var labelWidth = menuLabelMetrics.width;

            var shortcutWidth = 0;
            if (item.shortcut) {
                menuShortcutMetrics.text = item.shortcut;
                shortcutWidth = menuShortcutMetrics.width + 18;
            }

            var checkWidth = (item.checkable === true || forceCheckMarkVisible) ? checkMarkWidth : 0;
            var iconWidthVal = (item.icon || forceIconVisible) ? iconSize : 0;
            var arrowWidth = hasSubMenu(item) ? subMenuArrowSize + 8 : 0;

            var contentWidth = checkWidth + iconWidthVal + labelWidth + shortcutWidth + arrowWidth;
            var spacingSlots = 0;
            if (checkWidth > 0) spacingSlots++;
            if (iconWidthVal > 0) spacingSlots++;
            if (shortcutWidth > 0) spacingSlots++;
            if (arrowWidth > 0) spacingSlots++;
            contentWidth += spacingSlots * iconSpacing;

            var totalWidth = contentWidth + menuPadding * 2 + 20;
            if (totalWidth > maxWidth) maxWidth = totalWidth;
        }
        return maxWidth;
    }

    /** 查找第一个可用的菜单项索引 */
    function firstEnabledIndex() {
        for (var i = 0; i < menuItems.length; i++) {
            var item = menuItems[i];
            if (item && !item.separator && item.enabled !== false) return i;
        }
        return -1;
    }

    /** 从 startIndex 沿 direction 方向查找下一个可用项 */
    function nextEnabledIndex(startIndex, direction) {
        if (menuItems.length === 0) return -1;
        var index = startIndex;
        for (var attempt = 0; attempt < menuItems.length; attempt++) {
            index += direction;
            if (index < 0) index = menuItems.length - 1;
            else if (index >= menuItems.length) index = 0;
            var item = menuItems[index];
            if (item && !item.separator && item.enabled !== false) return index;
        }
        return startIndex;
    }

    // ============ 子菜单管理 ============

    /** 取消所有待处理的关闭计时器 */
    function cancelAllCloseTimers() {
        for (var i = closeTimers.length - 1; i >= 0; i--) {
            var timer = closeTimers[i];
            if (timer && typeof timer.stop === 'function') {
                try { timer.stop(); timer.destroy(); } catch (e) {}
            }
        }
        closeTimers = [];
    }

    /** 注册关闭计时器 */
    function registerCloseTimer(timer) {
        closeTimers.push(timer);
    }

    /** 注销关闭计时器 */
    function unregisterCloseTimer(timer) {
        var idx = closeTimers.indexOf(timer);
        if (idx >= 0) closeTimers.splice(idx, 1);
    }

    /** 路径匹配检查 */
    function pathMatches(path1, path2) {
        if (!path1 || !path2) return false;
        if (path1.length !== path2.length) return false;
        for (var i = 0; i < path1.length; i++) {
            if (path1[i] !== path2[i]) return false;
        }
        return true;
    }

    /** 判断 path1 和 path2 是否共享前缀 */
    function isParentChildPath(path1, path2) {
        if (!path1 || !path2) return false;
        var minLen = Math.min(path1.length, path2.length);
        for (var i = 0; i < minLen; i++) {
            if (path1[i] !== path2[i]) return false;
        }
        return true;
    }

    /** 检查指定路径的子菜单是否已打开 */
    function isSubMenuOpen(path) {
        for (var i = 0; i < childPopups.length; i++) {
            if (pathMatches(childPopups[i].subPath, path)) return true;
        }
        return false;
    }

    /** 关闭不在 keepPath 前缀内的所有子菜单 */
    function closeSubMenusExceptPath(keepPath) {
        for (var i = childPopups.length - 1; i >= 0; i--) {
            if (!isParentChildPath(childPopups[i].subPath, keepPath)) {
                closeChildPopup(childPopups[i]);
            }
        }
    }

    /** 按路径关闭子菜单 */
    function closeSubMenuByPath(path) {
        for (var i = childPopups.length - 1; i >= 0; i--) {
            if (pathMatches(childPopups[i].subPath, path)) {
                closeChildPopup(childPopups[i]);
            }
        }
    }

    /** 关闭单个子菜单 Popup */
    function closeChildPopup(popupInstance) {
        var idx = childPopups.indexOf(popupInstance);
        if (idx >= 0) childPopups.splice(idx, 1);
        if (popupInstance && popupInstance.visible) {
            popupInstance.close();
            if (typeof popupInstance.destroy === 'function') {
                popupInstance.destroy();
            }
        }
    }

    /** 关闭所有子菜单 */
    function closeAllChildPopups() {
        cancelAllCloseTimers();
        for (var i = childPopups.length - 1; i >= 0; i--) {
            var p = childPopups[i];
            if (p && p.visible) p.close();
            if (p && typeof p.destroy === 'function') p.destroy();
        }
        childPopups = [];
    }

    /** 打开子菜单 */
    function openSubMenu(parentPopupInstance, itemIndex) {
        var item;
        if (parentPopupInstance === popup) {
            item = menuItems[itemIndex];
        } else {
            item = parentPopupInstance.menuItems[itemIndex];
        }
        if (!hasSubMenu(item)) return;

        cancelAllCloseTimers();

        var subItems = item.submenu;
        var parentPath = (parentPopupInstance === popup) ? menuPath : parentPopupInstance.subPath;
        var subPath = parentPath.concat([itemIndex]);

        // 计算子菜单位置 — 使用父级 Popup 的实际位置
        var parentX = parentPopupInstance.x + parentPopupInstance.width;
        var parentY = parentPopupInstance.y + itemIndex * menuItemHeight + menuPadding;

        // 计算子菜单尺寸
        var subWidth = calculateSubMenuWidth(subItems);
        var subHeight = Math.min(subItems.length * menuItemHeight + menuPadding * 2,
                                   subMenuScrollHeight + menuPadding * 2);

        // 屏幕边界检测
        var screenWidth = (popup.parent) ? popup.parent.width : 1920;
        var screenHeight = (popup.parent) ? popup.parent.height : 1080;

        var openRight = (parentX + subWidth <= screenWidth);
        var finalX = openRight ? parentX : (popup.x - subWidth);

        var openDown = (parentY + subHeight <= screenHeight);
        var finalY;
        if (openDown) {
            finalY = Math.max(0, parentY);
        } else {
            finalY = Math.max(0, parentY + menuItemHeight - subHeight);
        }

        // 创建子菜单 Popup
        var child = subMenuPopupComponent.createObject(popup.parent ? popup.parent : popup, {
            "menuItems": subItems,
            "subPath": subPath,
            "parentMenuPopup": parentPopupInstance,
            "highlightedIndex": -1,
            "x": finalX,
            "y": finalY,
            "subMenuWidth": subWidth
        });

        childPopups.push(child);
        child.open();
    }

    /** 计算子菜单宽度 */
    function calculateSubMenuWidth(items) {
        var maxWidth = menuMinWidth;
        for (var i = 0; i < items.length; i++) {
            var item = items[i];
            if (!item || item.separator) continue;

            var labelInfo = mnemonicInfo(item.text || "");
            menuLabelMetrics.text = labelInfo.display;
            var labelWidth = menuLabelMetrics.width;

            var shortcutWidth = 0;
            if (item.shortcut) {
                menuShortcutMetrics.text = item.shortcut;
                shortcutWidth = menuShortcutMetrics.width + 18;
            }

            var checkWidth = (item.checkable === true || forceCheckMarkVisible) ? checkMarkWidth : 0;
            var iconWidthVal = (item.icon || forceIconVisible) ? iconSize : 0;
            var arrowWidth = hasSubMenu(item) ? subMenuArrowSize + 8 : 0;

            var contentWidth = checkWidth + iconWidthVal + labelWidth + shortcutWidth + arrowWidth;
            var spacingSlots = 0;
            if (checkWidth > 0) spacingSlots++;
            if (iconWidthVal > 0) spacingSlots++;
            if (shortcutWidth > 0) spacingSlots++;
            if (arrowWidth > 0) spacingSlots++;
            contentWidth += spacingSlots * iconSpacing;

            var totalWidth = contentWidth + menuPadding * 2 + 20;
            if (totalWidth > maxWidth) maxWidth = totalWidth;
        }
        return maxWidth;
    }

    // ============ 触发菜单项 ============

    /** 触发菜单项（点击 / 回车） */
    function triggerMenuItem(itemIndex) {
        var item = menuItems[itemIndex];
        if (!item || item.separator || item.enabled === false) return;

        if (item.checkable === true) {
            var newChecked = !item.checked;
            item.checked = newChecked;
            menuCheckedChanged(menuPath.concat([itemIndex]), item, newChecked);
        }

        if (item.onTriggered && typeof item.onTriggered === 'function') {
            item.onTriggered();
        }

        menuTriggered(menuPath.concat([itemIndex]), item);

        // 触发后关闭菜单（除非是 checkable 项）
        if (item.checkable !== true) {
            closeAllMenus();
        }
    }

    /** 向上追溯关闭所有菜单 */
    function closeAllMenus() {
        closeAllChildPopups();
        popup.close();
    }

    // ============ 文本度量（用于宽度计算） ============

    TextMetrics {
        id: menuLabelMetrics
        font.pixelSize: 13
        text: ""
    }

    TextMetrics {
        id: menuShortcutMetrics
        font.pixelSize: 12
        text: ""
    }

    // ============ 内容区域 ============

    contentItem: Item {
        id: contentRoot
        implicitWidth: popup.popupWidth
        implicitHeight: Math.min(menuList.contentHeight, popup.subMenuScrollHeight)
        focus: true

        Keys.onPressed: function (event) {
            if (event.key === Qt.Key_Escape) {
                popup.closeAllMenus();
                event.accepted = true;
                return;
            }
            if (event.key === Qt.Key_Down) {
                popup.highlightedIndex = popup.nextEnabledIndex(popup.highlightedIndex, 1);
                event.accepted = true;
                return;
            }
            if (event.key === Qt.Key_Up) {
                popup.highlightedIndex = popup.nextEnabledIndex(popup.highlightedIndex, -1);
                event.accepted = true;
                return;
            }
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                if (popup.highlightedIndex >= 0) {
                    var item = popup.menuItems[popup.highlightedIndex];
                    if (popup.hasSubMenu(item)) {
                        popup.openSubMenu(popup, popup.highlightedIndex);
                        if (popup.childPopups.length > 0) {
                            popup.childPopups[popup.childPopups.length - 1].forceActiveFocus();
                        }
                    } else {
                        popup.triggerMenuItem(popup.highlightedIndex);
                    }
                }
                event.accepted = true;
                return;
            }
            if (event.key === Qt.Key_Right) {
                var currentItem = popup.menuItems[popup.highlightedIndex];
                if (popup.hasSubMenu(currentItem)) {
                    popup.openSubMenu(popup, popup.highlightedIndex);
                    if (popup.childPopups.length > 0) {
                        popup.childPopups[popup.childPopups.length - 1].forceActiveFocus();
                    }
                }
                event.accepted = true;
                return;
            }
            if (event.key === Qt.Key_Left) {
                // 如果有父级菜单，关闭当前菜单回到父级
                if (popup.parentMenuPopup) {
                    popup.closeAllChildPopups();
                    popup.close();
                    if (popup.parentMenuPopup) {
                        popup.parentMenuPopup.forceActiveFocus();
                    }
                }
                event.accepted = true;
                return;
            }
            // 助记符搜索
            if (event.text && event.text.length === 1) {
                var keyText = event.text.toLowerCase();
                for (var i = 0; i < popup.menuItems.length; i++) {
                    var info = popup.mnemonicInfo(popup.menuItems[i].text || "");
                    if (info.mnemonic === keyText) {
                        var targetItem = popup.menuItems[i];
                        if (popup.hasSubMenu(targetItem)) {
                            popup.highlightedIndex = i;
                            popup.openSubMenu(popup, i);
                            if (popup.childPopups.length > 0) {
                                popup.childPopups[popup.childPopups.length - 1].forceActiveFocus();
                            }
                        } else {
                            popup.triggerMenuItem(i);
                        }
                        event.accepted = true;
                        return;
                    }
                }
            }
        }

        ListView {
            id: menuList
            anchors.fill: parent
            width: contentRoot.implicitWidth
            implicitHeight: Math.min(contentHeight, popup.subMenuScrollHeight)
            model: popup.menuItems
            interactive: contentHeight > popup.subMenuScrollHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            delegate: Item {
                id: menuItem

                property var itemData: modelData
                property bool isSeparator: itemData && itemData.separator === true
                property bool isEnabled: itemData && itemData.enabled !== false
                property bool isCheckable: itemData && itemData.checkable === true
                property bool isChecked: itemData && itemData.checked === true
                property string itemIcon: itemData && itemData.icon || ""
                property bool hasSub: popup.hasSubMenu(itemData)

                implicitWidth: menuList.width
                width: menuList.width
                height: isSeparator ? 8 : popup.menuItemHeight

                // ---- 普通菜单项背景 ----
                Rectangle {
                    anchors.fill: parent
                    visible: !isSeparator
                    radius: 6
                    color: popup.highlightedIndex === index
                        ? popup.menuItemHoverBackground
                        : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: popup.iconSpacing

                        // Checkmark 区域
                        Item {
                            Layout.minimumWidth: isCheckable || popup.forceCheckMarkVisible ? popup.checkMarkWidth : 0
                            Layout.preferredWidth: isCheckable || popup.forceCheckMarkVisible ? popup.checkMarkWidth : 0
                            Layout.maximumWidth: isCheckable || popup.forceCheckMarkVisible ? popup.checkMarkWidth : 0
                            Layout.minimumHeight: isCheckable || popup.forceCheckMarkVisible ? popup.iconSize : 0
                            Layout.preferredHeight: isCheckable || popup.forceCheckMarkVisible ? popup.iconSize : 0
                            Layout.maximumHeight: isCheckable || popup.forceCheckMarkVisible ? popup.iconSize : 0
                            visible: isCheckable || popup.forceCheckMarkVisible

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 2
                                text: isChecked ? "✔" : ""
                                color: !menuItem.isEnabled
                                    ? popup.textDisabledColor
                                    : (popup.highlightedIndex === index
                                        ? popup.menuItemHoverTextColor
                                        : popup.menuItemTextColor)
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        // 图标区域
                        Item {
                            id: iconItemContainer
                            property bool hasIcon: itemIcon !== "" || popup.forceIconVisible
                            property bool isSvgSource: {
                                if (!itemIcon) return false;
                                var sourceStr = itemIcon.toString().toLowerCase();
                                return sourceStr.endsWith(".svg") || sourceStr.endsWith(".svgz");
                            }

                            Layout.minimumWidth: hasIcon ? popup.iconSize : 0
                            Layout.preferredWidth: hasIcon ? popup.iconSize : 0
                            Layout.maximumWidth: hasIcon ? popup.iconSize : 0
                            Layout.minimumHeight: hasIcon ? popup.iconSize : 0
                            Layout.preferredHeight: hasIcon ? popup.iconSize : 0
                            Layout.maximumHeight: hasIcon ? popup.iconSize : 0
                            visible: hasIcon

                            Image {
                                id: iconImage
                                anchors.fill: parent
                                visible: iconItemContainer.hasIcon
                                source: itemIcon
                                fillMode: Image.PreserveAspectFit
                                opacity: !menuItem.isEnabled ? 0.5 : 1
                            }

                            MultiEffect {
                                anchors.fill: iconImage
                                source: iconImage
                                colorization: popup.iconEffectColorization
                                colorizationColor: popup.iconEffectColor
                                brightness: popup.iconEffectBrightness
                                visible: iconItemContainer.hasIcon && menuItem.isEnabled && iconItemContainer.isSvgSource
                                enabled: iconItemContainer.isSvgSource
                            }
                        }

                        // 文字
                        Text {
                            text: popup.richTextWithUnderline(
                                popup.mnemonicInfo(itemData.text || "").display,
                                popup.mnemonicInfo(itemData.text || "").index)
                            textFormat: Text.RichText
                            color: !menuItem.isEnabled
                                ? popup.textDisabledColor
                                : (popup.highlightedIndex === index
                                    ? popup.menuItemHoverTextColor
                                    : popup.menuItemTextColor)
                            font.pixelSize: 13
                        }

                        Item { Layout.fillWidth: true }

                        // 快捷键
                        Text {
                            text: itemData.shortcut || ""
                            color: !menuItem.isEnabled ? popup.textDisabledColor : popup.menuItemTextColor
                            font.pixelSize: 12
                            visible: !hasSub
                        }

                        // 子菜单箭头
                        Text {
                            text: "▶"
                            color: !menuItem.isEnabled
                                ? popup.textDisabledColor
                                : (popup.highlightedIndex === index
                                    ? popup.menuItemHoverTextColor
                                    : popup.subMenuArrowColor)
                            font.pixelSize: popup.subMenuArrowSize
                            visible: hasSub
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    MouseArea {
                        id: menuMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: menuItem.isEnabled
                        cursorShape: Qt.PointingHandCursor

                        property var hoverTimer: null
                        property var closeTimer: null

                        onEntered: {
                            popup.cancelAllCloseTimers();
                            popup.highlightedIndex = index;

                            var currentPath = popup.menuPath.concat([index]);
                            popup.closeSubMenusExceptPath(currentPath);

                            if (hasSub) {
                                if (hoverTimer) { hoverTimer.stop(); hoverTimer.destroy(); }
                                hoverTimer = timerComponent.createObject(menuMouseArea, {
                                    "running": true,
                                    "interval": popup.subMenuOpenDelay,
                                    "onTimeout": function () {
                                        popup.openSubMenu(popup, index);
                                        if (hoverTimer) { hoverTimer.destroy(); hoverTimer = null; }
                                    }
                                });
                            }
                        }

                        onExited: {
                            if (hoverTimer) {
                                hoverTimer.stop();
                                hoverTimer.destroy();
                                hoverTimer = null;
                            }

                            if (hasSub) {
                                var currentPath = popup.menuPath.concat([index]);
                                if (popup.isSubMenuOpen(currentPath)) {
                                    closeTimer = timerComponent.createObject(menuMouseArea, {
                                        "running": true,
                                        "interval": popup.subMenuCloseDelay,
                                        "onTimeout": function () {
                                            popup.closeSubMenuByPath(currentPath);
                                            popup.unregisterCloseTimer(closeTimer);
                                            if (closeTimer) { closeTimer.destroy(); closeTimer = null; }
                                        }
                                    });
                                    popup.registerCloseTimer(closeTimer);
                                }
                            }
                        }

                        onClicked: {
                            if (hasSub) {
                                var currentPath = popup.menuPath.concat([index]);
                                if (popup.isSubMenuOpen(currentPath)) {
                                    popup.closeSubMenuByPath(currentPath);
                                } else {
                                    popup.openSubMenu(popup, index);
                                }
                            } else {
                                popup.triggerMenuItem(index);
                            }
                        }
                    }
                }

                // ---- 分隔线 ----
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: 1
                    visible: isSeparator
                    color: popup.menuSeparatorColor
                }
            }
        }
    }

    // ============ 子菜单工厂组件 ============

    /**
     * 用于动态创建子菜单 Popup 的组件。
     * 每个子菜单是一个独立的 Popup，拥有与父级相同的 delegate 结构。
     */
    Component {
        id: subMenuPopupComponent

        Popup {
            id: subPopup

            property var menuItems: []
            property var subPath: []
            property var parentMenuPopup: null
            property int subMenuWidth: popup.menuMinWidth
            property int highlightedIndex: -1

            modal: false
            focus: true
            padding: popup.menuPadding
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

            background: Rectangle {
                color: popup.menuBackground
                border.width: 1
                border.color: popup.menuBorderColor
                radius: popup.menuRadius
            }

            onOpened: {
                highlightedIndex = subFirstEnabledIndex();
                forceActiveFocus();
            }

            onClosed: {
                // 如果父级菜单仍然可见，归还焦点
                if (parentMenuPopup && parentMenuPopup.visible) {
                    parentMenuPopup.forceActiveFocus();
                }
                // 递归关闭子菜单
                if (typeof closeAllSubPopups === 'function') {
                    closeAllSubPopups();
                }
            }

            function subFirstEnabledIndex() {
                for (var i = 0; i < menuItems.length; i++) {
                    var item = menuItems[i];
                    if (item && !item.separator && item.enabled !== false) return i;
                }
                return -1;
            }

            function subNextEnabledIndex(startIndex, direction) {
                if (menuItems.length === 0) return -1;
                var index = startIndex;
                for (var attempt = 0; attempt < menuItems.length; attempt++) {
                    index += direction;
                    if (index < 0) index = menuItems.length - 1;
                    else if (index >= menuItems.length) index = 0;
                    var item = menuItems[index];
                    if (item && !item.separator && item.enabled !== false) return index;
                }
                return startIndex;
            }

            function closeAllSubPopups() {
                // 子菜单的子菜单由 popup 的 childPopups 管理
                // 在关闭事件中不做额外处理，让 popup.closeAllChildPopups 负责
            }

            contentItem: Item {
                id: subContentRoot
                implicitWidth: subPopup.subMenuWidth
                implicitHeight: Math.min(subMenuList.contentHeight, popup.subMenuScrollHeight)
                focus: true

                Keys.onPressed: function (event) {
                    if (event.key === Qt.Key_Escape) {
                        subPopup.close();
                        event.accepted = true;
                        return;
                    }
                    if (event.key === Qt.Key_Down) {
                        subPopup.highlightedIndex = subPopup.subNextEnabledIndex(subPopup.highlightedIndex, 1);
                        event.accepted = true;
                        return;
                    }
                    if (event.key === Qt.Key_Up) {
                        subPopup.highlightedIndex = subPopup.subNextEnabledIndex(subPopup.highlightedIndex, -1);
                        event.accepted = true;
                        return;
                    }
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        if (subPopup.highlightedIndex >= 0) {
                            var item = subPopup.menuItems[subPopup.highlightedIndex];
                            if (popup.hasSubMenu(item)) {
                                popup.openSubMenu(subPopup, subPopup.highlightedIndex);
                            } else {
                                popup.triggerMenuItemFromSub(subPopup, subPopup.highlightedIndex);
                            }
                        }
                        event.accepted = true;
                        return;
                    }
                    if (event.key === Qt.Key_Right) {
                        var currentItem = subPopup.menuItems[subPopup.highlightedIndex];
                        if (popup.hasSubMenu(currentItem)) {
                            popup.openSubMenu(subPopup, subPopup.highlightedIndex);
                        }
                        event.accepted = true;
                        return;
                    }
                    if (event.key === Qt.Key_Left) {
                        subPopup.close();
                        if (subPopup.parentMenuPopup && subPopup.parentMenuPopup.visible) {
                            subPopup.parentMenuPopup.forceActiveFocus();
                        }
                        event.accepted = true;
                        return;
                    }
                    // 助记符搜索
                    if (event.text && event.text.length === 1) {
                        var keyText = event.text.toLowerCase();
                        for (var i = 0; i < subPopup.menuItems.length; i++) {
                            var info = popup.mnemonicInfo(subPopup.menuItems[i].text || "");
                            if (info.mnemonic === keyText) {
                                var targetItem = subPopup.menuItems[i];
                                if (popup.hasSubMenu(targetItem)) {
                                    subPopup.highlightedIndex = i;
                                    popup.openSubMenu(subPopup, i);
                                } else {
                                    popup.triggerMenuItemFromSub(subPopup, i);
                                }
                                event.accepted = true;
                                return;
                            }
                        }
                    }
                }

                ListView {
                    id: subMenuList
                    anchors.fill: parent
                    width: subContentRoot.implicitWidth
                    implicitHeight: Math.min(contentHeight, popup.subMenuScrollHeight)
                    model: subPopup.menuItems
                    interactive: contentHeight > popup.subMenuScrollHeight
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true

                    delegate: Item {
                        id: subMenuItem

                        property var itemData: modelData
                        property bool isSeparator: itemData && itemData.separator === true
                        property bool isEnabled: itemData && itemData.enabled !== false
                        property bool isCheckable: itemData && itemData.checkable === true
                        property bool isChecked: itemData && itemData.checked === true
                        property string itemIcon: itemData && itemData.icon || ""
                        property bool hasSub: popup.hasSubMenu(itemData)

                        implicitWidth: subMenuList.width
                        width: subMenuList.width
                        height: isSeparator ? 8 : popup.menuItemHeight

                        Rectangle {
                            anchors.fill: parent
                            visible: !isSeparator
                            radius: 6
                            color: subPopup.highlightedIndex === index
                                ? popup.menuItemHoverBackground
                                : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: popup.iconSpacing

                                Item {
                                    Layout.minimumWidth: isCheckable || popup.forceCheckMarkVisible ? popup.checkMarkWidth : 0
                                    Layout.preferredWidth: isCheckable || popup.forceCheckMarkVisible ? popup.checkMarkWidth : 0
                                    Layout.maximumWidth: isCheckable || popup.forceCheckMarkVisible ? popup.checkMarkWidth : 0
                                    Layout.minimumHeight: isCheckable || popup.forceCheckMarkVisible ? popup.iconSize : 0
                                    Layout.preferredHeight: isCheckable || popup.forceCheckMarkVisible ? popup.iconSize : 0
                                    Layout.maximumHeight: isCheckable || popup.forceCheckMarkVisible ? popup.iconSize : 0
                                    visible: isCheckable || popup.forceCheckMarkVisible

                                    Text {
                                        anchors.fill: parent
                                        anchors.leftMargin: 2
                                        text: isChecked ? "✔" : ""
                                        color: !subMenuItem.isEnabled
                                            ? popup.textDisabledColor
                                            : (subPopup.highlightedIndex === index
                                                ? popup.menuItemHoverTextColor
                                                : popup.menuItemTextColor)
                                        font.pixelSize: 14
                                        horizontalAlignment: Text.AlignLeft
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                Item {
                                    id: subIconItemContainer
                                    property bool hasIcon: itemIcon !== "" || popup.forceIconVisible
                                    property bool isSvgSource: {
                                        if (!itemIcon) return false;
                                        var sourceStr = itemIcon.toString().toLowerCase();
                                        return sourceStr.endsWith(".svg") || sourceStr.endsWith(".svgz");
                                    }

                                    Layout.minimumWidth: hasIcon ? popup.iconSize : 0
                                    Layout.preferredWidth: hasIcon ? popup.iconSize : 0
                                    Layout.maximumWidth: hasIcon ? popup.iconSize : 0
                                    Layout.minimumHeight: hasIcon ? popup.iconSize : 0
                                    Layout.preferredHeight: hasIcon ? popup.iconSize : 0
                                    Layout.maximumHeight: hasIcon ? popup.iconSize : 0
                                    visible: hasIcon

                                    Image {
                                        id: subIconImage
                                        anchors.fill: parent
                                        visible: subIconItemContainer.hasIcon
                                        source: itemIcon
                                        fillMode: Image.PreserveAspectFit
                                        opacity: !subMenuItem.isEnabled ? 0.5 : 1
                                    }

                                    MultiEffect {
                                        anchors.fill: subIconImage
                                        source: subIconImage
                                        colorization: popup.iconEffectColorization
                                        colorizationColor: popup.iconEffectColor
                                        brightness: popup.iconEffectBrightness
                                        visible: subIconItemContainer.hasIcon && subMenuItem.isEnabled && subIconItemContainer.isSvgSource
                                        enabled: subIconItemContainer.isSvgSource
                                    }
                                }

                                Text {
                                    text: popup.richTextWithUnderline(
                                        popup.mnemonicInfo(itemData.text || "").display,
                                        popup.mnemonicInfo(itemData.text || "").index)
                                    textFormat: Text.RichText
                                    color: !subMenuItem.isEnabled
                                        ? popup.textDisabledColor
                                        : (subPopup.highlightedIndex === index
                                            ? popup.menuItemHoverTextColor
                                            : popup.menuItemTextColor)
                                    font.pixelSize: 13
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: itemData.shortcut || ""
                                    color: !subMenuItem.isEnabled ? popup.textDisabledColor : popup.menuItemTextColor
                                    font.pixelSize: 12
                                    visible: !hasSub
                                }

                                Text {
                                    text: "▶"
                                    color: !subMenuItem.isEnabled
                                        ? popup.textDisabledColor
                                        : (subPopup.highlightedIndex === index
                                            ? popup.menuItemHoverTextColor
                                            : popup.subMenuArrowColor)
                                    font.pixelSize: popup.subMenuArrowSize
                                    visible: hasSub
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: subMenuItem.isEnabled
                                cursorShape: Qt.PointingHandCursor

                                property var hoverTimer: null
                                property var closeTimer: null

                                onEntered: {
                                    popup.cancelAllCloseTimers();
                                    subPopup.highlightedIndex = index;

                                    var currentPath = subPopup.subPath.concat([index]);
                                    popup.closeSubMenusExceptPath(currentPath);

                                    if (hasSub) {
                                        if (hoverTimer) { hoverTimer.stop(); hoverTimer.destroy(); }
                                        hoverTimer = timerComponent.createObject(this, {
                                            "running": true,
                                            "interval": popup.subMenuOpenDelay,
                                            "onTimeout": function () {
                                                popup.openSubMenu(subPopup, index);
                                                if (hoverTimer) { hoverTimer.destroy(); hoverTimer = null; }
                                            }
                                        });
                                    }
                                }

                                onExited: {
                                    if (hoverTimer) {
                                        hoverTimer.stop();
                                        hoverTimer.destroy();
                                        hoverTimer = null;
                                    }

                                    if (hasSub) {
                                        var currentPath = subPopup.subPath.concat([index]);
                                        if (popup.isSubMenuOpen(currentPath)) {
                                            closeTimer = timerComponent.createObject(this, {
                                                "running": true,
                                                "interval": popup.subMenuCloseDelay,
                                                "onTimeout": function () {
                                                    popup.closeSubMenuByPath(currentPath);
                                                    popup.unregisterCloseTimer(closeTimer);
                                                    if (closeTimer) { closeTimer.destroy(); closeTimer = null; }
                                                }
                                            });
                                            popup.registerCloseTimer(closeTimer);
                                        }
                                    }
                                }

                                onClicked: {
                                    if (hasSub) {
                                        var currentPath = subPopup.subPath.concat([index]);
                                        if (popup.isSubMenuOpen(currentPath)) {
                                            popup.closeSubMenuByPath(currentPath);
                                        } else {
                                            popup.openSubMenu(subPopup, index);
                                        }
                                    } else {
                                        popup.triggerMenuItemFromSub(subPopup, index);
                                    }
                                }
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: 1
                            visible: isSeparator
                            color: popup.menuSeparatorColor
                        }
                    }
                }
            }
        }
    }

    // ============ 计时器组件 ============

    Component {
        id: timerComponent
        Timer {
            property var onTimeout: null
            onTriggered: {
                if (onTimeout) onTimeout();
            }
        }
    }

    // ============ 从子菜单触发项 ============

    /**
     * 由子菜单 Popup 调用，处理其菜单项的触发。
     * 需要将子菜单的路径和数据转发到根级 popup 的信号。
     */
    function triggerMenuItemFromSub(subPopupInstance, itemIndex) {
        var items = subPopupInstance.menuItems;
        var item = items[itemIndex];
        if (!item || item.separator || item.enabled === false) return;

        var fullPath = subPopupInstance.subPath.concat([itemIndex]);

        if (item.checkable === true) {
            var newChecked = !item.checked;
            item.checked = newChecked;
            menuCheckedChanged(fullPath, item, newChecked);
        }

        if (item.onTriggered && typeof item.onTriggered === 'function') {
            item.onTriggered();
        }

        menuTriggered(fullPath, item);

        // 触发后关闭所有菜单
        if (item.checkable !== true) {
            closeAllMenus();
        }
    }
}
