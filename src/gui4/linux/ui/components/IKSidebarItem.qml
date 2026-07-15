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
import QtQuick.Effects
import kDrive.UI

Rectangle {
    id: root

    property url iconSource
    property string label: ""
    property bool selected: false
    property int badgeCount: 0
    property bool hovered: false
    signal triggered

    implicitHeight: IKMainWindow.sidebarItemHeight
    radius: IKRadius.r8
    color: selected ? IKColors.surfaceTertiary : hovered ? IKColors.surfacePrimary : "transparent"
    opacity: enabled ? 1 : 0.45

    Image {
        id: iconImage

        anchors.left: parent.left
        anchors.leftMargin: IKSpacing.s12
        anchors.verticalCenter: parent.verticalCenter
        width: IKIconSizes.medium
        height: IKIconSizes.medium
        source: root.iconSource
        sourceSize.width: width
        sourceSize.height: height
        layer.enabled: true
        layer.effect: MultiEffect {
            colorization: 1
            colorizationColor: root.selected ? IKColors.textPrimary : IKColors.textSecondary
        }
    }

    Text {
        anchors.left: iconImage.right
        anchors.leftMargin: IKSpacing.s12
        anchors.right: badge.left
        anchors.rightMargin: IKSpacing.s8
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: root.selected ? IKColors.textPrimary : IKColors.textSecondary
        font.pixelSize: IKFonts.bodySize
        font.weight: root.selected ? IKFonts.emphasized : Font.Normal
        elide: Text.ElideRight
    }

    IKBadge {
        id: badge

        anchors.right: parent.right
        anchors.rightMargin: IKSpacing.s12
        anchors.verticalCenter: parent.verticalCenter
        count: root.badgeCount
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onEntered: root.hovered = true
        onExited: root.hovered = false
        onClicked: root.triggered()
    }
}
