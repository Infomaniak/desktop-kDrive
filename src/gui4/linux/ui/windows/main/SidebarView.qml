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
import kDrive.UI

Rectangle {
    id: root

    required property var appRouter
    required property var controller

    readonly property int currentTab: appRouter.currentMainTabIndex
    readonly property int tabHome: AppRouter.Home
    readonly property int tabActivities: AppRouter.Activities
    readonly property int tabStorage: AppRouter.Storage

    color: IKColors.surfaceSecondary

    Column {
        anchors.fill: parent
        anchors.margins: IKSpacing.s16
        spacing: IKSpacing.s16

        SyncSelectorView {
            width: parent.width
            controller: root.controller
        }

        Column {
            width: parent.width
            spacing: IKSpacing.s4

            IKSidebarItem {
                width: parent.width
                iconSource: "qrc:/assets/main/house.svg"
                label: qsTrId("tabTitleHome")
                selected: root.currentTab === root.tabHome
                onTriggered: root.appRouter.navigateToMainTab(root.tabHome)
            }

            IKSidebarItem {
                width: parent.width
                iconSource: "qrc:/assets/main/activities.svg"
                label: qsTrId("tabTitleActivities")
                selected: root.currentTab === root.tabActivities
                badgeCount: root.controller.currentErrorCount
                onTriggered: root.appRouter.navigateToMainTab(root.tabActivities)
            }

            IKSidebarItem {
                width: parent.width
                iconSource: "qrc:/assets/main/storage.svg"
                label: qsTrId("tabTitleStorage")
                selected: root.currentTab === root.tabStorage
                onTriggered: root.appRouter.navigateToMainTab(root.tabStorage)
            }

            IKSidebarItem {
                width: parent.width
                enabled: root.controller.canOpenCurrentSyncFolder
                iconSource: "qrc:/assets/main/folder.svg"
                label: qsTrId("buttonOpenFolder")
                onTriggered: root.controller.openCurrentSyncFolder()
            }
        }
    }
}
