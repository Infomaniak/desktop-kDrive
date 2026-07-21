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

// Presents one configured synchronization by drive name and color, with an optional dropdown chevron.
Rectangle {
    id: root

    property string driveName: ""
    property color driveColor: IKColors.driveDefaultColor
    property bool selected: false
    readonly property bool hovered: pointerArea.enabled && pointerArea.containsMouse
    property bool interactive: true
    property bool showChevron: false
    signal triggered

    implicitHeight: IKMainWindow.syncSelectorHeight
    radius: IKRadius.r4
    color: selected || hovered ? IKColors.surfaceTertiary : IKColors.surfacePrimary

    IKDriveIcon {
        id: driveIcon

        anchors.left: parent.left
        anchors.leftMargin: IKSpacing.s8
        anchors.verticalCenter: parent.verticalCenter
        driveColor: root.driveColor
    }

    Text {
        anchors.left: driveIcon.right
        anchors.leftMargin: IKSpacing.s8
        anchors.right: chevron.left
        anchors.rightMargin: IKSpacing.s8
        anchors.verticalCenter: parent.verticalCenter
        text: root.driveName
        color: IKColors.textPrimary
        font.pixelSize: IKFonts.bodySize
        elide: Text.ElideRight
    }

    Image {
        id: chevron

        anchors.right: parent.right
        anchors.rightMargin: IKSpacing.s8
        anchors.verticalCenter: parent.verticalCenter
        width: root.showChevron ? IKIconSizes.small : 0
        height: IKIconSizes.small
        visible: root.showChevron
        source: "qrc:/assets/main/chevron-down.svg"
        sourceSize.width: width
        sourceSize.height: height
        layer.enabled: true
        layer.effect: MultiEffect {
            colorization: 1
            colorizationColor: IKColors.textSecondary
        }
    }

    MouseArea {
        id: pointerArea

        anchors.fill: parent
        enabled: root.interactive
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        cursorShape: root.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.triggered()
    }
}
