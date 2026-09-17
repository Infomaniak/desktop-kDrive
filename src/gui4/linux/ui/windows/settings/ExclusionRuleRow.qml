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

    required property var controller
    required property int row
    required property string pattern
    required property bool notificationEnabled
    required property bool selected
    property bool editable: false

    implicitHeight: IKSettings.exclusionRowHeight

    Rectangle {
        anchors.fill: parent
        radius: IKRadius.r6
        color: root.editable && root.selected ? IKColors.surfaceTertiary : "transparent"
    }

    RowLayout {
        anchors.fill: parent
        spacing: IKSpacing.s8

        IKCheckBox {
            Layout.preferredWidth: visible ? implicitWidth : 0
            Layout.preferredHeight: visible ? implicitHeight : 0
            visible: root.editable
            enabled: root.controller.ready && !root.controller.saving
            checkState: root.selected ? Qt.Checked : Qt.Unchecked
            Accessible.name: root.pattern
            onClicked: root.controller.setSelected(root.row, !root.selected)
        }

        IKTintedIcon {
            Layout.preferredWidth: IKSettings.exclusionFileIconSize
            Layout.preferredHeight: IKSettings.exclusionFileIconSize
            source: "qrc:/assets/main/activities/file.svg"
            color: IKColors.textTertiary
            Accessible.ignored: true
        }

        Text {
            Layout.fillWidth: true
            text: root.pattern
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize
            elide: Text.ElideMiddle
        }

        IKSwitch {
            Layout.preferredWidth: visible ? implicitWidth : 0
            Layout.preferredHeight: visible ? implicitHeight : 0
            visible: root.editable
            enabled: root.controller.ready && !root.controller.saving
            text: qsTrId("labelNotifyIfFileExcluded")
            value: root.notificationEnabled
            onToggleRequested: value => root.controller.setRuleNotification(root.row, value)
        }
    }
}
