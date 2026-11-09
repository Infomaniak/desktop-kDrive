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

// Pause/Resume menu of the toolbar with several synchronizations: the selected synchronization and its action, then the
// actions on every synchronization with the number of synchronizations they concern. Actions without target are hidden.
Popup {
    id: root

    required property var syncControl
    required property var sidebarController
    required property Item returnFocusItem

    enum ActionKind {
        ToggleSelected,
        PauseAll,
        ResumeAll
    }

    readonly property bool selectedShowsResume: root.syncControl.selectedState === SyncControl.Resume
    readonly property bool selectedActionVisible: root.syncControl.selectedState === SyncControl.Pause
                                                  || root.selectedShowsResume
    readonly property var entries: buildEntries()
    readonly property bool hasActions: root.syncControl.multipleSyncs && root.entries.length > 0
    // Action of the row that last had the focus, kept when the rows are rebuilt.
    property int focusedAction: -1

    function buildEntries() {
        const result = [];
        if (root.selectedActionVisible) {
            result.push({
                action: SyncControlMenu.ToggleSelected,
                label: root.selectedShowsResume ? qsTrId("menuSyncResume") : qsTrId("menuSyncPause"),
                icon: root.selectedShowsResume ? "qrc:/assets/main/home/play.svg" : "qrc:/assets/main/home/pause.svg",
                count: -1,
                separatorBefore: false
            });
        }
        // The actions on every synchronization are separated from the selected synchronization and its header.
        let separatorBefore = true;
        if (root.syncControl.pauseAllCount > 0) {
            result.push({
                action: SyncControlMenu.PauseAll,
                label: qsTrId("menuSyncPauseAll"),
                icon: "qrc:/assets/main/home/pause.svg",
                count: root.syncControl.pauseAllCount,
                separatorBefore: separatorBefore
            });
            separatorBefore = false;
        }
        if (root.syncControl.resumeAllCount > 0) {
            result.push({
                action: SyncControlMenu.ResumeAll,
                label: qsTrId("menuSyncResumeAll"),
                icon: "qrc:/assets/main/home/play.svg",
                count: root.syncControl.resumeAllCount,
                separatorBefore: separatorBefore
            });
        }
        return result;
    }

    function triggerAction(action) {
        root.close();
        switch (action) {
        case SyncControlMenu.ToggleSelected:
            root.syncControl.toggleSelected();
            break;
        case SyncControlMenu.PauseAll:
            root.syncControl.pauseAll();
            break;
        case SyncControlMenu.ResumeAll:
            root.syncControl.resumeAll();
            break;
        }
    }

    function focusAction(index) {
        if (actionsList.count === 0) {
            return;
        }
        actionsList.currentIndex = Math.max(0, Math.min(actionsList.count - 1, index));
        Qt.callLater(function () {
            if (actionsList.currentItem) {
                actionsList.currentItem.forceActiveFocus();
            }
        });
    }

    // A status or count change rebuilds the rows: the focus stays on the same action, or moves to the first one.
    function restoreFocus() {
        if (!root.opened) {
            return;
        }
        const index = root.entries.findIndex(entry => entry.action === root.focusedAction);
        root.focusAction(index >= 0 ? index : 0);
    }

    width: IKMainWindow.syncControlMenuWidth
    padding: IKSpacing.s4
    focus: true
    modal: true
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onHasActionsChanged: {
        if (!root.hasActions) {
            root.close();
        }
    }
    onEntriesChanged: Qt.callLater(root.restoreFocus)
    onOpened: {
        root.focusedAction = -1;
        root.focusAction(0);
    }
    onClosed: {
        if (root.returnFocusItem) {
            root.returnFocusItem.forceActiveFocus();
        }
    }

    background: Rectangle {
        radius: IKRadius.r12
        color: IKColors.toolbarMenuSurface
        border.width: 1
        border.color: IKColors.toolbarMenuDivider
    }

    contentItem: Column {
        spacing: 0

        Item {
            id: header

            width: parent.width
            height: IKMainWindow.syncControlMenuItemHeight
            Accessible.role: Accessible.StaticText
            Accessible.name: root.sidebarController.currentTitle

            IKSyncIcon {
                id: headerIcon

                anchors.left: parent.left
                anchors.leftMargin: IKSpacing.s8
                anchors.verticalCenter: parent.verticalCenter
                width: IKMainWindow.syncControlMenuIconSize
                height: IKMainWindow.syncControlMenuIconSize
                advancedSync: root.sidebarController.currentEntryType === SyncSelectorModel.AdvancedSync
                driveColor: root.sidebarController.currentDriveColor
            }

            Text {
                id: headerTitle

                anchors.left: headerIcon.right
                anchors.leftMargin: IKMainWindow.syncControlMenuIconSpacing
                anchors.right: parent.right
                anchors.rightMargin: IKSpacing.s8
                anchors.verticalCenter: parent.verticalCenter
                text: root.sidebarController.currentTitle
                textFormat: Text.PlainText
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                font.weight: IKFonts.medium
                elide: Text.ElideRight
            }

            HoverHandler {
                id: headerHover
            }

            IKToolTip {
                text: root.sidebarController.currentTitle
                showRequested: headerHover.hovered && headerTitle.truncated
            }
        }

        ListView {
            id: actionsList

            width: parent.width
            height: contentHeight
            interactive: false
            model: root.entries

            delegate: ItemDelegate {
                id: actionItem

                required property var modelData
                required property int index
                readonly property bool hasCount: modelData.count >= 0

                width: actionsList.width
                height: IKMainWindow.syncControlMenuItemHeight
                        + (modelData.separatorBefore ? IKMainWindow.syncControlMenuSeparatorHeight : 0)
                topPadding: modelData.separatorBefore ? IKMainWindow.syncControlMenuSeparatorHeight : 0
                leftPadding: IKSpacing.s8
                rightPadding: IKSpacing.s8
                bottomPadding: 0
                focusPolicy: Qt.StrongFocus
                hoverEnabled: true
                onClicked: root.triggerAction(modelData.action)
                onActiveFocusChanged: {
                    if (activeFocus) {
                        root.focusedAction = modelData.action;
                    }
                }
                Keys.onUpPressed: root.focusAction(actionItem.index - 1)
                Keys.onDownPressed: root.focusAction(actionItem.index + 1)

                contentItem: Item {
                    IKTintedIcon {
                        id: actionIcon

                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: IKMainWindow.syncControlMenuIconSize
                        height: IKMainWindow.syncControlMenuIconSize
                        source: actionItem.modelData.icon
                        color: IKColors.textPrimary
                    }

                    Text {
                        anchors.left: actionIcon.right
                        anchors.leftMargin: IKMainWindow.syncControlMenuIconSpacing
                        anchors.right: countLabel.left
                        anchors.rightMargin: actionItem.hasCount ? IKMainWindow.syncControlMenuIconSpacing : 0
                        anchors.verticalCenter: parent.verticalCenter
                        text: actionItem.modelData.label
                        color: IKColors.textPrimary
                        font.pixelSize: IKFonts.bodySize
                        font.weight: IKFonts.medium
                        elide: Text.ElideRight
                    }

                    Text {
                        id: countLabel

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: actionItem.hasCount ? implicitWidth : 0
                        visible: actionItem.hasCount
                        text: actionItem.hasCount ? String(actionItem.modelData.count) : ""
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.bodySize
                        font.weight: IKFonts.regular
                    }
                }

                background: Item {
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin: Math.floor(IKMainWindow.syncControlMenuSeparatorHeight / 2)
                        height: 1
                        color: IKColors.toolbarMenuDivider
                        visible: actionItem.modelData.separatorBefore
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: IKMainWindow.syncControlMenuItemHeight
                        radius: IKRadius.r6
                        color: actionItem.hovered || actionItem.down || actionItem.visualFocus ? IKColors.toolbarMenuHover
                                                                                               : "transparent"
                        border.width: actionItem.visualFocus ? 2 : 0
                        border.color: IKColors.accentPrimary
                    }
                }

                Accessible.role: Accessible.MenuItem
                Accessible.name: actionItem.hasCount ? actionItem.modelData.label + ", " + actionItem.modelData.count
                                                     : actionItem.modelData.label
            }
        }
    }
}
