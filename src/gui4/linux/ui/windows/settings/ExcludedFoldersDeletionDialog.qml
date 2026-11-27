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

// Confirms a save that deletes unchecked folders from this computer. Confirming closes the dialog and emits
// `saveConfirmed`: the page saves, and shows a failure in its footer like any other save.
IKConfirmationDialog {
    id: root

    property int folderCount: 0
    property Item triggerItem: null

    signal saveConfirmed

    function showFrom(trigger, count) {
        triggerItem = trigger;
        folderCount = count;
        open();
    }

    title: qsTrId("dialogExcludedFoldersDeletionTitle", root.folderCount)
    description: qsTrId("dialogExcludedFoldersDeletionDescription", root.folderCount)
    cancelText: qsTrId("buttonCancel")
    confirmText: qsTrId("buttonDeleteAndSave")
    confirmRole: IKModalButton.DestructiveSecondary
    onConfirmed: {
        root.close();
        root.saveConfirmed();
    }
    onClosed: {
        if (root.triggerItem && root.triggerItem.enabled && root.triggerItem.visible) {
            root.triggerItem.forceActiveFocus(Qt.BacktabFocusReason);
        }

        root.triggerItem = null;
    }
}
