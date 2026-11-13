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

            Accessible.name: qsTrId("manyDeleteDialogFilesHeader").arg(root.itemCount)
            Accessible.role: Accessible.List
            acceptedButtons: Qt.NoButton
            // A single tab stop: Up and Down move the current row, which the view keeps in sight, so keyboard users can
            // read every path before choosing an action.
            activeFocusOnTab: true
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            currentIndex: -1
            // Inline footer: the count of the items left out only shows once the list is scrolled to its end.
            footer: root.hiddenCount > 0 ? hiddenItemsFooter : null
            height: Math.min(contentHeight, IKModalTokens.fileListMaxHeight)
            keyNavigationEnabled: true
            model: root.items
            width: parent.width

            ScrollBar.vertical: ScrollBar {
                policy: itemList.contentHeight > itemList.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }
            delegate: Item {
                id: itemRow

                readonly property bool keyboardCurrent: ListView.isCurrentItem && itemList.activeFocus
                required property var modelData

                Accessible.description: itemRow.modelData.displayPath
                Accessible.name: itemRow.modelData.shortPath
                Accessible.role: Accessible.ListItem
                Accessible.selected: itemRow.keyboardCurrent
                height: IKModalTokens.fileListRowHeight
                width: itemList.width

                HoverHandler {
                    id: itemHover
                }
                Rectangle {
                    anchors.fill: parent
                    border.color: IKColors.accentPrimary
                    border.width: IKModalTokens.focusBorderWidth
                    color: "transparent"
                    radius: IKRadius.r4
                    visible: itemRow.keyboardCurrent
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

            onActiveFocusChanged: {
                if (activeFocus && currentIndex < 0 && count > 0) {
                    currentIndex = 0;
                }
            }
            onCurrentIndexChanged: {
                if (currentIndex < 0) {
                    return;
                }
                // The last row also brings the count of the items left out into view.
                if (currentIndex === count - 1) {
                    positionViewAtEnd();
                } else {
                    positionViewAtIndex(currentIndex, ListView.Contain);
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
