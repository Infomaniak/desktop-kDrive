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

Item {
    id: root

    required property var controller
    required property var syncDbId
    readonly property string navigationTitle: qsTrId("titleManageSynchronization")
    // Screen of the user action log (see UserActionLog).
    readonly property string screenId: "settings.accounts.excludedFolders"
    readonly property bool blackListReady: !root.controller.loading && !root.controller.loadFailed
    readonly property bool footerErrorShown: root.controller.saveFailed || root.controller.excludedFolderLimitExceeded
    readonly property bool selectedSizeShown: !root.footerErrorShown && root.controller.selectedSizeText.length > 0
    // Scrim geometry of the settings window, for the deletion dialog.
    property real modalScrimInset: 0
    property real modalScrimRadius: 0

    // Cancel and the back chevron both drop the draft: only Save publishes it.
    signal closeRequested

    Component.onCompleted: root.controller.open(root.syncDbId)
    Component.onDestruction: root.controller.close(root.syncDbId)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: IKSettings.pageMargin
        spacing: IKSpacing.s16

        Column {
            Layout.fillWidth: true
            spacing: IKSpacing.s2

            Text {
                width: parent.width
                text: qsTrId("selectFoldersToSync")
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.emphasized
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: qsTrId("selectFoldersToSyncDescriptionShort")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                wrapMode: Text.WordWrap
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            RemoteFolderTree {
                id: folderTree

                anchors.fill: parent
                visible: root.blackListReady
                // The saved list is captured when Save is clicked: a later edit would be lost when the page closes.
                enabled: !root.controller.saving
                treeModel: root.controller.folderTreeModel
                actionIdPrefix: root.screenId + ".tree"
                onVisibleChanged: {
                    if (visible) {
                        Qt.callLater(() => folderTree.keyboardFocusItem.forceActiveFocus());
                    }
                }
            }

            IKLoadingSpinner {
                anchors.centerIn: parent
                visible: root.controller.loading
                width: IKIconSizes.large
                height: width
                color: IKColors.actionPrimary
                Accessible.role: Accessible.Indicator
                Accessible.name: qsTrId("titleManageSynchronization")
            }

            Column {
                anchors.centerIn: parent
                width: parent.width - 2 * IKSpacing.s24
                spacing: IKSpacing.s12
                visible: root.controller.loadFailed

                Text {
                    width: parent.width
                    text: qsTrId("onboardingLoginErrorDescription")
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.bodySize
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.AlertMessage
                }

                IKModalButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    role: IKModalButton.Secondary
                    text: qsTrId("buttonRetry")
                    actionId: root.screenId + ".retry"
                    onClicked: root.controller.retry()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: IKSettings.rowHeight
            radius: IKRadius.r12
            color: IKColors.settingsCardSurface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: IKSettings.groupPadding
                anchors.rightMargin: IKSettings.groupPadding
                spacing: IKSettings.rowSpacing

                Text {
                    Layout.fillWidth: true
                    visible: root.footerErrorShown
                    text: root.controller.excludedFolderLimitExceeded
                          ? qsTrId("excludedFoldersLimitReached").arg(root.controller.maxExcludedFolders)
                          : qsTrId("unexpectedErrorTeachingTipContent")
                    textFormat: Text.PlainText
                    color: IKColors.statusStrongWarning
                    font.pixelSize: IKFonts.subheadlineSize
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    Accessible.role: Accessible.AlertMessage
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.selectedSizeShown
                    text: qsTrId("manageSyncSelectedSize").arg(root.controller.selectedSizeText)
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.emphasized
                    elide: Text.ElideRight
                }

                Item {
                    Layout.fillWidth: true
                    visible: !root.footerErrorShown && !root.selectedSizeShown
                }

                IKModalButton {
                    role: IKModalButton.Tonal
                    text: qsTrId("buttonCancel")
                    actionEnabled: !root.controller.saving
                    actionId: root.screenId + ".cancel"
                    actionDetails: ({ syncDbId: root.syncDbId })
                    onClicked: root.closeRequested()
                }

                IKModalButton {
                    id: saveButton

                    role: IKModalButton.Primary
                    text: qsTrId("buttonSave")
                    actionId: root.screenId + ".save"
                    actionDetails: ({ syncDbId: root.syncDbId })
                    actionEnabled: root.controller.canSave
                    busy: root.controller.saving
                    onClicked: {
                        // Unchecked folders are deleted from this computer: the user confirms it first.
                        if (root.controller.newlyExcludedFolderCount > 0) {
                            deletionDialog.showFrom(saveButton, root.controller.newlyExcludedFolderCount);
                            return;
                        }

                        root.controller.save();
                    }
                }
            }
        }
    }

    ExcludedFoldersDeletionDialog {
        id: deletionDialog

        dialogId: root.screenId + ".deletionDialog"
        dialogDetails: ({ syncDbId: root.syncDbId, folderCount: deletionDialog.folderCount })
        scrimInset: root.modalScrimInset
        scrimRadius: root.modalScrimRadius
        onSaveConfirmed: root.controller.save()
    }

    Connections {
        target: root.controller

        function onSaved() {
            root.closeRequested();
        }
    }
}
