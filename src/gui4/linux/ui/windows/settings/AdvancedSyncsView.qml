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
    readonly property string navigationTitle: qsTrId("advancedSyncTitle")

    signal manageRequested(Item trigger, var syncDbId)
    signal deleteRequested(Item trigger, var syncDbId)
    signal addSyncRequested(Item trigger)

    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    Component.onCompleted: root.controller.open(root.driveDbId)
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
                model: root.controller.model

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
            onClicked: root.addSyncRequested(addSyncButton)
        }
    }
}
