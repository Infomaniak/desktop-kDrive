/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import kDrive.UI

Popup {
    id: root

    required property var controller
    required property string rowId
    required property int availableActions
    required property Item returnFocusItem

    enum ActionKind {
        OpenLocal,
        OpenOnline,
        CopyShareLink,
        FixErrors
    }

    readonly property var entries: buildEntries()

    function hasAction(actionFlag) {
        return (root.availableActions & actionFlag) !== 0;
    }

    function buildEntries() {
        const result = [];
        if (hasAction(ActivityListModel.OpenLocalAction)) {
            result.push({
                action: ActivityOptionsMenu.OpenLocal,
                label: qsTrId("buttonOpenInExplorer"),
                icon: "qrc:/assets/main/activities/open-local.svg",
                iconWidth: 16,
                iconHeight: 16,
                enabled: true,
                separatorBefore: false
            });
        }
        if (hasAction(ActivityListModel.OpenOnlineAction)) {
            result.push({
                action: ActivityOptionsMenu.OpenOnline,
                label: qsTrId("buttonOpenInKDrive"),
                icon: "qrc:/assets/main/home/kdrive-folders-stacked.svg",
                iconWidth: 14,
                iconHeight: 14,
                enabled: true,
                separatorBefore: false
            });
        }
        if (hasAction(ActivityListModel.CopyShareLinkAction)) {
            result.push({
                action: ActivityOptionsMenu.CopyShareLink,
                label: qsTrId("buttonCopyShareLink"),
                icon: "qrc:/assets/main/activities/share.svg",
                iconWidth: 16,
                iconHeight: 15,
                enabled: !root.controller.shareLinkCopyPending,
                separatorBefore: result.length > 0
            });
        }
        if (hasAction(ActivityListModel.FixErrorsAction)) {
            result.push({
                action: ActivityOptionsMenu.FixErrors,
                label: qsTrId("buttonFixErrors"),
                icon: "qrc:/assets/main/activities/fix-errors.svg",
                iconWidth: 14,
                iconHeight: 13.125,
                enabled: true,
                separatorBefore: false
            });
        }
        return result;
    }

    function triggerAction(action) {
        root.close();
        switch (action) {
        case ActivityOptionsMenu.OpenLocal:
            root.controller.openLocal(root.rowId);
            break;
        case ActivityOptionsMenu.OpenOnline:
            root.controller.openOnline(root.rowId);
            break;
        case ActivityOptionsMenu.CopyShareLink:
            root.controller.copyShareLink(root.rowId);
            break;
        case ActivityOptionsMenu.FixErrors:
            root.controller.requestFixErrors(root.rowId);
            break;
        }
    }

    function focusAction(index, direction) {
        let nextIndex = Math.max(0, Math.min(actionsList.count - 1, index));
        while (nextIndex >= 0 && nextIndex < actionsList.count && !root.entries[nextIndex].enabled) {
            nextIndex += direction;
        }
        if (nextIndex < 0 || nextIndex >= actionsList.count) {
            return;
        }
        actionsList.currentIndex = nextIndex;
        Qt.callLater(function () {
            if (actionsList.currentItem) {
                actionsList.currentItem.forceActiveFocus();
            }
        });
    }

    width: IKActivities.actionMenuWidth
    height: actionsList.contentHeight + 2 * IKSpacing.s4
    padding: IKSpacing.s4
    focus: true
    modal: true
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: {
        root.focusAction(0, 1);
    }
    onClosed: {
        if (root.returnFocusItem) {
            root.returnFocusItem.forceActiveFocus();
        }
    }

    background: Rectangle {
        radius: IKRadius.r12
        color: IKColors.activitiesActionMenuSurface
        border.width: 1
        border.color: IKColors.activitiesDivider
    }

    contentItem: ListView {
        id: actionsList

        interactive: false
        model: root.entries

        delegate: ItemDelegate {
            id: actionItem

            required property var modelData
            required property int index

            width: actionsList.width
            height: IKActivities.actionMenuItemHeight + (modelData.separatorBefore ? IKActivities.actionMenuSeparatorHeight : 0)
            topPadding: modelData.separatorBefore ? IKActivities.actionMenuSeparatorHeight : 0
            leftPadding: IKSpacing.s8
            rightPadding: IKSpacing.s8
            bottomPadding: 0
            enabled: modelData.enabled
            focusPolicy: enabled ? Qt.StrongFocus : Qt.NoFocus
            hoverEnabled: enabled
            onClicked: root.triggerAction(modelData.action)
            Keys.onUpPressed: root.focusAction(actionItem.index - 1, -1)
            Keys.onDownPressed: root.focusAction(actionItem.index + 1, 1)

            contentItem: Row {
                spacing: IKSpacing.s8

                Item {
                    anchors.verticalCenter: parent.verticalCenter
                    width: IKActivities.actionMenuIconSlotSize
                    height: IKActivities.actionMenuIconSlotSize

                    IKTintedIcon {
                        anchors.centerIn: parent
                        width: actionItem.modelData.iconWidth
                        height: actionItem.modelData.iconHeight
                        source: actionItem.modelData.icon
                        color: actionItem.enabled ? IKColors.textPrimary : IKColors.actionDisabled
                    }
                }

                Text {
                    width: Math.max(0, parent.width - x)
                    anchors.verticalCenter: parent.verticalCenter
                    text: actionItem.modelData.label
                    color: actionItem.enabled ? IKColors.textPrimary : IKColors.actionDisabled
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.medium
                    elide: Text.ElideRight
                }
            }

            background: Item {
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: actionItem.modelData.separatorBefore ? 1 : 0
                    anchors.topMargin: actionItem.modelData.separatorBefore ? IKSpacing.s4 : 0
                    anchors.leftMargin: IKSpacing.s16
                    color: IKColors.activitiesDivider
                    visible: actionItem.modelData.separatorBefore
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: IKActivities.actionMenuItemHeight
                    radius: IKRadius.r6
                    color: actionItem.hovered || actionItem.down || actionItem.visualFocus ? IKColors.activitiesActionMenuHover : "transparent"
                    border.width: actionItem.visualFocus ? 2 : 0
                    border.color: IKColors.accentPrimary
                }
            }

            Accessible.name: actionItem.modelData.label
        }
    }
}
