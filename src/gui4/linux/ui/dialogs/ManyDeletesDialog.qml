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
import QtQuick.Controls
import QtQuick.Layouts
import kDrive.UI

IKModal {
    id: root

    enum PendingAction {
        None,
        Restore,
        DeleteOnline
    }

    required property var controller
    required property bool presentationAllowed
    readonly property bool hardLimit: controller.severity === ManyDeletesController.Hard
    property int pendingAction: ManyDeletesDialog.None
    readonly property bool softLimit: controller.severity === ManyDeletesController.Soft
    readonly property real requiredActionsWidth: {
        if (softLimit) {
            return openTrashButton.implicitWidth + closeButton.implicitWidth + IKModalTokens.actionSpacing;
        }
        return deleteOnlineButton.implicitWidth + restoreButton.implicitWidth + IKModalTokens.actionSpacing;
    }

    function synchronizePresentation() {
        if (!controller.visible || !presentationAllowed) {
            root.close();
            return;
        }

        doNotShowAgainCheckBox.checked = false;
        root.pendingAction = ManyDeletesDialog.None;
        if (!root.opened) {
            root.open();
        } else {
            Qt.callLater(function () {
                if (root.initialFocusItem) {
                    root.initialFocusItem.forceActiveFocus();
                }
            });
        }
    }

    actionsStacked: requiredActionsWidth > availableWidth
    escapeDismissible: softLimit && !controller.busy
    iconColor: hardLimit ? IKColors.modalHardWarningIcon : IKColors.statusMediumWarning
    iconSource: "qrc:/assets/main/triangle-alert.svg"
    initialFocusItem: softLimit ? closeButton : restoreButton
    title: qsTrId("manyDeleteDialogTitle").arg(controller.itemCount)

    bodyData: [
        Text {
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            lineHeight: IKFonts.title2Size
            lineHeightMode: Text.FixedHeight
            text: root.hardLimit ? qsTrId("manyDeleteDialogHardLimitContent") : qsTrId("manyDeleteDialogSoftLimitContent")
            width: parent.width
            wrapMode: Text.WordWrap
        },
        CheckBox {
            id: doNotShowAgainCheckBox

            focusPolicy: Qt.StrongFocus
            hoverEnabled: true
            padding: 0
            spacing: IKSpacing.s8
            text: qsTrId("manyDeleteDialogSoftLimitDoNotShowAgain")
            visible: root.softLimit
            width: parent.width

            contentItem: Text {
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                leftPadding: doNotShowAgainCheckBox.indicator.width + doNotShowAgainCheckBox.spacing
                text: doNotShowAgainCheckBox.text
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
            }
            indicator: Rectangle {
                border.color: doNotShowAgainCheckBox.visualFocus ? IKColors.accentPrimary : IKColors.textTertiary
                border.width: doNotShowAgainCheckBox.visualFocus ? IKModalTokens.focusBorderWidth : IKModalTokens.borderWidth
                color: doNotShowAgainCheckBox.checked ? IKColors.actionPrimary : "transparent"
                implicitHeight: IKModalTokens.checkboxSize
                implicitWidth: IKModalTokens.checkboxSize
                radius: IKRadius.r4
                x: doNotShowAgainCheckBox.leftPadding
                y: Math.round((doNotShowAgainCheckBox.height - height) / 2)

                Text {
                    anchors.centerIn: parent
                    color: IKColors.actionOnPrimary
                    font.pixelSize: IKFonts.subheadlineSize
                    font.weight: IKFonts.emphasized
                    text: "✓"
                    visible: doNotShowAgainCheckBox.checked
                }
            }
        },
        Rectangle {
            color: IKColors.statusLightWarning
            implicitHeight: visible ? errorContent.implicitHeight + 2 * IKModalTokens.errorPadding : 0
            radius: IKRadius.r8
            visible: root.hardLimit && root.controller.submissionFailed
            width: parent.width

            Row {
                id: errorContent

                anchors.left: parent.left
                anchors.leftMargin: IKModalTokens.errorPadding
                anchors.right: parent.right
                anchors.rightMargin: IKModalTokens.errorPadding
                anchors.verticalCenter: parent.verticalCenter
                spacing: IKSpacing.s8

                IKTintedIcon {
                    anchors.top: parent.top
                    color: IKColors.statusStrongWarning
                    height: IKModalTokens.errorIconSize
                    source: "qrc:/assets/main/triangle-alert.svg"
                    width: IKModalTokens.errorIconSize
                }
                Column {
                    spacing: IKSpacing.s4
                    width: Math.max(0, parent.width - x)

                    Text {
                        color: IKColors.textPrimary
                        font.pixelSize: IKFonts.bodySize
                        font.weight: IKFonts.emphasized
                        text: qsTrId("defaultErrorTitle")
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.subheadlineSize
                        text: qsTrId("unexpectedErrorTeachingTipContent")
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    ]
    footerData: [
        IKModalButton {
            id: openTrashButton

            Layout.fillWidth: root.actionsStacked
            actionEnabled: root.controller.canOpenTrash && !root.controller.busy
            role: IKModalButton.Secondary
            text: qsTrId("buttonOpenTrash")
            visible: root.softLimit

            onClicked: root.controller.openTrash(doNotShowAgainCheckBox.checked)
        },
        IKModalButton {
            id: closeButton

            Layout.fillWidth: root.actionsStacked
            actionEnabled: !root.controller.busy
            role: IKModalButton.Primary
            text: qsTrId("buttonClose")
            visible: root.softLimit

            onClicked: root.controller.dismissSoft(doNotShowAgainCheckBox.checked)
        },
        IKModalButton {
            id: deleteOnlineButton

            Layout.fillWidth: root.actionsStacked
            actionEnabled: !root.controller.busy
            busy: root.controller.busy && root.pendingAction === ManyDeletesDialog.DeleteOnline
            role: IKModalButton.Destructive
            text: qsTrId("manyDeleteDialogHardLimitSecondary")
            visible: root.hardLimit

            onClicked: {
                root.pendingAction = ManyDeletesDialog.DeleteOnline;
                root.controller.deleteOnline();
            }
        },
        IKModalButton {
            id: restoreButton

            Layout.fillWidth: root.actionsStacked
            actionEnabled: !root.controller.busy
            busy: root.controller.busy && root.pendingAction === ManyDeletesDialog.Restore
            role: IKModalButton.Primary
            text: qsTrId("manyDeleteDialogHardLimitPrimary")
            visible: root.hardLimit

            onClicked: {
                root.pendingAction = ManyDeletesDialog.Restore;
                root.controller.restoreFiles();
            }
        }
    ]

    Component.onCompleted: synchronizePresentation()
    onDismissRequested: controller.dismissSoft(doNotShowAgainCheckBox.checked)
    onPresentationAllowedChanged: synchronizePresentation()

    Connections {
        function onBusyChanged() {
            if (!root.controller.busy) {
                root.pendingAction = ManyDeletesDialog.None;
            }
        }
        function onPresentationChanged() {
            root.synchronizePresentation();
        }

        target: root.controller
    }
}
