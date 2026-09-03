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

import QtQuick
import QtQuick.Controls
import kDrive.UI

// Always-alive composition point for app-global dialogs. Feature-specific queueing remains in each controller; a
// cross-feature arbiter can be introduced here later if simultaneous global modal families become a real requirement.
Item {
    id: root

    required property var manyDeletesController
    required property bool presentationAllowed
    required property real surfaceInset
    required property real surfaceRadius
    required property var systemTrayController
    required property var targetWindow
    required property rect windowMoveArea
    readonly property bool modalVisible: manyDeletesDialog.visible || quitConfirmationDialog.visible
    property bool quitPending: false

    function requestQuitConfirmation() {
        root.systemTrayController.showMainWindow();

        if (quitConfirmationDialog.visible || root.quitPending) {
            return;
        }

        if (manyDeletesDialog.visible
                && root.manyDeletesController.severity === root.manyDeletesController.Hard) {
            return;
        }

        quitConfirmationDialog.open();
    }

    ManyDeletesDialog {
        id: manyDeletesDialog

        controller: root.manyDeletesController
        presentationAllowed: root.presentationAllowed && !quitConfirmationDialog.visible
        scrimInset: root.surfaceInset
        scrimRadius: root.surfaceRadius
    }

    IKConfirmationDialog {
        id: quitConfirmationDialog

        busy: root.quitPending
        cancelText: qsTrId("buttonCancel")
        confirmText: qsTrId("statusBarQuitApp")
        description: qsTrId("quitConfirmationDialogDescription")
        scrimInset: root.surfaceInset
        scrimRadius: root.surfaceRadius
        title: qsTrId("quitConfirmationDialogTitle")

        onConfirmed: {
            root.quitPending = true;
            root.systemTrayController.requestApplicationQuit();
        }
    }

    Connections {
        target: root.systemTrayController

        function onQuitConfirmationRequested() {
            root.requestQuitConfirmation();
        }
    }

    Item {
        parent: Overlay.overlay
        anchors.fill: parent
        z: Math.max(manyDeletesDialog.z, quitConfirmationDialog.z) + 1
        visible: root.modalVisible

        Item {
            id: modalWindowMoveArea

            x: root.windowMoveArea.x
            y: root.windowMoveArea.y
            width: root.windowMoveArea.width
            height: root.windowMoveArea.height

            DragHandler {
                target: null
                acceptedButtons: Qt.LeftButton
                onActiveChanged: {
                    if (active) {
                        root.targetWindow.startSystemMove()
                    }
                }
            }
        }

        IKWindowResizeHandles {
            anchors.fill: parent
            targetWindow: root.targetWindow
            surfaceInset: root.surfaceInset
            visible: root.surfaceInset > 0
        }
    }
}
