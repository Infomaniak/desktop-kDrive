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
import QtQuick.Controls.Basic
import kDrive.UI

AbstractButton {
    id: root
    required text
    property bool value: false
    signal toggleRequested(bool value)
    implicitWidth: IKSettings.switchWidth
    implicitHeight: IKSettings.buttonHeight
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.role: Accessible.CheckBox
    Accessible.checkable: true
    Accessible.checked: value
    Accessible.name: text
    Accessible.onPressAction: root.clicked()
    Accessible.onToggleAction: root.clicked()
    onClicked: toggleRequested(!value)
    background: Item {}
    contentItem: Item {
        Rectangle {
            anchors.centerIn: parent
            width: IKSettings.switchWidth
            height: IKSettings.switchHeight
            radius: height / 2
            color: !root.enabled ? IKColors.actionDisabled : root.value ? IKColors.actionPrimary : IKColors.textTertiary
            border.width: root.visualFocus ? 2 : 0
            border.color: IKColors.accentPrimary
            Rectangle {
                x: root.value ? parent.width - width - IKSettings.switchInset : IKSettings.switchInset
                y: IKSettings.switchInset
                width: parent.height - 2 * IKSettings.switchInset
                height: width
                radius: height / 2
                color: IKColors.actionOnPrimary
            }
        }
    }
}
