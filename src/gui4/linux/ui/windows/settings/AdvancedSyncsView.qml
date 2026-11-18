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
import QtQuick.Controls
import kDrive.UI

// Settings "Advanced sync" page of one drive: its advanced synchronizations, and the entry point to add one.
ScrollView {
    id: root

    required property var controller
    required property var driveDbId
    // Synchronization whose card is unfolded, scrolled to and focused once it exists, when Settings opens on it.
    property var revealSyncDbId: null
    readonly property string navigationTitle: qsTrId("advancedSyncTitle")
    // Screen of the user action log (see UserActionLog).
    readonly property string screenId: "settings.accounts.advancedSyncs"

    signal manageRequested(Item trigger, var syncDbId)
    signal deleteRequested(Item trigger, var syncDbId)
    signal addSyncRequested(Item trigger)

    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    function revealSync(syncDbId) {
        root.revealSyncDbId = syncDbId;
        root.revealPendingSync();
    }

    function revealPendingSync() {
        if (root.revealSyncDbId === null) {
            return;
        }

        for (let index = 0; index < syncsRepeater.count; ++index) {
            const card = syncsRepeater.itemAt(index) as AdvancedSyncCard;
            if (card && card.syncDbId === root.revealSyncDbId) {
                root.revealSyncDbId = null;
                card.expanded = true;
                // Waits for the unfolded card to be laid out before scrolling to it.
                Qt.callLater(() => {
                    const flickable = root.contentItem as Flickable;
                    const cardY = card.mapToItem(flickable.contentItem, 0, 0).y;
                    flickable.contentY = Math.min(Math.max(0, cardY - IKSettings.pageMargin),
                                                  Math.max(0, flickable.contentHeight - flickable.height));
                    card.focusHeader();
                });
                return;
            }
        }
    }

    Component.onCompleted: {
        root.controller.open(root.driveDbId);
        root.revealPendingSync();
    }
    Component.onDestruction: root.controller.close(root.driveDbId)

    Column {
        width: root.availableWidth
        padding: IKSettings.pageMargin
        spacing: IKSettings.driveManagementSectionSpacing

        Column {
            width: parent.width - 2 * IKSettings.pageMargin
            spacing: IKSpacing.s4

            Text {
                width: parent.width
                text: qsTrId("advancedSyncSubtitle")
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.emphasized
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: qsTrId("advancedSyncDescription")
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                wrapMode: Text.WordWrap
            }
        }

        Column {
            width: parent.width - 2 * IKSettings.pageMargin
            visible: !root.controller.empty
            spacing: IKSettings.groupSpacing

            Repeater {
                id: syncsRepeater

                model: root.controller.model
                onItemAdded: root.revealPendingSync()

                delegate: AdvancedSyncCard {
                    width: parent.width
                    driveColor: root.controller.driveColor
                    onOpenLocalFolderRequested: root.controller.openLocalFolder(syncDbId)
                    onOpenRemoteFolderRequested: root.controller.openRemoteFolder(syncDbId)
                    onRetryBlackListRequested: root.controller.reloadBlackList(syncDbId)
                    onManageRequested: trigger => root.manageRequested(trigger, syncDbId)
                    onDeleteRequested: trigger => root.deleteRequested(trigger, syncDbId)
                }
            }
        }

        IKModalButton {
            id: addSyncButton

            role: IKModalButton.Tonal
            text: qsTrId("buttonAddAdvancedSync")
            actionId: root.screenId + ".add"
            onClicked: root.addSyncRequested(addSyncButton)
        }
    }
}
