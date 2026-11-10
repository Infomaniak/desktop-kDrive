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
import kDrive.UI

// Storage advice with its action: a tinted icon tile, a title, a description, and a tonal button.
Rectangle {
    id: root

    required property url iconSource
    required property string title
    required property string description
    required property string actionText

    signal actionTriggered

    color: IKColors.storageActionSurface
    radius: IKRadius.r12
    implicitHeight: Math.max(textColumn.implicitHeight, IKStorage.actionIconTileSize, actionButton.implicitHeight)
                    + 2 * IKStorage.actionCardPadding

    Rectangle {
        id: iconTile

        anchors.left: parent.left
        anchors.leftMargin: IKStorage.actionCardPadding
        anchors.top: parent.top
        anchors.topMargin: IKStorage.actionCardPadding
        width: IKStorage.actionIconTileSize
        height: IKStorage.actionIconTileSize
        radius: IKStorage.actionIconTileRadius
        color: IKColors.storageActionIconTile

        IKTintedIcon {
            anchors.centerIn: parent
            width: IKStorage.actionIconSize
            height: IKStorage.actionIconSize
            source: root.iconSource
            color: IKColors.storageActionIcon
        }
    }

    Column {
        id: textColumn

        anchors.left: iconTile.right
        anchors.leftMargin: IKStorage.actionContentSpacing
        anchors.right: actionButton.left
        anchors.rightMargin: IKStorage.actionButtonSpacing
        anchors.top: parent.top
        anchors.topMargin: IKStorage.actionCardPadding
        spacing: IKStorage.actionTextSpacing

        Text {
            width: parent.width
            text: root.title
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.medium
            wrapMode: Text.WordWrap
        }

        Text {
            width: parent.width
            text: root.description
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.subheadlineSize
            lineHeight: IKStorage.actionSubtitleLineHeight
            lineHeightMode: Text.FixedHeight
            wrapMode: Text.WordWrap
        }
    }

    IKModalButton {
        id: actionButton

        anchors.right: parent.right
        anchors.rightMargin: IKStorage.actionCardPadding
        anchors.verticalCenter: parent.verticalCenter
        implicitHeight: IKStorage.actionButtonHeight
        role: IKModalButton.Tonal
        iconSource: root.iconSource
        text: root.actionText

        onClicked: root.actionTriggered()
    }
}
