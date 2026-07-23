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
import kDrive.UI

Item {
    id: root

    required property var controller

    implicitHeight: currentSyncItem.implicitHeight

    IKSyncSelectorItem {
        id: currentSyncItem

        anchors.left: parent.left
        anchors.right: parent.right
        title: root.controller.currentTitle
        subtitle: root.controller.currentSubtitle
        driveColor: root.controller.currentDriveColor
        interactive: root.controller.syncCount > 1
        showChevron: root.controller.syncCount > 1
        onTriggered: syncPopup.open()
    }

    Popup {
        id: syncPopup

        x: 0
        y: root.height + IKSpacing.s4
        width: root.width
        height: Math.min(syncList.contentHeight + IKSpacing.s8 * 2, IKMainWindow.syncSelectorPopupMaxHeight)
        padding: IKSpacing.s8
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: Qt.callLater(function() {
            if (syncList.currentItem) {
                syncList.currentItem.forceActiveFocus()
            }
        })
        onClosed: {
            if (currentSyncItem.interactive) {
                currentSyncItem.forceActiveFocus()
            }
        }

        background: Rectangle {
            radius: IKRadius.r8
            color: IKColors.surfaceSecondary
            border.width: 1
            border.color: IKColors.surfaceTertiary
        }

        contentItem: ListView {
            id: syncList

            clip: true
            spacing: IKSpacing.s4
            model: root.controller.syncsModel
            currentIndex: root.controller.selectedRow

            onCurrentIndexChanged: {
                if (currentIndex >= 0) {
                    positionViewAtIndex(currentIndex, ListView.Contain)
                }
            }

            delegate: IKSyncSelectorItem {
                required property var model

                width: syncList.width
                title: model.title
                subtitle: model.subtitle
                driveColor: model.driveColor
                selected: model.isSelected
                onTriggered: {
                    root.controller.selectSync(model.syncDbId)
                    syncPopup.close()
                }
            }
        }
    }
}
