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
    property bool lastRow: false
    property real contentInset: 0

    signal notificationSaveStartedFromKeyboard(int row)

    function focusNotificationSwitch() {
        notificationSwitch.forceActiveFocus(Qt.TabFocusReason);
    }

    implicitHeight: IKSettings.exclusionRowHeight

    Rectangle {
        anchors.fill: parent
        radius: root.editable ? IKRadius.r6 : 0
        bottomLeftRadius: root.editable ? IKRadius.r6 : (root.lastRow ? IKRadius.r12 : 0)
        bottomRightRadius: bottomLeftRadius
        color: {
            if (root.editable && root.selected) {
                return IKColors.surfaceTertiary
            }
            if (!root.editable && root.row % 2 !== 0) {
                return IKColors.settingsAlternateRowSurface
            }
            return "transparent"
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.contentInset
        anchors.rightMargin: root.contentInset
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

        Text {
            Layout.fillWidth: true
            text: root.pattern
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize
            elide: Text.ElideMiddle
        }

        IKSwitch {
            id: notificationSwitch

            Layout.preferredWidth: visible ? implicitWidth : 0
            Layout.preferredHeight: visible ? implicitHeight : 0
            visible: root.editable
            enabled: root.controller.ready && !root.controller.saving
            text: qsTrId("labelNotifyIfFileExcluded")
            value: root.notificationEnabled
            Accessible.name: root.pattern
            Accessible.description: text
            onToggleRequested: value => {
                // Only a keyboard focus is worth restoring: a mouse click must not leave a focus ring after the save.
                const keyboardFocus = notificationSwitch.visualFocus;
                root.controller.setRuleNotification(root.row, value);
                if (keyboardFocus && root.controller.saving) {
                    root.notificationSaveStartedFromKeyboard(root.row);
                }
            }
        }
    }
}
