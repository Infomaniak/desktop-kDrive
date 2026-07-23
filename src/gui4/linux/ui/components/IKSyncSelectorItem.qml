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
import QtQuick.Effects
import kDrive.UI

// Presents one configured synchronization by title, drive color, and optional subtitle and dropdown chevron.
Button {
    id: root

    property string title: ""
    property string subtitle: ""
    property color driveColor: IKColors.driveDefaultColor
    property bool selected: false
    property bool interactive: true
    property bool showChevron: false
    readonly property bool hasSubtitle: subtitle.length > 0
    signal triggered

    implicitHeight: hasSubtitle ? IKMainWindow.syncSelectorAdvancedHeight : IKMainWindow.syncSelectorHeight
    padding: 0
    enabled: interactive
    focusPolicy: interactive ? Qt.StrongFocus : Qt.NoFocus
    hoverEnabled: interactive
    text: hasSubtitle ? title + ", " + subtitle : title
    onClicked: triggered()

    background: Rectangle {
        radius: IKRadius.r4
        color: root.selected || root.down || root.hovered ? IKColors.surfaceTertiary : IKColors.surfacePrimary
        border.width: root.visualFocus ? 2 : 0
        border.color: IKColors.accentPrimary
    }

    contentItem: Item {
        IKDriveIcon {
            id: driveIcon

            anchors.left: parent.left
            anchors.leftMargin: IKSpacing.s8
            anchors.verticalCenter: parent.verticalCenter
            driveColor: root.driveColor
        }

        Column {
            anchors.left: driveIcon.right
            anchors.leftMargin: IKSpacing.s8
            anchors.right: chevron.left
            anchors.rightMargin: IKSpacing.s8
            anchors.verticalCenter: parent.verticalCenter

            Text {
                width: parent.width
                text: root.title
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: root.hasSubtitle
                text: root.subtitle
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                elide: Text.ElideRight
            }
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
    }

    HoverHandler {
        cursorShape: root.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
