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

import QtQuick
import kDrive.UI

Item {
    Row {
        anchors.left: parent.left
        anchors.leftMargin: IKSpacing.s8
        anchors.verticalCenter: parent.verticalCenter
        spacing: IKSpacing.s8

        Image {
            width: IKIconSizes.large
            height: IKIconSizes.large
            anchors.verticalCenter: parent.verticalCenter
            source: "qrc:/assets/taskbar/logo_kdrive.svg"
            sourceSize.width: width
            sourceSize.height: height
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "kDrive"
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.headlineSize
            font.weight: IKFonts.emphasized
        }
    }
}
