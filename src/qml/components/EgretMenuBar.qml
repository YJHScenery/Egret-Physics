/**
 * @file        EgretMenuBar.qml
 * @brief       菜单栏组件文件
 * @details     定义应用程序的菜单栏组件，包含文件、编辑、视图等菜单项。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-16
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Effects

Rectangle {
    id: root

    property var menus: []

    property int barHeight: 34
    property int itemPadding: 10
    property int itemSpacing: 6
    property int itemRadius: 8

    property int menuMinWidth: 180
    property int menuItemHeight: 28
    property int menuPadding: 8
    property int menuRadius: 10

    property color barBackground: "#0E213F"
    property color barBorderColor: "#2D5178"

    property color itemHoverBackground: "#1B3D63"
    property color itemActiveBackground: "#234B78"
    property color textColor: "#D3E6FF"
    property color textHoverColor: "#FFFFFF"
    property color textDisabledColor: "#7E98B6"

    property color menuBackground: "#0E233F"
    property color menuBorderColor: "#2D5178"
    property color menuItemHoverBackground: "#1B3D63"
    property color menuItemPressedBackground: "#25507E"
    property color menuItemTextColor: "#D3E6FF"
    property color menuItemHoverTextColor: "#FFFFFF"
    property color menuSeparatorColor: "#2C4E74"

    property color iconEffectColor: "#DDDDDD"
    property double iconEffectColorization: 1.0
    property double iconEffectBrightness: 0.75

    property int iconSize: 16
    property int iconSpacing: 8
    property int checkMarkWidth: 20

    property bool forceCheckMarkVisible: false
    property bool forceIconVisible: false

    property int subMenuOpenDelay: 300
    property int subMenuCloseDelay: 100
    property int subMenuMaxVisibleItems: 15
    property int subMenuScrollHeight: 420

    property color subMenuBackground: root.menuBackground
    property color subMenuBorderColor: root.menuBorderColor
    property int subMenuBorderWidth: 1
    property int subMenuRadius: root.menuRadius
    property int subMenuItemHeight: root.menuItemHeight
    property color subMenuItemHoverBackground: root.menuItemHoverBackground
    property color subMenuItemPressedBackground: root.menuItemPressedBackground
    property color subMenuItemTextColor: root.menuItemTextColor
    property color subMenuItemHoverTextColor: root.menuItemHoverTextColor
    property color subMenuSeparatorColor: root.menuSeparatorColor
    property int subMenuPadding: root.menuPadding
    property color subMenuArrowColor: root.menuItemTextColor
    property int subMenuArrowSize: 12

    signal menuTriggered(int menuIndex, int itemIndex, string itemId)
    signal checkedChanged(int menuIndex, int itemIndex, bool checked)
    signal submenuAboutToOpen(var menuIndex, var path)
    signal submenuOpened(var menuIndex, var path)
    signal submenuClosed(var menuIndex, var path)

    property int openIndex: -1
    property bool mnemonicsVisible: false
    property var buttonRefs: []
    property var shortcutItems: []
    property var menuMnemonicItems: []

    property var subMenuPath: []
    property var subMenuPopups: []
    property var activeSubMenuIndex: []
    property var closeTimers: []

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

    height: barHeight
    color: barBackground
    border.width: 1
    border.color: barBorderColor
    radius: 8

    focus: true

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Alt && !event.isAutoRepeat) {
            mnemonicsVisible = true;
            event.accepted = true;
            return;
        }
        if (event.modifiers & Qt.AltModifier) {
            var keyText = event.text.toLowerCase();
            if (keyText.length === 1) {
                var targetIndex = findMenuByMnemonic(keyText);
                if (targetIndex >= 0) {
                    openMenu(targetIndex);
                    event.accepted = true;
                }
            }
        }
    }

    Keys.onReleased: function (event) {
        if (event.key === Qt.Key_Alt && !event.isAutoRepeat) {
            if (openIndex === -1 && activeSubMenuIndex.length === 0) {
                mnemonicsVisible = false;
            }
            event.accepted = true;
        }
    }

    Component.onCompleted: rebuildShortcuts()
    onMenusChanged: rebuildShortcuts()

    function escapeHtml(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

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

    function richTextWithUnderline(display, index) {
        if (!mnemonicsVisible || index < 0 || index >= display.length) {
            return escapeHtml(display);
        }
        var before = escapeHtml(display.slice(0, index));
        var middle = escapeHtml(display.slice(index, index + 1));
        var after = escapeHtml(display.slice(index + 1));
        return before + "<u>" + middle + "</u>" + after;
    }

    function findMenuByMnemonic(keyText) {
        for (var i = 0; i < menus.length; i++) {
            var info = mnemonicInfo(menus[i].title || "");
            if (info.mnemonic === keyText) {
                return i;
            }
        }
        return -1;
    }

    function registerButton(index, item) {
        buttonRefs[index] = item;
    }

    function rebuildShortcuts() {
        var flat = [];
        var mnemonicShortcuts = [];
        for (var i = 0; i < menus.length; i++) {
            var menuInfo = mnemonicInfo(menus[i].title || "");
            if (menuInfo.mnemonic) {
                mnemonicShortcuts.push({
                    menuIndex: i,
                    sequence: "Alt+" + menuInfo.mnemonic.toUpperCase()
                });
            }
            collectShortcutItems(menus[i].items || [], i, flat);
        }
        shortcutItems = flat;
        menuMnemonicItems = mnemonicShortcuts;
    }

    function collectShortcutItems(items, menuIndex, flat) {
        for (var j = 0; j < items.length; j++) {
            var item = items[j];
            if (!item || item.separator || !item.shortcut) {
                continue;
            }
            flat.push({
                menuIndex: menuIndex,
                itemIndex: j,
                shortcut: item.shortcut,
                enabled: item.enabled !== false
            });
            if (item.submenu && item.submenu.length > 0) {
                collectShortcutItems(item.submenu, menuIndex, flat);
            }
        }
    }

    function openMenu(index) {
        if (index < 0 || index >= menus.length) {
            return;
        }
        closeAllSubMenus();
        openIndex = index;
        mnemonicsVisible = true;
        menuPopup.menuIndex = index;
        menuPopup.menuItems = menus[index].items || [];
        menuPopup.subMenuPath = [index];
        menuPopup.highlightedIndex = menuPopup.firstEnabledIndex();
        menuPopup.popupWidth = Math.max(root.menuMinWidth, root.calculateSubMenuWidth(menuPopup.menuItems));
        menuPopup.open();

        var button = buttonRefs[index];
        if (button) {
            var point = button.mapToItem(root, 0, button.height);
            menuPopup.x = point.x;
            menuPopup.y = point.y;
        }
    }

    function closeMenu() {
        closeAllSubMenus();
        menuPopup.close();
        openIndex = -1;
        mnemonicsVisible = false;
        activeSubMenuIndex = [];
    }

    function closeAllSubMenus() {
        for (var i = subMenuPopups.length - 1; i >= 0; i--) {
            if (subMenuPopups[i] && subMenuPopups[i].visible) {
                var popup = subMenuPopups[i];
                var path = popup.subMenuPath;
                submenuClosed(popup.menuIndex, path);
                popup.close();
            }
        }
        subMenuPopups = [];
        activeSubMenuIndex = [];
    }

    function triggerMenuItem(menuIndex, itemIndex, path) {
        // console.log("Triggered: ")
        // console.log(menuIndex)
        // console.log(itemIndex)
        // console.log(path)

        var menu;
        var item;
        if (path && path.length > 1) {
            menu = menus[path[0]];
            var items = menu.items;
            for (var p = 1; p < path.length; p++) {
                var idx = path[p];
                if (items[idx] && items[idx].submenu) {
                    items = items[idx].submenu;
                }
            }
            menu = {
                items: items
            };
            item = items[itemIndex];
        } else {
            menu = menus[menuIndex];
            if (!menu)
                return;
            item = menu.items[itemIndex];
        }

        if (!item || item.separator || item.enabled === false) {
            return;
        }

        if (item.checkable === true) {
            var newChecked = !item.checked;
            item.checked = newChecked;

            if (item.actionGroup) {
                var allItems = menu.items;
                for (var j = 0; j < allItems.length; j++) {
                    var otherItem = allItems[j];
                    if (otherItem && otherItem !== item && otherItem.actionGroup === item.actionGroup) {
                        otherItem.checked = false;
                    }
                }
            }

            checkedChanged(menuIndex, itemIndex, newChecked);
        }

        if (item.onTriggered) {
            item.onTriggered();
        }
        menuTriggered(menuIndex, itemIndex, item.id || "");
        closeMenu();
    }

    function hasSubMenu(item) {
        return !!(item && item.submenu && item.submenu.length > 0);
    }

    function cancelAllCloseTimers() {
        for (var i = closeTimers.length - 1; i >= 0; i--) {
            var timer = closeTimers[i];
            if (timer && typeof timer.stop === 'function') {
                try {
                    timer.stop();
                    timer.destroy();
                } catch (e) {}
            }
        }
        closeTimers = [];
    }

    function registerCloseTimer(timer) {
        closeTimers.push(timer);
    }

    function unregisterCloseTimer(timer) {
        var idx = closeTimers.indexOf(timer);
        if (idx >= 0) {
            closeTimers.splice(idx, 1);
        }
    }

    function openSubMenu(parentPopup, parentItem, itemIndex, items, menuIndex, path) {
        if (!hasSubMenu(items[itemIndex])) {
            return;
        }

        cancelAllCloseTimers();
        submenuAboutToOpen(menuIndex, path);

        var subMenuItems = items[itemIndex].submenu;
        var subMenuPath = path.concat([itemIndex]);

        var subMenuPopup = subMenuPool.acquire();
        if (!subMenuPopup) {
            subMenuPopup = subMenuComponent.createObject(root);
        }

        subMenuPopup.menuIndex = menuIndex;
        subMenuPopup.menuItems = subMenuItems;
        subMenuPopup.subMenuPath = subMenuPath;
        subMenuPopup.parentMenu = parentPopup;
        subMenuPopup.parentItemIndex = itemIndex;
        subMenuPopup.highlightedIndex = subMenuPopup.firstEnabledIndex();

        var parentX = parentPopup.x + parentPopup.width;
        var parentY = parentPopup.y + itemIndex * root.subMenuItemHeight;

        var screenWidth = root.width;
        var screenHeight = root.height;

        var subMenuWidth = Math.max(root.menuMinWidth, calculateSubMenuWidth(subMenuItems));
        var subMenuHeight = Math.min(subMenuItems.length * root.subMenuItemHeight, root.subMenuScrollHeight);

        var openRight = true;
        if (parentX + subMenuWidth > screenWidth) {
            openRight = false;
        }

        var openUp = false;
        var adjustedY = parentY;
        if (parentY + subMenuHeight > screenHeight && parentY > screenHeight / 2) {
            openUp = true;
            adjustedY = parentY + root.subMenuItemHeight - subMenuHeight;
        }

        subMenuPopup.popupWidth = subMenuWidth;
        subMenuPopup.subMenuHeight = subMenuHeight;

        if (openRight) {
            subMenuPopup.x = parentX;
        } else {
            subMenuPopup.x = parentPopup.x - subMenuWidth;
        }

        if (openUp) {
            subMenuPopup.y = Math.max(0, adjustedY);
        } else {
            subMenuPopup.y = Math.max(0, parentY);
        }

        subMenuPopups.push(subMenuPopup);
        activeSubMenuIndex = subMenuPath;

        subMenuPopup.open();
        submenuOpened(menuIndex, subMenuPath);
    }

    function calculateSubMenuWidth(items) {
        var maxWidth = root.menuMinWidth;
        var hasAny = false;
        for (var i = 0; i < items.length; i++) {
            var item = items[i];
            if (!item || item.separator) {
                continue;
            }
            hasAny = true;
            var labelInfo = root.mnemonicInfo(item.text || "");
            menuLabelMetrics.text = labelInfo.display;
            var labelWidth = menuLabelMetrics.width;

            var shortcutWidth = 0;
            if (item.shortcut) {
                menuShortcutMetrics.text = item.shortcut;
                shortcutWidth = menuShortcutMetrics.width + 18;
            }

            var checkWidth = (item.checkable === true || root.forceCheckMarkVisible) ? root.checkMarkWidth : 0;
            var iconWidth = (item.icon || root.forceIconVisible) ? root.iconSize : 0;
            var arrowWidth = (item.submenu && item.submenu.length > 0) ? root.subMenuArrowSize + 8 : 0;

            var contentWidth = checkWidth + iconWidth + labelWidth + shortcutWidth + arrowWidth;
            var spacingSlots = 0;
            if (checkWidth > 0) {
                spacingSlots += 1;
            }
            if (iconWidth > 0) {
                spacingSlots += 1;
            }
            if (shortcutWidth > 0) {
                spacingSlots += 1;
            }
            if (arrowWidth > 0) {
                spacingSlots += 1;
            }
            contentWidth += spacingSlots * root.iconSpacing;

            var totalWidth = contentWidth + root.subMenuPadding * 2 + 20;
            if (totalWidth > maxWidth) {
                maxWidth = totalWidth;
            }
        }
        return hasAny ? maxWidth : root.menuMinWidth;
    }

    function closeSubMenu(popup) {
        var idx = subMenuPopups.indexOf(popup);
        if (idx >= 0) {
            subMenuPopups.splice(idx, 1);
        }
        if (popup.subMenuPath && popup.subMenuPath.length > 0) {
            submenuClosed(popup.menuIndex, popup.subMenuPath);
        }
        popup.close();
        activeSubMenuIndex = popup.parentMenu ? popup.parentMenu.subMenuPath : [];
    }

    function closeSubMenuByPath(path) {
        for (var i = subMenuPopups.length - 1; i >= 0; i--) {
            var popup = subMenuPopups[i];
            if (popup.subMenuPath && pathMatches(popup.subMenuPath, path)) {
                closeSubMenu(popup);
            }
        }
    }

    function closeSubMenusExceptPath(keepPath) {
        for (var i = subMenuPopups.length - 1; i >= 0; i--) {
            var popup = subMenuPopups[i];
            if (popup.subMenuPath && !isParentChildPath(popup.subMenuPath, keepPath)) {
                closeSubMenu(popup);
            }
        }
    }

    function isParentChildPath(path1, path2) {
        if (!path1 || !path2)
            return false;
        var minLen = Math.min(path1.length, path2.length);
        for (var i = 0; i < minLen; i++) {
            if (path1[i] !== path2[i])
                return false;
        }
        return true;
    }

    function pathMatches(path1, path2) {
        if (!path1 || !path2)
            return false;
        if (path1.length !== path2.length)
            return false;
        for (var i = 0; i < path1.length; i++) {
            if (path1[i] !== path2[i])
                return false;
        }
        return true;
    }

    Component {
        id: subMenuComponent

        Popup {
            id: subMenuPopup

            property int menuIndex: -1
            property var menuItems: []
            property var subMenuPath: []
            property int highlightedIndex: -1
            property var parentMenu: null
            property int parentItemIndex: -1
            property int subMenuHeight: 0
            property int popupWidth: root.menuMinWidth

            modal: false
            focus: true
            padding: root.subMenuPadding
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

            background: Rectangle {
                color: root.subMenuBackground
                border.width: root.subMenuBorderWidth
                border.color: root.subMenuBorderColor
                radius: root.subMenuRadius
            }

            onOpened: {
                highlightedIndex = firstEnabledIndex();
                forceActiveFocus();
            }

            onClosed: {
                if (parentMenu) {
                    parentMenu.forceActiveFocus();
                }
            }

            function firstEnabledIndex() {
                for (var i = 0; i < menuItems.length; i++) {
                    var item = menuItems[i];
                    if (item && !item.separator && item.enabled !== false) {
                        return i;
                    }
                }
                return -1;
            }

            function nextEnabledIndex(startIndex, direction) {
                if (menuItems.length === 0) {
                    return -1;
                }
                var index = startIndex;
                for (var attempt = 0; attempt < menuItems.length; attempt++) {
                    index += direction;
                    if (index < 0) {
                        index = menuItems.length - 1;
                    } else if (index >= menuItems.length) {
                        index = 0;
                    }
                    var item = menuItems[index];
                    if (item && !item.separator && item.enabled !== false) {
                        return i;
                    }
                }
                return startIndex;
            }

            contentItem: Item {
                id: subMenuRoot
                implicitWidth: subMenuPopup.popupWidth
                implicitHeight: Math.min(subMenuList.contentHeight, root.subMenuScrollHeight)
                focus: true

                Keys.onPressed: function (event) {
                    if (event.key === Qt.Key_Escape) {
                        closeSubMenu(subMenuPopup);
                        event.accepted = true;
                        return;
                    }
                    if (event.key === Qt.Key_Down) {
                        highlightedIndex = nextEnabledIndex(highlightedIndex, 1);
                        event.accepted = true;
                        return;
                    }
                    if (event.key === Qt.Key_Up) {
                        highlightedIndex = nextEnabledIndex(highlightedIndex, -1);
                        event.accepted = true;
                        return;
                    }
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        if (highlightedIndex >= 0) {
                            var item = menuItems[highlightedIndex];
                            if (hasSubMenu(item)) {
                                root.openSubMenu(subMenuPopup, subMenuPopup, highlightedIndex, menuItems, menuIndex, subMenuPath);
                            } else {
                                root.triggerMenuItem(menuIndex, highlightedIndex, subMenuPath);
                            }
                        }
                        event.accepted = true;
                        return;
                    }
                    if (event.key === Qt.Key_Right) {
                        var currentItem = menuItems[highlightedIndex];
                        if (hasSubMenu(currentItem)) {
                            root.openSubMenu(subMenuPopup, subMenuPopup, highlightedIndex, menuItems, menuIndex, subMenuPath);
                            if (subMenuPopups.length > 0) {
                                var newestSubMenu = subMenuPopups[subMenuPopups.length - 1];
                                newestSubMenu.forceActiveFocus();
                            }
                        }
                        event.accepted = true;
                        return;
                    }
                    if (event.key === Qt.Key_Left) {
                        if (parentMenu) {
                            closeSubMenu(subMenuPopup);
                            if (parentMenu && parentMenu.parentMenu) {
                                parentMenu.forceActiveFocus();
                            }
                        }
                        event.accepted = true;
                        return;
                    }
                    if (event.text && event.text.length === 1) {
                        var keyText = event.text.toLowerCase();
                        for (var i = 0; i < menuItems.length; i++) {
                            var info = root.mnemonicInfo(menuItems[i].text || "");
                            if (info.mnemonic === keyText) {
                                var targetItem = menuItems[i];
                                if (hasSubMenu(targetItem)) {
                                    highlightedIndex = i;
                                    root.openSubMenu(subMenuPopup, subMenuPopup, i, menuItems, menuIndex, subMenuPath);
                                } else {
                                    root.triggerMenuItem(menuIndex, i, subMenuPath);
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
                    width: subMenuRoot.implicitWidth
                    implicitHeight: Math.min(contentHeight, root.subMenuScrollHeight)
                    model: menuItems
                    interactive: contentHeight > root.subMenuScrollHeight
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
                        property bool hasSubMenu: root.hasSubMenu(itemData)

                        implicitWidth: subMenuList.width
                        width: subMenuList.width
                        height: isSeparator ? 8 : root.subMenuItemHeight

                        Rectangle {
                            anchors.fill: parent
                            visible: !isSeparator
                            radius: 6
                            color: subMenuPopup.highlightedIndex === index ? root.subMenuItemHoverBackground : "transparent"

                            RowLayout {
                                id: row
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: root.iconSpacing

                                Item {
                                    Layout.minimumWidth: isCheckable || root.forceCheckMarkVisible ? root.checkMarkWidth : 0
                                    Layout.preferredWidth: isCheckable || root.forceCheckMarkVisible ? root.checkMarkWidth : 0
                                    Layout.maximumWidth: isCheckable || root.forceCheckMarkVisible ? root.checkMarkWidth : 0
                                    Layout.minimumHeight: isCheckable || root.forceCheckMarkVisible ? root.iconSize : 0
                                    Layout.preferredHeight: isCheckable || root.forceCheckMarkVisible ? root.iconSize : 0
                                    Layout.maximumHeight: isCheckable || root.forceCheckMarkVisible ? root.iconSize : 0
                                    visible: isCheckable || root.forceCheckMarkVisible

                                    Text {
                                        anchors.fill: parent
                                        anchors.leftMargin: 2
                                        text: isChecked ? "✔" : ""
                                        color: !subMenuItem.isEnabled ? root.textDisabledColor : (subMenuPopup.highlightedIndex === index ? root.subMenuItemHoverTextColor : root.subMenuItemTextColor)
                                        font.pixelSize: 14
                                        horizontalAlignment: Text.AlignLeft
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                Item {
                                    id: subMenuIconItem
                                    property bool hasIcon: itemIcon !== "" || root.forceIconVisible

                                    property bool isSvgSource: {
                                        if (!itemIcon)
                                            return false;
                                        var sourceStr = itemIcon.toString().toLowerCase();
                                        return sourceStr.endsWith(".svg") || sourceStr.endsWith(".svgz");
                                    }

                                    Layout.minimumWidth: hasIcon ? root.iconSize : 0
                                    Layout.preferredWidth: hasIcon ? root.iconSize : 0
                                    Layout.maximumWidth: hasIcon ? root.iconSize : 0
                                    Layout.minimumHeight: hasIcon ? root.iconSize : 0
                                    Layout.preferredHeight: hasIcon ? root.iconSize : 0
                                    Layout.maximumHeight: hasIcon ? root.iconSize : 0
                                    visible: hasIcon

                                    Image {
                                        id: subMenuIconImage
                                        anchors.fill: parent
                                        visible: subMenuIconItem.hasIcon
                                        source: itemIcon
                                        fillMode: Image.PreserveAspectFit
                                        opacity: !subMenuItem.isEnabled ? 0.5 : 1
                                    }

                                    MultiEffect {
                                        anchors.fill: subMenuIconImage
                                        source: subMenuIconImage
                                        colorization: root.iconEffectColorization
                                        colorizationColor: root.iconEffectColor
                                        brightness: root.iconEffectBrightness
                                        visible: subMenuIconItem.hasIcon && subMenuItem.isEnabled && subMenuIconItem.isSvgSource
                                        enabled: subMenuIconItem.isSvgSource
                                    }
                                }

                                Text {
                                    text: root.richTextWithUnderline(root.mnemonicInfo(itemData.text || "").display, root.mnemonicInfo(itemData.text || "").index)
                                    textFormat: Text.RichText
                                    color: !subMenuItem.isEnabled ? root.textDisabledColor : (subMenuPopup.highlightedIndex === index ? root.subMenuItemHoverTextColor : root.subMenuItemTextColor)
                                    font.pixelSize: 13
                                }

                                Item {
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: itemData.shortcut || ""
                                    color: !subMenuItem.isEnabled ? root.textDisabledColor : root.subMenuItemTextColor
                                    font.pixelSize: 12
                                    visible: !hasSubMenu
                                }

                                Text {
                                    text: "▶"
                                    color: !subMenuItem.isEnabled ? root.textDisabledColor : (subMenuPopup.highlightedIndex === index ? root.subMenuItemHoverTextColor : root.subMenuArrowColor)
                                    font.pixelSize: root.subMenuArrowSize
                                    visible: hasSubMenu
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            MouseArea {
                                id: subMenuMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: subMenuItem.isEnabled

                                property var hoverTimer: null
                                property var closeTimer: null

                                onEntered: {
                                    root.cancelAllCloseTimers();
                                    subMenuPopup.highlightedIndex = index;

                                    var currentPath = subMenuPath.concat([index]);
                                    root.closeSubMenusExceptPath(currentPath);

                                    if (hasSubMenu) {
                                        if (hoverTimer) {
                                            hoverTimer.stop();
                                        }
                                        hoverTimer = timerComponent.createObject(subMenuMouseArea, {
                                            running: true,
                                            interval: root.subMenuOpenDelay,
                                            onTimeout: function () {
                                                root.openSubMenu(subMenuPopup, subMenuItem, index, menuItems, menuIndex, subMenuPath);
                                                if (hoverTimer) {
                                                    hoverTimer.destroy();
                                                    hoverTimer = null;
                                                }
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

                                    if (hasSubMenu && isSubMenuOpen(subMenuPath.concat([index]))) {
                                        closeTimer = timerComponent.createObject(subMenuMouseArea, {
                                            running: true,
                                            interval: root.subMenuCloseDelay,
                                            onTimeout: function () {
                                                root.closeSubMenuByPath(subMenuPath.concat([index]));
                                                root.unregisterCloseTimer(closeTimer);
                                                if (closeTimer) {
                                                    closeTimer.destroy();
                                                    closeTimer = null;
                                                }
                                            }
                                        });
                                        root.registerCloseTimer(closeTimer);
                                    }
                                }

                                onClicked: {
                                    if (hasSubMenu) {
                                        var currentPath = subMenuPath.concat([index]);
                                        if (isSubMenuOpen(currentPath)) {
                                            root.closeSubMenuByPath(currentPath);
                                        } else {
                                            root.openSubMenu(subMenuPopup, subMenuItem, index, menuItems, menuIndex, subMenuPath);
                                        }
                                    } else {
                                        root.triggerMenuItem(menuIndex, index, subMenuPath);
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
                            color: root.subMenuSeparatorColor
                        }
                    }
                }
            }
        }
    }

    function isSubMenuOpen(path) {
        for (var i = 0; i < subMenuPopups.length; i++) {
            if (pathMatches(subMenuPopups[i].subMenuPath, path)) {
                return true;
            }
        }
        return false;
    }

    Component {
        id: timerComponent
        Timer {
            property var onTimeout: null
            onTriggered: {
                if (onTimeout) {
                    onTimeout();
                }
            }
        }
    }

    QtObject {
        id: subMenuPool
        property var pool: []
        property var used: []

        function acquire() {
            if (pool.length > 0) {
                var popup = pool.pop();
                used.push(popup);
                return popup;
            }
            return null;
        }

        function release(popup) {
            var idx = used.indexOf(popup);
            if (idx >= 0) {
                used.splice(idx, 1);
                popup.close();
                popup.menuItems = [];
                popup.subMenuPath = [];
                popup.parentMenu = null;
                popup.parentItemIndex = -1;
                pool.push(popup);
            }
        }

        function releaseAll() {
            while (used.length > 0) {
                release(used[0]);
            }
        }
    }

    RowLayout {
        id: barRow
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: itemSpacing

        Repeater {
            id: menuRepeater
            model: menus

            delegate: Rectangle {
                id: menuButton

                property bool hovered: false
                property var info: root.mnemonicInfo(modelData.title || "")

                Layout.alignment: Qt.AlignLeft
                radius: itemRadius
                height: parent.height
                implicitWidth: label.implicitWidth + itemPadding * 2
                color: root.openIndex === index ? root.itemActiveBackground : (hovered ? root.itemHoverBackground : "transparent")

                Component.onCompleted: root.registerButton(index, menuButton)

                Text {
                    id: label
                    anchors.left: parent.left
                    anchors.leftMargin: itemPadding
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.richTextWithUnderline(info.display, info.index)
                    textFormat: Text.RichText
                    color: root.openIndex === index || hovered ? root.textHoverColor : root.textColor
                    font.pixelSize: 13
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        menuButton.hovered = true;
                        if (root.openIndex !== -1 && root.openIndex !== index) {
                            root.openMenu(index);
                        }
                    }
                    onExited: menuButton.hovered = false
                    onClicked: {
                        if (root.openIndex === index && menuPopup.visible) {
                            root.closeMenu();
                        } else {
                            root.openMenu(index);
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }
    }

    Popup {
        id: menuPopup

        property int menuIndex: -1
        property var menuItems: []
        property var subMenuPath: []
        property int highlightedIndex: -1
        property int popupWidth: root.menuMinWidth

        modal: false
        focus: true
        padding: menuPadding
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: root.menuBackground
            border.width: 1
            border.color: root.menuBorderColor
            radius: root.menuRadius
        }

        onOpened: {
            highlightedIndex = firstEnabledIndex();
            forceActiveFocus();
        }

        onClosed: {
            if (root.openIndex !== -1) {
                root.openIndex = -1;
            }
            root.mnemonicsVisible = false;
            root.closeAllSubMenus();
        }

        function firstEnabledIndex() {
            for (var i = 0; i < menuItems.length; i++) {
                var item = menuItems[i];
                if (item && !item.separator && item.enabled !== false) {
                    return i;
                }
            }
            return -1;
        }

        function nextEnabledIndex(startIndex, direction) {
            if (menuItems.length === 0) {
                return -1;
            }
            var index = startIndex;
            for (var attempt = 0; attempt < menuItems.length; attempt++) {
                index += direction;
                if (index < 0) {
                    index = menuItems.length - 1;
                } else if (index >= menuItems.length) {
                    index = 0;
                }
                var item = menuItems[index];
                if (item && !item.separator && item.enabled !== false) {
                    return index;
                }
            }
            return startIndex;
        }

        contentItem: Item {
            id: popupRoot
            implicitWidth: menuPopup.popupWidth
            implicitHeight: Math.min(menuList.contentHeight, root.subMenuScrollHeight)
            focus: true

            Keys.onPressed: function (event) {
                if (event.key === Qt.Key_Escape) {
                    root.closeMenu();
                    event.accepted = true;
                    return;
                }
                if (event.key === Qt.Key_Down) {
                    highlightedIndex = nextEnabledIndex(highlightedIndex, 1);
                    event.accepted = true;
                    return;
                }
                if (event.key === Qt.Key_Up) {
                    highlightedIndex = nextEnabledIndex(highlightedIndex, -1);
                    event.accepted = true;
                    return;
                }
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    if (highlightedIndex >= 0) {
                        var item = menuPopup.menuItems[highlightedIndex];
                        if (root.hasSubMenu(item)) {
                            root.openSubMenu(menuPopup, menuPopup, highlightedIndex, menuPopup.menuItems, menuPopup.menuIndex, [menuPopup.menuIndex]);
                            if (subMenuPopups.length > 0) {
                                var newestSubMenu = subMenuPopups[subMenuPopups.length - 1];
                                newestSubMenu.forceActiveFocus();
                            }
                        } else {
                            root.triggerMenuItem(menuIndex, highlightedIndex, [menuIndex]);
                        }
                    }
                    event.accepted = true;
                    return;
                }
                if (event.key === Qt.Key_Left) {
                    root.openMenu((root.openIndex - 1 + root.menus.length) % root.menus.length);
                    event.accepted = true;
                    return;
                }
                if (event.key === Qt.Key_Right) {
                    var currentItem = menuPopup.menuItems[highlightedIndex];
                    if (root.hasSubMenu(currentItem)) {
                        root.openSubMenu(menuPopup, menuPopup, highlightedIndex, menuPopup.menuItems, menuPopup.menuIndex, [menuPopup.menuIndex]);
                        if (subMenuPopups.length > 0) {
                            var newestSubMenu = subMenuPopups[subMenuPopups.length - 1];
                            newestSubMenu.forceActiveFocus();
                        }
                    } else {
                        root.openMenu((root.openIndex + 1) % root.menus.length);
                    }
                    event.accepted = true;
                    return;
                }
                if (event.text && event.text.length === 1) {
                    var keyText = event.text.toLowerCase();
                    for (var i = 0; i < menuPopup.menuItems.length; i++) {
                        var info = root.mnemonicInfo(menuPopup.menuItems[i].text || "");
                        if (info.mnemonic === keyText) {
                            var targetItem = menuPopup.menuItems[i];
                            if (root.hasSubMenu(targetItem)) {
                                highlightedIndex = i;
                                root.openSubMenu(menuPopup, menuPopup, i, menuPopup.menuItems, menuPopup.menuIndex, [menuPopup.menuIndex]);
                                if (subMenuPopups.length > 0) {
                                    var newestSubMenu = subMenuPopups[subMenuPopups.length - 1];
                                    newestSubMenu.forceActiveFocus();
                                }
                            } else {
                                root.triggerMenuItem(menuPopup.menuIndex, i, [menuPopup.menuIndex]);
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
                width: popupRoot.implicitWidth
                implicitHeight: Math.min(contentHeight, root.subMenuScrollHeight)
                model: menuPopup.menuItems
                interactive: contentHeight > root.subMenuScrollHeight
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
                    property bool hasSubMenu: root.hasSubMenu(itemData)

                    implicitWidth: menuList.width
                    width: menuList.width
                    height: isSeparator ? 8 : root.menuItemHeight

                    Rectangle {
                        anchors.fill: parent
                        visible: !isSeparator
                        radius: 6
                        color: menuPopup.highlightedIndex === index ? root.menuItemHoverBackground : "transparent"

                        RowLayout {
                            id: row
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: root.iconSpacing

                            Item {
                                Layout.minimumWidth: isCheckable || root.forceCheckMarkVisible ? root.checkMarkWidth : 0
                                Layout.preferredWidth: isCheckable || root.forceCheckMarkVisible ? root.checkMarkWidth : 0
                                Layout.maximumWidth: isCheckable || root.forceCheckMarkVisible ? root.checkMarkWidth : 0
                                Layout.minimumHeight: isCheckable || root.forceCheckMarkVisible ? root.iconSize : 0
                                Layout.preferredHeight: isCheckable || root.forceCheckMarkVisible ? root.iconSize : 0
                                Layout.maximumHeight: isCheckable || root.forceCheckMarkVisible ? root.iconSize : 0
                                visible: isCheckable || root.forceCheckMarkVisible

                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 2
                                    text: isChecked ? "✔" : ""
                                    color: !menuItem.isEnabled ? root.textDisabledColor : (menuPopup.highlightedIndex === index ? root.menuItemHoverTextColor : root.menuItemTextColor)
                                    font.pixelSize: 14
                                    horizontalAlignment: Text.AlignLeft
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Item {
                                id: iconItem
                                property bool hasIcon: itemIcon !== "" || root.forceIconVisible

                                property bool isSvgSource: {
                                    if (!itemIcon)
                                        return false;
                                    var sourceStr = itemIcon.toString().toLowerCase();
                                    return sourceStr.endsWith(".svg") || sourceStr.endsWith(".svgz");
                                }

                                Layout.minimumWidth: hasIcon ? root.iconSize : 0
                                Layout.preferredWidth: hasIcon ? root.iconSize : 0
                                Layout.maximumWidth: hasIcon ? root.iconSize : 0
                                Layout.minimumHeight: hasIcon ? root.iconSize : 0
                                Layout.preferredHeight: hasIcon ? root.iconSize : 0
                                Layout.maximumHeight: hasIcon ? root.iconSize : 0
                                visible: hasIcon

                                Image {
                                    id: iconImage
                                    anchors.fill: parent
                                    visible: iconItem.hasIcon
                                    source: itemIcon
                                    fillMode: Image.PreserveAspectFit
                                    opacity: !menuItem.isEnabled ? 0.5 : 1
                                }

                                MultiEffect {
                                    anchors.fill: iconImage
                                    source: iconImage
                                    colorization: root.iconEffectColorization
                                    colorizationColor: root.iconEffectColor
                                    brightness: root.iconEffectBrightness
                                    visible: iconItem.hasIcon && menuItem.isEnabled && iconItem.isSvgSource
                                    enabled: iconItem.isSvgSource
                                }
                            }

                            Text {
                                text: root.richTextWithUnderline(root.mnemonicInfo(itemData.text || "").display, root.mnemonicInfo(itemData.text || "").index)
                                textFormat: Text.RichText
                                color: !menuItem.isEnabled ? root.textDisabledColor : (menuPopup.highlightedIndex === index ? root.menuItemHoverTextColor : root.menuItemTextColor)
                                font.pixelSize: 13
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Text {
                                text: itemData.shortcut || ""
                                color: !menuItem.isEnabled ? root.textDisabledColor : root.menuItemTextColor
                                font.pixelSize: 12
                                visible: !hasSubMenu
                            }

                            Text {
                                text: "▶"
                                color: !menuItem.isEnabled ? root.textDisabledColor : (menuPopup.highlightedIndex === index ? root.menuItemHoverTextColor : root.subMenuArrowColor)
                                font.pixelSize: root.subMenuArrowSize
                                visible: hasSubMenu
                                horizontalAlignment: Text.AlignRight
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        MouseArea {
                            id: menuItemMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: menuItem.isEnabled

                            property var hoverTimer: null
                            property var closeTimer: null

                            onEntered: {
                                root.cancelAllCloseTimers();
                                menuPopup.highlightedIndex = index;

                                var currentPath = [menuPopup.menuIndex, index];
                                root.closeSubMenusExceptPath(currentPath);

                                if (hasSubMenu) {
                                    if (hoverTimer) {
                                        hoverTimer.stop();
                                        hoverTimer.destroy();
                                    }
                                    hoverTimer = timerComponent.createObject(menuItemMouseArea, {
                                        running: true,
                                        interval: root.subMenuOpenDelay,
                                        onTimeout: function () {
                                            root.openSubMenu(menuPopup, menuItem, index, menuPopup.menuItems, menuPopup.menuIndex, [menuPopup.menuIndex]);
                                            if (hoverTimer) {
                                                hoverTimer.destroy();
                                                hoverTimer = null;
                                            }
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

                                if (hasSubMenu) {
                                    var currentPath = [menuPopup.menuIndex, index];
                                    if (isSubMenuOpen(currentPath)) {
                                        closeTimer = timerComponent.createObject(menuItemMouseArea, {
                                            running: true,
                                            interval: root.subMenuCloseDelay,
                                            onTimeout: function () {
                                                root.closeSubMenuByPath(currentPath);
                                                root.unregisterCloseTimer(closeTimer);
                                                if (closeTimer) {
                                                    closeTimer.destroy();
                                                    closeTimer = null;
                                                }
                                            }
                                        });
                                        root.registerCloseTimer(closeTimer);
                                    }
                                }
                            }

                            onClicked: {
                                if (hasSubMenu) {
                                    var currentPath = [menuPopup.menuIndex, index];
                                    if (isSubMenuOpen(currentPath)) {
                                        root.closeSubMenuByPath(currentPath);
                                    } else {
                                        root.openSubMenu(menuPopup, menuItem, index, menuPopup.menuItems, menuPopup.menuIndex, [menuPopup.menuIndex]);
                                    }
                                } else {
                                    root.triggerMenuItem(menuPopup.menuIndex, index, [menuPopup.menuIndex]);
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
                        color: root.menuSeparatorColor
                    }
                }
            }
        }
    }

    Instantiator {
        model: root.shortcutItems

        delegate: Shortcut {
            sequence: modelData.shortcut
            enabled: modelData.enabled
            context: Qt.ApplicationShortcut
            onActivated: root.triggerMenuItem(modelData.menuIndex, modelData.itemIndex)
        }
    }

    Instantiator {
        model: root.menuMnemonicItems

        delegate: Shortcut {
            sequence: modelData.sequence
            context: Qt.ApplicationShortcut
            onActivated: root.openMenu(modelData.menuIndex)
        }
    }
}
