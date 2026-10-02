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

IKConfirmationDialog {
    id: root

    required property var controller
    property Item triggerItem: null
    property bool failed: false

    signal fallbackFocusRequested

    function showFrom(trigger) {
        triggerItem = trigger;
        failed = false;
        open();
    }

    title: qsTrId("dialogSyncDeletionWarningTitle")
    description: qsTrId("dialogSyncDeletionWarningContent")
    cancelText: qsTrId("buttonCancel")
    confirmText: qsTrId("buttonRemove")
    confirmRole: IKModalButton.Destructive
    busy: root.controller.deletePending
    errorText: root.failed ? qsTrId("unexpectedErrorTeachingTipContent") : ""
    onConfirmed: {
        root.failed = false;
        root.controller.deleteMainSync();
    }
    onClosed: {
        root.failed = false;
        if (root.triggerItem && root.triggerItem.enabled && root.triggerItem.visible) {
            root.triggerItem.forceActiveFocus(Qt.BacktabFocusReason);
        } else {
            root.fallbackFocusRequested();
        }
        root.triggerItem = null;
    }

    Connections {
        target: root.controller
        enabled: root.opened

        function onDeleteSucceeded() {
            root.close();
        }

        function onDeleteFailed() {
            root.failed = true;
        }

        function onDriveRemoved() {
            root.close();
        }
    }
}
