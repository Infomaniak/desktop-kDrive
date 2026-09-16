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
import QtQuick.Layouts
import kDrive.UI

Button {
    id: root

    required property string title
    property string description: ""
    property bool separator: true

    signal navigationRequested(var trigger)

    width: parent.width
    z: visualFocus ? 1 : 0
    implicitHeight: Math.max(IKSettings.rowHeight, contentLayout.implicitHeight + 2 * IKSettings.rowPadding)
    padding: 0
    leftPadding: IKSettings.groupPadding
    rightPadding: IKSettings.groupPadding
    focusPolicy: enabled ? Qt.StrongFocus : Qt.NoFocus
    hoverEnabled: enabled
    Accessible.role: Accessible.Button
    Accessible.name: title
    onClicked: navigationRequested(root)

    background: Rectangle {
        radius: IKRadius.r6
        color: !root.enabled ? "transparent"
                             : root.down ? IKColors.surfaceTertiary
                                         : root.hovered ? IKColors.toolbarControlSurface : "transparent"
        border.width: root.visualFocus ? 2 : 0
        border.color: IKColors.accentPrimary
    }

    contentItem: RowLayout {
        id: contentLayout

        spacing: IKSettings.rowSpacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.title
                color: root.enabled ? IKColors.textPrimary : IKColors.actionDisabled
                font.pixelSize: IKFonts.bodySize
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.description
                color: root.enabled ? IKColors.textSecondary : IKColors.actionDisabled
                font.pixelSize: IKFonts.subheadlineSize
                wrapMode: Text.WordWrap
            }
        }

        IKTintedIcon {
            Layout.preferredWidth: IKSettings.navigationIconSize
            Layout.preferredHeight: IKSettings.navigationIconSize
            source: "qrc:/assets/settings/chevron-right.svg"
            color: root.enabled ? IKColors.settingsNavigationIcon : IKColors.actionDisabled
            Accessible.ignored: true
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: IKSettings.groupPadding
        anchors.right: parent.right
        anchors.rightMargin: IKSettings.groupPadding
        anchors.bottom: parent.bottom
        height: 1
        visible: root.separator && !root.visualFocus
        color: IKColors.settingsDivider
    }

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
