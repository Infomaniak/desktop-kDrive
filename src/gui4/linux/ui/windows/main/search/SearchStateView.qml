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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick
import kDrive.UI

// Centered illustration, title, subtitle, and optional action, as the macOS search sheet shows them.
Item {
    id: root

    required property string title
    property string subtitle
    property string actionText

    signal actionTriggered

    Column {
        anchors.centerIn: parent
        width: Math.max(0, parent.width - 2 * IKSearch.emptyContentMargin)
        spacing: IKSearch.emptyContentSpacing

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(IKSearch.emptyIllustrationMaxWidth, parent.width)
            height: width / IKSearch.emptyIllustrationAspectRatio
            source: ThemeMode.isDark ? "qrc:/assets/main/search/empty-dark.svg" : "qrc:/assets/main/search/empty.svg"
            fillMode: Image.PreserveAspectFit
            sourceSize.width: width
            sourceSize.height: height
        }

        Column {
            width: parent.width
            spacing: IKSearch.emptyTextSpacing

            Text {
                width: parent.width
                text: root.title
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.title3Size
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                visible: root.subtitle.length > 0
                text: root.subtitle
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }

        IKModalButton {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.actionText.length > 0
            text: root.actionText
            onClicked: root.actionTriggered()
        }
    }
}
