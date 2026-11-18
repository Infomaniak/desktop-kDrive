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

Rectangle {
    id: root

    signal settingsRequested
    signal searchRequested

    required property var appRouter
    required property var controller
    required property var syncControl
    // Selected synchronization of the sidebar selector, shown in the Pause/Resume menu header.
    required property var sidebarController
    property bool searchAvailable: false
    property string searchShortcutText: ""
    property string settingsShortcutText: ""

    readonly property int currentTab: root.appRouter.currentMainTabIndex
    readonly property int tabActivities: AppRouter.Activities
    readonly property int tabStorage: AppRouter.Storage
    readonly property var activitiesNavigation: root.appRouter.activitiesNavigation
    readonly property bool canGoBack: root.currentTab === root.tabActivities && root.activitiesNavigation.canGoBack
    readonly property bool syncShowsResume: root.syncControl.selectedState === SyncControl.Resume
    readonly property url syncControlIcon: root.syncShowsResume ? "qrc:/assets/main/home/play.svg"
                                                                : "qrc:/assets/main/home/pause.svg"

    color: "transparent"

    function tooltipWithShortcut(label, shortcutText) {
        return shortcutText.length > 0 ? label + " (" + shortcutText + ")" : label;
    }

    // Opens the menu below the toolbar group, centered on the Pause/Resume button and its chevron, kept inside the toolbar.
    function openSyncControlMenu() {
        const buttonCenter = syncControlButtons.mapToItem(root, syncControlButtons.width / 2, 0);
        const groupBottom = actionGroup.mapToItem(root, 0, actionGroup.height);
        syncControlMenu.x = Math.max(0, Math.min(root.width - syncControlMenu.width, buttonCenter.x - syncControlMenu.width / 2));
        syncControlMenu.y = groupBottom.y + IKMainWindow.syncControlMenuOffset;
        syncControlMenu.open();
    }

    HeaderIconButton {
        id: backButton

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        visible: root.canGoBack
        iconSource: "qrc:/assets/settings/chevron-left.svg"
        text: qsTrId("accessibilityBack")
        Accessible.description: qsTrId("tabTitleActivities")
        onClicked: root.activitiesNavigation.back()
    }

    Text {
        id: titleLabel

        anchors.left: root.canGoBack ? backButton.right : parent.left
        anchors.leftMargin: root.canGoBack ? IKSpacing.s12 : 0
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(implicitWidth, Math.max(0, actionsRow.x - x - IKSpacing.s16))
        text: {
            switch (root.currentTab) {
            case root.tabActivities:
                switch (root.activitiesNavigation.currentPage) {
                case ActivitiesNavigation.Errors:
                    return qsTrId("errorPageTitle")
                case ActivitiesNavigation.ConflictsQuick:
                case ActivitiesNavigation.ConflictsList:
                    return qsTrId("conflictErrorTitle")
                default:
                    return qsTrId("tabTitleActivities")
                }
            case root.tabStorage:
                return qsTrId("tabTitleStorage")
            default:
                return qsTrId("tabTitleHome")
            }
        }
        color: IKColors.textPrimary
        font.pixelSize: IKFonts.headlineSize
        font.weight: IKFonts.emphasized
        elide: Text.ElideRight
    }

    Row {
        id: actionsRow

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: IKSpacing.s8

        HeaderIconButton {
            iconSource: "qrc:/assets/main/home/headphones.svg"
            text: qsTrId("infomaniakSupport")
            onClicked: root.controller.openSupport()
        }

        Rectangle {
            id: actionGroup

            width: root.syncControl.multipleSyncs ? IKMainWindow.toolbarActionGroupMenuWidth
                                                  : IKMainWindow.toolbarActionGroupWidth
            height: IKMainWindow.toolbarActionGroupHeight
            radius: height / 2
            color: IKColors.toolbarControlSurface
            border.width: 1
            border.color: IKColors.surfaceTertiary

            Row {
                anchors.centerIn: parent
                spacing: IKMainWindow.toolbarActionGroupSpacing

                // The button pauses or resumes the selected synchronization. With several synchronizations, its chevron
                // opens the menu that also acts on every synchronization.
                Row {
                    id: syncControlButtons

                    GroupedIconButton {
                        enabled: root.syncControl.selectedState === SyncControl.Pause || root.syncShowsResume
                        iconSource: root.syncControlIcon
                        text: root.syncShowsResume ? qsTrId("buttonRestartSync") : qsTrId("buttonPause")
                        onClicked: root.syncControl.toggleSelected()
                    }

                    SyncMenuChevronButton {
                        id: syncMenuButton

                        visible: root.syncControl.multipleSyncs
                        enabled: root.syncControl.available
                        menuOpened: syncControlMenu.opened
                        text: qsTrId("buttonShowOption")
                        onClicked: root.openSyncControlMenu()
                    }
                }

                Item {
                    visible: root.syncControl.multipleSyncs
                    width: IKMainWindow.toolbarActionGroupSeparatorSlotWidth
                    height: IKMainWindow.toolbarActionGroupButtonSize

                    Rectangle {
                        anchors.centerIn: parent
                        width: 1
                        height: IKMainWindow.toolbarActionGroupSeparatorHeight
                        color: IKColors.toolbarActionGroupSeparator
                    }
                }

                GroupedIconButton {
                    iconSource: "qrc:/assets/main/home/cog.svg"
                    text: qsTrId("buttonSettings")
                    tooltipText: root.tooltipWithShortcut(text, root.settingsShortcutText)
                    onClicked: root.settingsRequested()
                }
            }
        }

        HeaderIconButton {
            enabled: root.searchAvailable
            iconSource: "qrc:/assets/main/home/search.svg"
            iconSize: IKMainWindow.toolbarSearchIconSize
            text: qsTrId("buttonSearch")
            tooltipText: root.tooltipWithShortcut(text, root.searchShortcutText)
            onClicked: root.searchRequested()
        }
    }

    SyncControlMenu {
        id: syncControlMenu

        syncControl: root.syncControl
        sidebarController: root.sidebarController
        returnFocusItem: syncMenuButton
    }

    component HeaderIconButton: ToolButton {
        id: buttonRoot

        required property url iconSource
        property real iconSize: IKIconSizes.medium
        property string tooltipText: text

        width: IKMainWindow.toolbarIconButtonSize
        height: IKMainWindow.toolbarIconButtonSize
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        display: AbstractButton.IconOnly

        contentItem: Item {
            IKTintedIcon {
                anchors.centerIn: parent
                width: buttonRoot.iconSize
                height: buttonRoot.iconSize
                source: buttonRoot.iconSource
                color: buttonRoot.enabled ? IKColors.textSecondary : IKColors.actionDisabled
            }
        }

        background: Rectangle {
            radius: width / 2
            color: buttonRoot.hovered || buttonRoot.down ? IKColors.toolbarControlHover : IKColors.toolbarControlSurface
            border.width: buttonRoot.visualFocus ? 2 : 1
            border.color: buttonRoot.visualFocus ? IKColors.accentPrimary : IKColors.surfaceTertiary
        }

        IKToolTip {
            targetButton: buttonRoot
            text: buttonRoot.tooltipText
        }
    }

    component GroupedIconButton: ToolButton {
        id: groupedButton

        required property url iconSource
        property string tooltipText: text

        width: IKMainWindow.toolbarActionGroupButtonSize
        height: IKMainWindow.toolbarActionGroupButtonSize
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        display: AbstractButton.IconOnly

        contentItem: IKTintedIcon {
            width: IKIconSizes.medium
            height: IKIconSizes.medium
            source: groupedButton.iconSource
            color: groupedButton.enabled ? IKColors.textSecondary : IKColors.actionDisabled
        }

        background: Rectangle {
            radius: width / 2
            color: groupedButton.hovered || groupedButton.down ? IKColors.toolbarControlHover : "transparent"
            border.width: groupedButton.visualFocus ? 2 : 0
            border.color: IKColors.accentPrimary
        }

        IKToolTip {
            targetButton: groupedButton
            text: groupedButton.tooltipText
        }
    }

    // Chevron that opens the menu of a grouped button, with its own hover surface.
    component SyncMenuChevronButton: ToolButton {
        id: chevronButton

        property bool menuOpened: false

        width: IKMainWindow.toolbarSyncMenuChevronButtonWidth
        height: IKMainWindow.toolbarActionGroupButtonSize
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        display: AbstractButton.IconOnly
        Accessible.role: Accessible.ButtonMenu

        contentItem: Item {
            IKTintedIcon {
                anchors.centerIn: parent
                width: IKMainWindow.toolbarSyncMenuChevronSize
                height: IKMainWindow.toolbarSyncMenuChevronSize
                source: "qrc:/assets/main/chevron-down.svg"
                color: chevronButton.enabled ? IKColors.textSecondary : IKColors.actionDisabled
            }
        }

        background: Rectangle {
            radius: Math.min(width, height) / 2
            color: chevronButton.hovered || chevronButton.down || chevronButton.menuOpened ? IKColors.toolbarControlHover
                                                                                           : "transparent"
            border.width: chevronButton.visualFocus ? 2 : 0
            border.color: IKColors.accentPrimary
        }

        IKToolTip {
            targetButton: chevronButton
            showRequested: !chevronButton.menuOpened && (chevronButton.hovered || chevronButton.visualFocus)
            text: chevronButton.text
        }
    }
}
