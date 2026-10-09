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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import kDrive.UI

// One search result: file type icon, name, "folder - date - size" subtitle, and a web marker for remote results.
ItemDelegate {
    id: root

    required property int index
    required property string name
    required property string fileIconName
    required property string subtitleText
    required property bool isDirectory
    required property bool availableLocally
    // Keyboard cursor of the list, drawn like the hover state.
    property bool current: false

    // Emitted on click. Ctrl asks to reveal a local result in its folder rather than to open it.
    signal activated(bool revealInFolder)

    padding: IKSearch.rowPadding
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    Accessible.name: root.name
    Accessible.description: root.availableLocally ? root.subtitleText
                                                  : root.subtitleText + ". " + qsTrId("searchResultOpenInBrowserTooltip")

    // Accessible presses use the delegate's inherited clicked signal.
    onClicked: root.activated(false)

    background: Rectangle {
        radius: IKSearch.rowRadius
        color: root.hovered || root.current ? IKColors.surfaceSecondary : "transparent"
    }

    contentItem: Item {
        implicitHeight: textColumn.implicitHeight

        ActivityFileIcon {
            id: fileIcon

            anchors.left: parent.left
            anchors.top: parent.top
            anchors.topMargin: (IKSearch.rowTextLineHeight - height) / 2
            width: IKSearch.rowFileIconSize
            height: IKSearch.rowFileIconSize
            fileIconName: root.fileIconName
            isDirectory: root.isDirectory
        }

        Column {
            id: textColumn

            anchors.left: fileIcon.right
            anchors.leftMargin: IKSearch.rowSpacing
            anchors.right: externalIcon.visible ? externalIcon.left : parent.right
            anchors.rightMargin: externalIcon.visible ? IKSearch.rowSpacing : 0

            Text {
                id: nameText

                width: parent.width
                text: root.name
                textFormat: Text.PlainText
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.title3Size
                lineHeightMode: Text.FixedHeight
                lineHeight: IKSearch.rowTextLineHeight
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                width: parent.width
                text: root.subtitleText
                textFormat: Text.PlainText
                color: IKColors.textTertiary
                font.pixelSize: IKFonts.subheadlineSize
                lineHeightMode: Text.FixedHeight
                lineHeight: IKSearch.rowSubtitleLineHeight
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        IKExternalLinkIcon {
            id: externalIcon

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: (IKSearch.rowTextLineHeight - height) / 2
            visible: !root.availableLocally
            color: IKColors.textSecondary
        }
    }

    // Pointer clicks are handled here to preserve Ctrl modifiers. Hover stays with the delegate, as this area does not
    // accept hover events.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onClicked: mouse => root.activated((mouse.modifiers & Qt.ControlModifier) !== 0)
    }

    // The full name when it is elided, then the browser notice for a remote result.
    IKToolTip {
        targetButton: root
        showRequested: root.hovered
        text: [nameText.truncated ? root.name : "",
               root.availableLocally ? "" : qsTrId("searchResultOpenInBrowserTooltip")].filter(line => line.length > 0).join("\n")
    }
}
