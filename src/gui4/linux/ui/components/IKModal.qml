/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import kDrive.UI

// Reusable in-app modal surface. It deliberately owns presentation and focus only; feature state and actions stay in
// the dialog that supplies bodyData and footerData.
Popup {
    id: root

    required property string title
    property url iconSource
    property color iconColor: IKColors.textPrimary
    property bool escapeDismissible: false
    property bool actionsStacked: false
    property int actionColumns: 2
    property Item initialFocusItem: null
    property real scrimInset: 0
    property real scrimRadius: 0
    property alias bodyData: bodyColumn.data
    property alias footerData: footerLayout.data

    signal dismissRequested

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: parent ? Math.min(IKModalTokens.width, Math.max(0, parent.width - 2 * IKModalTokens.screenMargin))
                  : IKModalTokens.width
    implicitHeight: modalContent.implicitHeight + topPadding + bottomPadding
    padding: IKModalTokens.contentPadding
    popupType: Popup.Item
    modal: true
    dim: true
    focus: true
    closePolicy: Popup.NoAutoClose

    Shortcut {
        sequences: [ StandardKey.Cancel ]
        enabled: root.opened && root.escapeDismissible
        context: Qt.WindowShortcut
        onActivated: root.dismissRequested()
    }

    onOpened: Qt.callLater(function() {
        if (root.initialFocusItem) {
            root.initialFocusItem.forceActiveFocus()
        }
    })

    Overlay.modal: Item {
        Rectangle {
            x: root.scrimInset
            y: root.scrimInset
            width: Math.max(0, parent.width - 2 * root.scrimInset)
            height: Math.max(0, parent.height - 2 * root.scrimInset)
            radius: root.scrimRadius
            color: IKColors.modalScrim
        }

    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: IKModalTokens.enterDuration
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "scale"
                from: IKModalTokens.enterScale
                to: 1
                duration: IKModalTokens.enterDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: IKModalTokens.exitDuration
            easing.type: Easing.InCubic
        }
    }

    background: Rectangle {
        radius: IKRadius.r16
        color: IKColors.modalSurface
        border.width: IKModalTokens.borderWidth
        border.color: IKColors.modalBorder
    }

    contentItem: Column {
        id: modalContent

        spacing: IKModalTokens.contentSpacing
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        Item {
            width: parent.width
            implicitHeight: Math.max(headerIcon.height, titleText.implicitHeight)

            IKTintedIcon {
                id: headerIcon

                width: visible ? IKModalTokens.iconSize : 0
                height: visible ? IKModalTokens.iconSize : 0
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                visible: root.iconSource.toString().length > 0
                source: root.iconSource
                color: root.iconColor
            }

            Text {
                id: titleText

                anchors.left: headerIcon.right
                anchors.leftMargin: headerIcon.visible ? IKModalTokens.headerSpacing : 0
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: root.title
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.title3Size
                font.weight: IKFonts.emphasized
                wrapMode: Text.WordWrap
            }
        }

        Column {
            id: bodyColumn

            width: parent.width
            spacing: IKModalTokens.bodySpacing
        }

        Item {
            width: parent.width
            implicitHeight: footerLayout.implicitHeight

            GridLayout {
                id: footerLayout

                anchors.right: parent.right
                width: root.actionsStacked ? parent.width : implicitWidth
                columns: root.actionsStacked ? 1 : root.actionColumns
                columnSpacing: IKModalTokens.actionSpacing
                rowSpacing: IKModalTokens.actionSpacing
            }
        }
    }
}
