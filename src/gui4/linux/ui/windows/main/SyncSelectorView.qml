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

    visible: controller.entryCount > 0
    implicitHeight: visible ? currentSelectorItem.implicitHeight : 0

    IKDriveSyncSelectorItem {
        id: currentSelectorItem

        anchors.left: parent.left
        anchors.right: parent.right
        entryType: root.controller.currentEntryType
        title: root.controller.currentTitle
        subtitle: root.controller.currentSubtitle
        driveColor: root.controller.currentDriveColor
        errorCount: root.controller.currentErrorCount
        warning: root.controller.currentHasWarning
        interactive: root.controller.entryCount > 1
        showSurface: root.controller.entryCount > 1
        showChevron: root.controller.entryCount > 1
        onTriggered: selectorPopup.open()
    }

    Popup {
        id: selectorPopup

        x: 0
        y: root.height + IKSpacing.s4
        width: root.width
        height: Math.min(selectorList.contentHeight + IKSpacing.s8 * 2, IKMainWindow.syncSelectorPopupMaxHeight)
        padding: IKSpacing.s8
        focus: true
        modal: true
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: {
            if (root.controller.selectedRow >= 0) {
                selectorList.positionViewAtIndex(root.controller.selectedRow, ListView.Contain)
            }
            Qt.callLater(function() {
                if (selectorList.currentItem) {
                    selectorList.currentItem.forceActiveFocus()
                }
            })
        }
        onClosed: {
            if (currentSelectorItem.interactive) {
                currentSelectorItem.forceActiveFocus()
            }
        }

        background: Rectangle {
            radius: IKRadius.r8
            color: IKColors.surfaceSecondary
            border.width: 1
            border.color: IKColors.surfaceTertiary
        }

        contentItem: ListView {
            id: selectorList

            clip: true
            spacing: IKSpacing.s4
            model: root.controller.selectorModel
            currentIndex: root.controller.selectedRow

            onCurrentIndexChanged: {
                if (currentIndex >= 0) {
                    positionViewAtIndex(currentIndex, ListView.Contain)
                }
            }

            delegate: IKDriveSyncSelectorItem {
                required property var model

                width: selectorList.width
                entryType: model.entryType
                title: model.title
                subtitle: model.subtitle
                driveColor: model.driveColor
                errorCount: model.errorCount
                warning: model.hasWarning
                selected: model.isSelected
                showSurface: false
                onTriggered: {
                    if (model.entryType === SyncSelectorModel.DriveOnly) {
                        root.controller.selectDrive(model.driveDbId)
                    } else {
                        root.controller.selectSync(model.syncDbId)
                    }
                    selectorPopup.close()
                }
            }
        }
    }
}
