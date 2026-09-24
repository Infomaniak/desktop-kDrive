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

pragma
ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import kDrive.UI

IKModal {
    id: root

    required property var controller
    property Item triggerItem: null
    readonly property bool statusVisible: controller.uploadInProgress || controller.uploadHasResult

    function toggleLastSessionOnly() {
        lastSessionCheckBox.checkState = lastSessionCheckBox.checkState === Qt.Checked ? Qt.Unchecked : Qt.Checked
    }

    function showFrom(trigger) {
        triggerItem = trigger;
        // Reopened during an upload: keep the option the running upload was started with.
        if (!controller.uploadInProgress) {
            lastSessionCheckBox.checkState = Qt.Unchecked;
        }
        open();
    }

    title: qsTrId("logUploadPopupTitle")
    escapeDismissible: !controller.uploadInProgress
    initialFocusItem: controller.uploadHasResult ? closeButton : lastSessionCheckBox
    onDismissRequested: close()
    onClosed: {
        if (triggerItem && triggerItem.enabled && triggerItem.visible) {
            triggerItem.forceActiveFocus(Qt.BacktabFocusReason);
        }
        if (controller.uploadHasResult) {
            controller.resetDebugLogsUploadPresentation();
        }
    }

    Connections {
        target: root.controller

        function onChanged() {
            if (root.opened && root.controller.uploadHasResult) {
                closeButton.forceActiveFocus(Qt.TabFocusReason);
            }
        }
    }

    bodyData: [
        Text {
            width: parent.width
            text: qsTrId("largeFolderRecommendation")
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
        },
        RowLayout {
            width: parent.width
            IKCheckBox {
                id: lastSessionCheckBox
                enabled: !root.controller.uploadInProgress
                Accessible.name: qsTrId("sendLastSessionOnly")
                onClicked: root.toggleLastSessionOnly()
            }
            Text {
                Layout.fillWidth: true
                text: qsTrId("sendLastSessionOnly")
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                wrapMode: Text.WordWrap

                MouseArea {
                    anchors.fill: parent
                    enabled: lastSessionCheckBox.enabled
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.toggleLastSessionOnly()
                }
            }
        },
        Item {
            width: parent.width
            implicitHeight: Math.max(IKModalTokens.iconSize, uploadStatusText.implicitHeight)

            RowLayout {
                anchors.fill: parent
                visible: root.statusVisible
                spacing: IKSpacing.s8

                IKLoadingSpinner {
                    Layout.preferredWidth: IKModalTokens.iconSize
                    Layout.preferredHeight: IKModalTokens.iconSize
                    visible: root.controller.uploadInProgress
                    Accessible.ignored: true
                }

                Text {
                    id: uploadStatusText

                    Layout.fillWidth: true
                    text: root.controller.uploadStatusText
                    color: root.controller.lastUploadFailed ? IKColors.statusStrongWarning
                        : root.controller.uploadSucceeded ? IKColors.statusMediumSuccess
                            : IKColors.textSecondary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: root.controller.uploadHasResult ? IKFonts.emphasized : Font.Normal
                    wrapMode: Text.WordWrap
                    Accessible.role: root.controller.uploadHasResult ? Accessible.AlertMessage : Accessible.StaticText
                }
            }
        }
    ]

    footerData: [
        IKModalButton {
            Layout.fillWidth: root.actionsStacked
            visible: !root.controller.uploadHasResult
            text: qsTrId("buttonCancel")
            role: IKModalButton.Secondary
            actionEnabled: !root.controller.uploadCancellationPending
            onClicked: root.controller.uploadInProgress ? root.controller.cancelDebugLogs() : root.close()
        },
        IKModalButton {
            id: closeButton

            Layout.fillWidth: root.actionsStacked
            visible: !root.controller.uploadInProgress
            text: root.controller.uploadHasResult ? qsTrId("buttonClose") : qsTrId("buttonSend")
            actionEnabled: !root.controller.uploadInProgress
            onClicked: {
                if (root.controller.uploadHasResult) {
                    root.close();
                } else {
                    root.controller.sendDebugLogs(lastSessionCheckBox.checkState === Qt.Checked);
                }
            }
        }
    ]
}
