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
import QtQuick.Controls
import kDrive.UI

FocusScope {
    id: root

    required property bool canGoBack
    required property string currentTitle
    property string previousTitle: ""

    signal backRequested

    ToolButton {
        id: backButton

        anchors.left: parent.left
        anchors.leftMargin: IKSettings.pageMargin
        anchors.verticalCenter: parent.verticalCenter
        width: IKSettings.iconButtonSize
        height: IKSettings.iconButtonSize
        visible: root.canGoBack
        enabled: root.canGoBack
        focusPolicy: enabled ? Qt.StrongFocus : Qt.NoFocus
        hoverEnabled: enabled
        display: AbstractButton.IconOnly
        Accessible.role: Accessible.Button
        Accessible.name: qsTrId("accessibilityBack")
        Accessible.description: root.previousTitle
        onClicked: root.backRequested()

        background: Rectangle {
            radius: width / 2
            color: backButton.down ? IKColors.surfaceTertiary
                                   : backButton.hovered ? IKColors.surfaceSecondary : "transparent"
            border.width: backButton.visualFocus ? 2 : 0
            border.color: IKColors.accentPrimary
        }

        contentItem: Item {
            IKTintedIcon {
                anchors.centerIn: parent
                width: IKSettings.navigationIconSize
                height: width
                source: "qrc:/assets/settings/chevron-left.svg"
                color: backButton.enabled ? IKColors.textPrimary : IKColors.actionDisabled
            }
        }
    }

    Text {
        anchors.left: root.canGoBack ? backButton.right : parent.left
        anchors.leftMargin: root.canGoBack ? IKSpacing.s12 : IKSettings.pageMargin
        anchors.right: parent.right
        anchors.rightMargin: IKSettings.pageMargin
        anchors.verticalCenter: parent.verticalCenter
        text: root.currentTitle
        font.pixelSize: IKFonts.headlineSize
        font.weight: IKFonts.emphasized
        color: IKColors.textPrimary
        elide: Text.ElideRight
    }
}
