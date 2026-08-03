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

Item {
    id: root

    required property var controller
    required property int visualStatus
    required property string title
    required property string description
    property string actionLabel: ""

    Column {
        id: content

        anchors.centerIn: parent
        width: parent.width
        spacing: IKSpacing.s32

        HomeStatusVisual {
            anchors.horizontalCenter: parent.horizontalCenter
            status: root.visualStatus
        }

        Column {
            width: parent.width
            spacing: IKSpacing.s8

            Text {
                width: parent.width
                text: root.title
                horizontalAlignment: Text.AlignHCenter
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.headlineSize
                font.weight: IKFonts.emphasized
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: root.description
                horizontalAlignment: Text.AlignHCenter
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                wrapMode: Text.WordWrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: root.actionLabel.length > 0
                enabled: root.controller.primaryAction !== HomeController.None
                text: root.actionLabel
                flat: true
                focusPolicy: Qt.StrongFocus
                onClicked: root.controller.triggerPrimaryAction()

                contentItem: Text {
                    text: parent.text
                    color: IKColors.actionPrimary
                    font.pixelSize: IKFonts.bodySize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
