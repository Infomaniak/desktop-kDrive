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

Rectangle {
    id: root

    enum Mode {
        Hidden,
        Loading,
        Success,
        Error
    }

    property int mode: SidebarNotification.Hidden
    property string message: ""

    function showLoading(text) {
        dismissTimer.stop();
        root.message = text;
        root.mode = SidebarNotification.Loading;
    }

    function showSuccess(text) {
        root.message = text;
        root.mode = SidebarNotification.Success;
        dismissTimer.restart();
    }

    function showError(text) {
        root.message = text;
        root.mode = SidebarNotification.Error;
        dismissTimer.restart();
    }

    function hide() {
        dismissTimer.stop();
        root.mode = SidebarNotification.Hidden;
        root.message = "";
    }

    visible: mode !== SidebarNotification.Hidden
    implicitHeight: visible ? Math.max(IKMainWindow.sidebarNotificationMinHeight, notificationRow.implicitHeight + 2 * IKSpacing.s8) : 0
    radius: IKRadius.r12
    color: IKColors.sidebarNotificationSurface
    border.width: 1
    border.color: IKColors.sidebarNotificationBorder

    Row {
        id: notificationRow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: IKSpacing.s12
        anchors.rightMargin: IKSpacing.s12
        spacing: IKSpacing.s8

        IKLoadingSpinner {
            anchors.verticalCenter: parent.verticalCenter
            width: IKMainWindow.sidebarNotificationIconSize
            height: IKMainWindow.sidebarNotificationIconSize
            strokeWidth: 2
            visible: root.mode === SidebarNotification.Loading
        }

        IKTintedIcon {
            anchors.verticalCenter: parent.verticalCenter
            width: IKMainWindow.sidebarNotificationIconSize
            height: IKMainWindow.sidebarNotificationIconSize
            visible: root.mode === SidebarNotification.Success || root.mode === SidebarNotification.Error
            source: root.mode === SidebarNotification.Success ? "qrc:/assets/main/activities/status-synchronized-outline.svg" : "qrc:/assets/main/triangle-alert.svg"
            color: root.mode === SidebarNotification.Success ? IKColors.statusMediumSuccess : IKColors.statusMediumWarning
        }

        Text {
            width: Math.max(0, parent.width - x)
            text: root.message
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.medium
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }

    Timer {
        id: dismissTimer

        interval: IKMainWindow.sidebarNotificationDuration
        onTriggered: root.hide()
    }
}
