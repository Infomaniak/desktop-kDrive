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

// Provides an interactive sidebar row whose selection, disabled, notification, and trailing-accessory states combine.
Rectangle {
    id: root

    property url iconSource
    property string label: ""
    property bool selected: false
    property int badgeCount: 0
    property bool notificationDot: false
    property url trailingIconSource
    readonly property bool hovered: pointerArea.enabled && pointerArea.containsMouse
    readonly property bool pressed: pointerArea.pressed
    signal triggered

    implicitHeight: IKMainWindow.sidebarItemHeight
    radius: IKRadius.r8
    color: !enabled ? "transparent"
                    : selected || pressed ? IKColors.surfaceTertiary : hovered ? IKColors.surfacePrimary : "transparent"

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
            colorizationColor: root.enabled ? IKColors.textSecondary : IKColors.actionDisabled
        }
    }

    Text {
        anchors.left: iconImage.right
        anchors.leftMargin: IKSpacing.s12
        anchors.right: accessories.left
        anchors.rightMargin: IKSpacing.s8
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: root.enabled ? IKColors.textPrimary : IKColors.actionDisabled
        font.pixelSize: IKFonts.bodySize
        font.weight: IKFonts.regular
        elide: Text.ElideRight
    }

    Row {
        id: accessories

        anchors.right: parent.right
        anchors.rightMargin: IKSpacing.s12
        anchors.verticalCenter: parent.verticalCenter
        spacing: IKSpacing.s8

        IKBadge {
            count: root.enabled ? root.badgeCount : 0
            dot: root.enabled && root.notificationDot
        }

        Image {
            visible: root.trailingIconSource.toString().length > 0
            width: visible ? IKIconSizes.medium : 0
            height: IKIconSizes.medium
            source: root.trailingIconSource
            sourceSize.width: width
            sourceSize.height: height
            layer.enabled: visible
            layer.effect: MultiEffect {
                colorization: 1
                colorizationColor: root.enabled ? IKColors.textSecondary : IKColors.actionDisabled
            }
        }
    }

    MouseArea {
        id: pointerArea

        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.triggered()
    }
}
