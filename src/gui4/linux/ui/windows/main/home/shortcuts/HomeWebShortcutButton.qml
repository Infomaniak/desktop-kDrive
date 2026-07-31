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
import kDrive.UI

Button {
    id: root

    required property url glyphSource

    implicitWidth: leftPadding + leadingIcon.width + IKSpacing.s8 + shortcutLabel.implicitWidth
                   + IKMainWindow.homeShortcutExternalIconSpacing
                   + externalIcon.width + rightPadding
    implicitHeight: IKMainWindow.homeShortcutHeight
    leftPadding: IKSpacing.s8
    rightPadding: IKSpacing.s8
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    contentItem: Row {
        spacing: IKMainWindow.homeShortcutExternalIconSpacing

        Row {
            width: Math.max(0, parent.width - externalIcon.width - parent.spacing)
            anchors.verticalCenter: parent.verticalCenter
            spacing: IKSpacing.s8

            IKTintedIcon {
                id: leadingIcon

                width: 14
                height: 14
                anchors.verticalCenter: parent.verticalCenter
                source: root.glyphSource
                color: IKColors.accentPrimary
            }

            Text {
                id: shortcutLabel

                width: Math.max(0, parent.width - leadingIcon.width - parent.spacing)
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                wrapMode: Text.NoWrap
            }
        }

        IKTintedIcon {
            id: externalIcon

            width: 10
            height: 10
            anchors.verticalCenter: parent.verticalCenter
            source: "qrc:/assets/main/home/external-link.svg"
            color: IKColors.textTertiary
        }
    }

    background: Rectangle {
        radius: IKRadius.r8
        color: root.hovered || root.down ? IKColors.surfaceTertiary : IKColors.surfacePrimary
        border.width: root.visualFocus ? 2 : 1
        border.color: root.visualFocus ? IKColors.accentPrimary : IKColors.surfaceTertiary
    }
}
