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

Item {
    id: root

    required property real nameColumnWidth
    required property real folderColumnWidth
    required property real timeColumnWidth
    required property real sizeColumnWidth
    required property real statusColumnWidth

    signal resizeRequested(real delta)

    width: nameColumnWidth + folderColumnWidth + timeColumnWidth + sizeColumnWidth + statusColumnWidth
    height: IKActivities.tableHeaderHeight

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: IKColors.activitiesDivider
    }

    Row {
        anchors.fill: parent

        HeaderCell {
            width: root.nameColumnWidth
            text: qsTrId("labelName")
            leftPadding: IKSpacing.s32 + IKSpacing.s8
            resizable: true
        }
        HeaderCell {
            width: root.folderColumnWidth
            text: qsTrId("labelFolder")
        }
        HeaderCell {
            width: root.timeColumnWidth
            text: qsTrId("labelTime")
        }
        HeaderCell {
            width: root.sizeColumnWidth
            text: qsTrId("labelSize")
        }
        HeaderCell {
            width: root.statusColumnWidth
            text: qsTrId("labelStatus")
        }
    }

    component HeaderCell: Item {
        id: cell

        required property string text
        property real leftPadding: IKActivities.secondaryCellPadding
        property bool resizable: false
        height: root.height

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            visible: cell.x > 0
            color: IKColors.activitiesDivider
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: cell.leftPadding
            anchors.right: parent.right
            anchors.rightMargin: IKActivities.secondaryCellPadding
            anchors.verticalCenter: parent.verticalCenter
            text: cell.text
            color: IKColors.textTertiary
            font.pixelSize: IKFonts.subheadlineSize
            font.weight: IKFonts.medium
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        MouseArea {
            id: resizeHandle

            property real previousX: 0

            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: IKActivities.columnResizeHandleWidth
            visible: cell.resizable
            cursorShape: Qt.SplitHCursor
            preventStealing: true
            onPressed: mouse => previousX = resizeHandle.mapToItem(root, mouse.x, 0).x
            onPositionChanged: mouse => {
                if (!pressed) {
                    return;
                }
                const currentX = resizeHandle.mapToItem(root, mouse.x, 0).x;
                const delta = currentX - previousX;
                previousX = currentX;
                root.resizeRequested(delta);
            }
        }
    }
}
