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
    readonly property bool blackListReady: !root.controller.loading && !root.controller.loadFailed

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
                text: qsTrId("selectFoldersToSyncDescription")
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
                treeModel: root.controller.folderTreeModel
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
                    onClicked: root.controller.retry()
                }
            }
        }

        SettingsGroup {
            Layout.fillWidth: true

            RowLayout {
                width: parent.width
                height: IKSettings.rowHeight
                spacing: IKSettings.rowSpacing

                Text {
                    Layout.fillWidth: true
                    visible: root.controller.saveFailed
                    text: qsTrId("unexpectedErrorTeachingTipContent")
                    textFormat: Text.PlainText
                    color: IKColors.statusStrongWarning
                    font.pixelSize: IKFonts.subheadlineSize
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    Accessible.role: Accessible.AlertMessage
                }

                Item {
                    Layout.fillWidth: true
                    visible: !root.controller.saveFailed
                }

                IKModalButton {
                    role: IKModalButton.Tonal
                    text: qsTrId("buttonCancel")
                    actionEnabled: !root.controller.saving
                    onClicked: root.closeRequested()
                }

                IKModalButton {
                    role: IKModalButton.Primary
                    text: qsTrId("buttonSave")
                    actionEnabled: root.controller.canSave
                    busy: root.controller.saving
                    onClicked: root.controller.save()
                }
            }
        }
    }

    Connections {
        target: root.controller

        function onSaved() {
            root.closeRequested();
        }
    }
}
