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

ToolButton {
    id: root
    implicitWidth: IKSettings.iconButtonSize
    implicitHeight: IKSettings.iconButtonSize
    text: qsTrId("accessibilityMoreInformation")
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.name: text
    contentItem: Item {
        IKTintedIcon {
            anchors.centerIn: parent
            width: IKSettings.informationIconSize
            height: width
            source: "qrc:/assets/settings/information.svg"
            color: IKColors.textSecondary
        }
    }
    background: Rectangle {
        radius: width / 2
        color: root.hovered || root.down ? IKColors.surfaceTertiary : "transparent"
        border.width: root.visualFocus ? 2 : 0
        border.color: IKColors.accentPrimary
    }
    IKToolTip {
        targetButton: root
        text: root.text
    }
}
