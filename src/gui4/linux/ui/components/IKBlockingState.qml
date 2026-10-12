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

pragma ComponentBehavior: Bound

import QtQuick
import kDrive.UI

// Card of a full-page blocking state (update required, blocking synchronization error): a visual, a title, a
// description and a row of actions, centered. The owner places the card and provides the visual and the actions.
Rectangle {
    id: root

    required property string title
    required property string description
    property alias visual: visualContainer.data
    property alias actions: actionsRow.data

    radius: IKBlockingStateTokens.cardRadius
    color: IKColors.blockingStateSurface
    border.width: IKBlockingStateTokens.cardBorderWidth
    border.color: IKColors.blockingStateBorder

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: IKBlockingStateTokens.contentMargin
        anchors.verticalCenter: parent.verticalCenter
        spacing: IKBlockingStateTokens.visualSpacing

        Item {
            id: visualContainer

            anchors.horizontalCenter: parent.horizontalCenter
            width: childrenRect.width
            height: childrenRect.height
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width, IKBlockingStateTokens.textMaximumWidth)
            spacing: IKBlockingStateTokens.textToActionsSpacing

            Column {
                width: parent.width
                spacing: IKBlockingStateTokens.textSpacing

                Text {
                    width: parent.width
                    text: root.title
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.titleSize
                    font.weight: IKFonts.emphasized
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.Heading
                }

                Text {
                    width: parent.width
                    text: root.description
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }

            Row {
                id: actionsRow

                anchors.horizontalCenter: parent.horizontalCenter
                spacing: IKBlockingStateTokens.actionsSpacing
            }
        }
    }
}
