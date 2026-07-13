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

Item {
    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: IKSpacing.s24
        spacing: IKSpacing.s24

        Text {
            width: parent.width
            text: qsTrId("synchroUpToDate")
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.titleSize
            font.weight: IKFonts.emphasized
            elide: Text.ElideRight
        }

        Rectangle {
            width: parent.width
            height: 240
            radius: IKRadius.r8
            color: IKColors.statusLightSecurity

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 2 * IKSpacing.s24, 360)
                spacing: IKSpacing.s8

                Rectangle {
                    width: 52
                    height: 52
                    radius: width / 2
                    color: IKColors.statusMediumSuccess
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTrId("synchroStatusUpToDateTitle")
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.headlineSize
                    font.weight: IKFonts.emphasized
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTrId("synchroStatusUpToDateDescription")
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.bodySize
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
