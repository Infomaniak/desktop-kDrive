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

IKModal {
    id: root

    required property var controller
    property Item triggerItem: null
    property var userDbId: 0
    property string userName
    property bool busy: false
    property bool failed: false
    property bool focusRestorationPending: false

    signal fallbackFocusRequested

    function showFrom(trigger, requestedUserDbId, requestedUserName) {
        triggerItem = trigger;
        userDbId = requestedUserDbId;
        userName = requestedUserName;
        busy = false;
        failed = false;
        focusRestorationPending = true;
        open();
    }

    title: qsTrId("dialogRemoveAccountTitle")
    actionsStacked: true
    escapeDismissible: !busy
    initialFocusItem: keepAccountButton
    onDismissRequested: close()
    onClosed: {
        busy = false;
        failed = false;
        if (triggerItem && triggerItem.enabled && triggerItem.visible) {
            triggerItem.forceActiveFocus(Qt.BacktabFocusReason);
        } else if (root.focusRestorationPending) {
            root.fallbackFocusRequested();
        }
        triggerItem = null;
        focusRestorationPending = false;
    }

    bodyData: [
        Text {
            width: parent.width
            text: qsTrId("dialogRemoveAccountContent").arg(root.userName)
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
        },
        Text {
            width: parent.width
            visible: root.failed
            text: qsTrId("errorDeletingAccount")
            color: IKColors.statusStrongWarning
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.emphasized
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.AlertMessage
        }
    ]

    footerData: [
        IKModalButton {
            id: keepAccountButton

            Layout.fillWidth: true
            role: IKModalButton.Primary
            text: qsTrId("buttonKeepAccount")
            actionEnabled: !root.busy
            onClicked: root.close()
        },
        IKModalButton {
            Layout.fillWidth: true
            role: IKModalButton.DestructiveSecondary
            text: qsTrId("buttonLogOut")
            actionEnabled: !root.busy
            busy: root.busy
            onClicked: {
                root.failed = false;
                root.busy = true;
                root.controller.disconnectUser(root.userDbId);
            }
        }
    ]

    Connections {
        target: root.controller

        function onDisconnectSucceeded(disconnectedUserDbId) {
            if (disconnectedUserDbId === root.userDbId) {
                root.busy = false;
                root.close();
            }
        }

        function onDisconnectFailed(failedUserDbId) {
            if (failedUserDbId === root.userDbId) {
                root.busy = false;
                root.failed = true;
            }
        }
    }
}
