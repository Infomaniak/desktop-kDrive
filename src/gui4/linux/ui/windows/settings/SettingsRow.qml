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

Item {
    id: root
    default property alias accessories: trailing.data
    required property string title
    property string description: ""
    property bool separator: true
    width: parent.width
    implicitHeight: Math.max(IKSettings.rowHeight, layout.implicitHeight + 2 * IKSettings.rowPadding)
    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.topMargin: IKSettings.rowPadding
        anchors.bottomMargin: IKSettings.rowPadding
        spacing: IKSettings.rowSpacing
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text {
                Layout.fillWidth: true
                text: root.title
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.description
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                wrapMode: Text.WordWrap
            }
        }
        RowLayout {
            id: trailing
            spacing: IKSpacing.s8
        }
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        visible: root.separator
        color: IKColors.settingsDivider
    }
}
