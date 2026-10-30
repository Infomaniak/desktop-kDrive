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
import QtQuick.Layouts
import kDrive.UI

// One version conflict of the list: its file, with the relative path in the tooltip, and "Choose a version".
Item {
    id: root

    required property int index
    required property string rowId
    required property string name
    required property string path
    required property string iconName
    required property bool isDirectory
    required property bool actionPending
    required property bool isLast

    signal chooseRequested(string rowId)

    implicitHeight: IKActivities.conflictsRowHeight

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: IKActivities.conflictsRowHorizontalPadding
        anchors.rightMargin: IKActivities.conflictsRowHorizontalPadding
        spacing: IKActivities.conflictsRowSpacing

        ActivityFileIcon {
            Layout.preferredWidth: IKActivities.conflictsRowIconSize
            Layout.preferredHeight: IKActivities.conflictsRowIconSize
            width: IKActivities.conflictsRowIconSize
            height: IKActivities.conflictsRowIconSize
            fileIconName: root.iconName
            isDirectory: root.isDirectory
        }

        Text {
            Layout.fillWidth: true
            text: root.name
            textFormat: Text.PlainText
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize
            elide: Text.ElideMiddle

            HoverHandler {
                id: nameHover
            }

            IKToolTip {
                text: root.path
                showRequested: nameHover.hovered
            }
        }

        IKModalButton {
            implicitHeight: IKActivities.conflictsButtonHeight
            role: IKModalButton.Tonal
            text: qsTrId("conflictErrorAction")
            busy: root.actionPending
            // Tells the buttons of the rows apart.
            Accessible.name: text + ", " + root.name
            onClicked: root.chooseRequested(root.rowId)
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: IKActivities.conflictsRowHorizontalPadding
        anchors.rightMargin: IKActivities.conflictsRowHorizontalPadding
        height: 1
        visible: !root.isLast
        color: IKColors.conflictsListDivider
    }
}
