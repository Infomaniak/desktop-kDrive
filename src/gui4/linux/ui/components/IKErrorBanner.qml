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

// Synchronization error banner shared by Home and Activities. The owning view supplies the height its screen
// specifies and handles `actionTriggered`; the banner itself never reads a controller.
Rectangle {
    id: root

    required property int errorCount

    signal actionTriggered

    implicitHeight: IKMainWindow.errorBannerHeight
    radius: IKRadius.r12
    color: IKColors.errorBannerSurface

    Row {
        anchors.fill: parent
        anchors.margins: IKSpacing.s16
        spacing: IKSpacing.s16

        Column {
            width: Math.max(0, parent.width - fixButton.width - parent.spacing)
            anchors.verticalCenter: parent.verticalCenter
            spacing: IKSpacing.s2

            Text {
                width: parent.width
                text: qsTrId("informationBlockSynchroErrorTitle", root.errorCount)
                textFormat: Text.StyledText
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: qsTrId("informationBlockSynchroErrorSubtitle")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                elide: Text.ElideRight
            }
        }

        Button {
            id: fixButton

            anchors.verticalCenter: parent.verticalCenter
            implicitHeight: IKMainWindow.errorBannerActionButtonHeight
            text: qsTrId("buttonFixErrors")
            focusPolicy: Qt.StrongFocus
            onClicked: root.actionTriggered()

            contentItem: Text {
                text: fixButton.text
                color: IKColors.actionOnPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.emphasized
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                implicitHeight: IKMainWindow.errorBannerActionButtonHeight
                radius: IKRadius.r6
                color: IKColors.actionPrimary
            }

            padding: 0
            leftPadding: IKSpacing.s16
            rightPadding: IKSpacing.s16
            topPadding: 0
            bottomPadding: 0
        }
    }
}
