import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

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

    signal menuTriggered(int menuIndex, int itemIndex, string itemId)

    property int openIndex: -1
    property bool mnemonicsVisible: false
    property var buttonRefs: []
    property var shortcutItems: []
    property var menuMnemonicItems: []

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
            if (openIndex === -1) {
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

        return { display: display, mnemonic: mnemonic, index: index };
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
            var items = menus[i].items || [];
            for (var j = 0; j < items.length; j++) {
                var item = items[j];
                if (!item || item.separator || !item.shortcut) {
                    continue;
                }
                flat.push({
                    menuIndex: i,
                    itemIndex: j,
                    shortcut: item.shortcut,
                    enabled: item.enabled !== false
                });
            }
        }
        shortcutItems = flat;
        menuMnemonicItems = mnemonicShortcuts;
    }

    function openMenu(index) {
        if (index < 0 || index >= menus.length) {
            return;
        }
        openIndex = index;
        mnemonicsVisible = true;
        menuPopup.menuIndex = index;
        menuPopup.menuItems = menus[index].items || [];
        menuPopup.highlightedIndex = menuPopup.firstEnabledIndex();
        menuPopup.open();

        var button = buttonRefs[index];
        if (button) {
            var point = button.mapToItem(root, 0, button.height);
            menuPopup.x = point.x;
            menuPopup.y = point.y;
        }
    }

    function closeMenu() {
        menuPopup.close();
        openIndex = -1;
        mnemonicsVisible = false;
    }

    function triggerMenuItem(menuIndex, itemIndex) {
        var menu = menus[menuIndex];
        if (!menu) {
            return;
        }
        var item = menu.items[itemIndex];
        if (!item || item.separator || item.enabled === false) {
            return;
        }
        if (item.onTriggered) {
            item.onTriggered();
        }
        menuTriggered(menuIndex, itemIndex, item.id || "");
        closeMenu();
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
        property int highlightedIndex: -1

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
            implicitWidth: menuList.implicitWidth
            implicitHeight: menuList.implicitHeight
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
                        root.triggerMenuItem(menuIndex, highlightedIndex);
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
                    root.openMenu((root.openIndex + 1) % root.menus.length);
                    event.accepted = true;
                    return;
                }
                if (event.text && event.text.length === 1) {
                    var keyText = event.text.toLowerCase();
                    for (var i = 0; i < menuItems.length; i++) {
                        var info = root.mnemonicInfo(menuItems[i].text || "");
                        if (info.mnemonic === keyText) {
                            root.triggerMenuItem(menuIndex, i);
                            event.accepted = true;
                            return;
                        }
                    }
                }
            }

            ListView {
                id: menuList
                anchors.fill: parent
                width: implicitWidth
                implicitWidth: Math.max(root.menuMinWidth, contentItem.childrenRect.width)
                implicitHeight: contentHeight
                model: menuPopup.menuItems
                interactive: true
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                id: menuItem

                property var itemData: modelData
                property bool isSeparator: itemData && itemData.separator === true
                property bool isEnabled: itemData && itemData.enabled !== false

                implicitWidth: row.implicitWidth + 20
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
                        spacing: 10

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
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: menuItem.isEnabled
                        onEntered: menuPopup.highlightedIndex = index
                        onClicked: root.triggerMenuItem(menuPopup.menuIndex, index)
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
