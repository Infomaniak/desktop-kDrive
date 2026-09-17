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
    property bool awaitingSave: false

    function showFrom(trigger) {
        triggerItem = trigger;
        awaitingSave = false;
        open();
    }

    function dismiss() {
        if (controller.saving) return;
        controller.dismissConnectionFailure();
        close();
    }

    title: qsTrId("proxyConnectionFailedTitle")
    escapeDismissible: !controller.saving
    initialFocusItem: cancelButton
    onDismissRequested: dismiss()
    onClosed: {
        awaitingSave = false;
        if (triggerItem && triggerItem.enabled && triggerItem.visible) {
            triggerItem.forceActiveFocus(Qt.BacktabFocusReason);
        }
    }

    Connections {
        target: root.controller

        function onSaveFinished() {
            if (root.awaitingSave) {
                root.awaitingSave = false;
                root.close();
            }
        }
    }

    bodyData: [
        Text {
            width: parent.width
            text: qsTrId("proxyConnectionFailedDescription")
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
        },
        Text {
            width: parent.width
            text: qsTrId("proxySaveAnywayQuestion")
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.emphasized
            wrapMode: Text.WordWrap
        }
    ]

    footerData: [
        IKModalButton {
            id: cancelButton

            Layout.fillWidth: root.actionsStacked
            text: qsTrId("buttonCancel")
            role: IKModalButton.Secondary
            actionEnabled: !root.controller.saving
            onClicked: root.dismiss()
        },
        IKModalButton {
            Layout.fillWidth: root.actionsStacked
            text: qsTrId("buttonSaveAnyway")
            actionEnabled: !root.controller.saving
            busy: root.controller.saving
            onClicked: {
                root.awaitingSave = true;
                root.controller.saveManualWithoutCheck();
            }
        }
    ]
}
