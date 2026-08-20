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
import QtQuick.Layouts
import kDrive.UI

Item {
    id: root

    required property var controller

    ColumnLayout {
        anchors.bottomMargin: IKSpacing.s24
        anchors.fill: parent
        anchors.leftMargin: IKSpacing.s24
        anchors.rightMargin: IKSpacing.s24
        anchors.topMargin: IKSpacing.s32
        spacing: IKSpacing.s24

        HomeGreeting {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            controller: root.controller
            visible: root.controller.status !== HomeController.SetupRequired && root.controller.status !== HomeController.Loading
        }
        IKErrorBanner {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            errorCount: root.controller.errorCount
            visible: root.controller.errorCount > 0
            onActionTriggered: root.controller.showActivities()
        }
        Row {
            id: panelsRow

            readonly property bool hasDriveShortcuts: root.controller.hasCurrentDrive
            readonly property real panelsWidth: Math.max(0, width - (hasDriveShortcuts ? spacing : 0))

            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: IKSpacing.s24

            HomeStatusPanel {
                controller: root.controller
                height: panelsRow.height
                width: panelsRow.hasDriveShortcuts ? Math.max(0, panelsRow.panelsWidth - driveWebShortcuts.implicitWidth) : panelsRow.width
            }
            DriveWebShortcutsView {
                id: driveWebShortcuts

                controller: root.controller
                height: panelsRow.height
                visible: panelsRow.hasDriveShortcuts
                width: implicitWidth
            }
        }
    }
}
