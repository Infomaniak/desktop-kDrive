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

// Card of the deleted items of a mass deletion warning: the total, then the items the server sent (path from the
// synchronization folder, complete path in the tooltip), closed by the count of the items left out.
Rectangle {
    id: root

    required property int hiddenCount
    required property int itemCount
    required property var items

    border.color: IKColors.modalListBorder
    border.width: IKModalTokens.borderWidth
    clip: true
    color: IKColors.modalListSurface
    implicitHeight: content.implicitHeight + 2 * border.width
    radius: IKRadius.r8

    Column {
        id: content

        anchors.fill: parent
        anchors.margins: root.border.width

        Text {
            color: IKColors.textSecondary
            elide: Text.ElideRight
            font.pixelSize: IKFonts.bodySize
            height: IKModalTokens.fileListCaptionHeight
            leftPadding: IKModalTokens.fileListPadding
            rightPadding: IKModalTokens.fileListPadding
            text: qsTrId("manyDeleteDialogFilesHeader").arg(root.itemCount)
            verticalAlignment: Text.AlignVCenter
            width: parent.width
        }
        Rectangle {
            color: IKColors.modalListBorder
            height: IKModalTokens.borderWidth
            width: parent.width
        }
        ListView {
            id: itemList

            acceptedButtons: Qt.NoButton
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            // Inline footer: the count of the items left out only shows once the list is scrolled to its end.
            footer: root.hiddenCount > 0 ? hiddenItemsFooter : null
            height: Math.min(contentHeight, IKModalTokens.fileListMaxHeight)
            model: root.items
            width: parent.width

            ScrollBar.vertical: ScrollBar {
                policy: itemList.contentHeight > itemList.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }
            delegate: Item {
                id: itemRow

                required property var modelData

                height: IKModalTokens.fileListRowHeight
                width: itemList.width

                HoverHandler {
                    id: itemHover
                }
                ActivityFileIcon {
                    id: itemIcon

                    anchors.left: parent.left
                    anchors.leftMargin: IKModalTokens.fileListPadding
                    anchors.verticalCenter: parent.verticalCenter
                    fileIconName: itemRow.modelData.iconName
                    isDirectory: false
                }
                Text {
                    anchors.left: itemIcon.right
                    anchors.leftMargin: IKModalTokens.fileListIconSpacing
                    anchors.right: parent.right
                    anchors.rightMargin: IKModalTokens.fileListPadding
                    anchors.verticalCenter: parent.verticalCenter
                    color: IKColors.textPrimary
                    elide: Text.ElideMiddle
                    font.pixelSize: IKFonts.bodySize
                    text: itemRow.modelData.shortPath
                    textFormat: Text.PlainText
                }
                // Always shown on hover, even when the path is not elided: it adds where the synchronization folder is.
                IKToolTip {
                    maximumTextWidth: IKModalTokens.fileListTooltipMaximumWidth
                    showRequested: itemHover.hovered
                    text: itemRow.modelData.displayPath
                }
            }
        }
    }
    Component {
        id: hiddenItemsFooter

        Column {
            width: itemList.width

            Rectangle {
                color: IKColors.modalListBorder
                height: IKModalTokens.borderWidth
                width: parent.width
            }
            Text {
                color: IKColors.textSecondary
                elide: Text.ElideRight
                font.pixelSize: IKFonts.bodySize
                height: IKModalTokens.fileListCaptionHeight
                horizontalAlignment: Text.AlignHCenter
                leftPadding: IKModalTokens.fileListPadding
                rightPadding: IKModalTokens.fileListPadding
                // The plural form holds "%1" rather than "%n" (its Loco source is "%@"), which qsTrId does not fill. The
                // singular form has no placeholder, hence replace() rather than arg().
                text: qsTrId("manyDeleteDialogOtherPlaceholder", root.hiddenCount).replace("%1", root.hiddenCount)
                verticalAlignment: Text.AlignVCenter
                width: parent.width
            }
        }
    }
}
