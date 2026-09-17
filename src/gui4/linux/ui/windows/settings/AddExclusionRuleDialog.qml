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
import QtQuick.Layouts
import kDrive.UI

IKModal {
    id: root

    required property var controller
    property Item triggerItem: null
    readonly property bool canSubmit: patternField.text.length > 0 && controller.ready

    function toggleNotification() {
        notifyCheckBox.checkState = notifyCheckBox.checkState === Qt.Checked ? Qt.Unchecked : Qt.Checked;
    }

    function showFrom(trigger) {
        triggerItem = trigger;
        patternField.clear();
        notifyCheckBox.checkState = Qt.Unchecked;
        open();
    }

    title: qsTrId("dialogNewExclusionRuleTitle")
    escapeDismissible: !controller.saving
    initialFocusItem: patternField
    onDismissRequested: {
        if (!controller.saving) {
            close();
        }
    }
    onClosed: {
        if (triggerItem && triggerItem.enabled && triggerItem.visible) {
            triggerItem.forceActiveFocus(Qt.BacktabFocusReason);
        }
    }

    Connections {
        target: root.controller

        function onRuleAdded() {
            if (root.opened) {
                root.close();
            }
        }
    }

    bodyData: [
        Text {
            width: parent.width
            text: qsTrId("excludeRuleDescription")
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
        },
        TextField {
            id: patternField

            width: parent.width
            implicitHeight: IKSettings.textFieldHeight
            placeholderText: qsTrId("filesToExclude")
            color: IKColors.textPrimary
            placeholderTextColor: IKColors.textTertiary
            font.pixelSize: IKFonts.bodySize
            enabled: !root.controller.saving
            selectByMouse: true
            leftPadding: IKSpacing.s12
            rightPadding: IKSpacing.s12
            Accessible.name: qsTrId("filesToExclude")
            Keys.onReturnPressed: event => {
                if (root.canSubmit && !root.controller.saving) {
                    root.controller.addRule(text, notifyCheckBox.checkState === Qt.Checked);
                    event.accepted = true;
                }
            }

            background: Rectangle {
                radius: IKRadius.r6
                color: IKColors.surfacePrimary
                border.width: patternField.activeFocus ? 2 : 1
                border.color: patternField.activeFocus ? IKColors.accentPrimary : IKColors.settingsDivider
            }
        },
        RowLayout {
            width: parent.width
            spacing: IKSpacing.s8

            IKCheckBox {
                id: notifyCheckBox

                Layout.preferredWidth: implicitWidth
                Layout.preferredHeight: implicitHeight
                enabled: !root.controller.saving
                Accessible.name: qsTrId("notifyOnFileExcluded")
                onClicked: root.toggleNotification()
            }

            Text {
                Layout.fillWidth: true
                text: qsTrId("notifyOnFileExcluded")
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                wrapMode: Text.WordWrap

                MouseArea {
                    anchors.fill: parent
                    enabled: notifyCheckBox.enabled
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.toggleNotification()
                }
            }
        },
        Text {
            width: parent.width
            visible: root.controller.errorText.length > 0
            text: root.controller.errorText
            color: IKColors.statusStrongWarning
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.AlertMessage
        }
    ]

    footerData: [
        IKModalButton {
            Layout.fillWidth: root.actionsStacked
            text: qsTrId("buttonCancel")
            role: IKModalButton.Secondary
            actionEnabled: !root.controller.saving
            onClicked: root.close()
        },
        IKModalButton {
            Layout.fillWidth: root.actionsStacked
            text: qsTrId("buttonAddFileExclusionRule")
            actionEnabled: root.canSubmit
            busy: root.controller.saving
            onClicked: root.controller.addRule(patternField.text, notifyCheckBox.checkState === Qt.Checked)
        }
    ]
}
