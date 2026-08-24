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
import QtQuick.Layouts
import kDrive.UI

Rectangle {
    id: root

    required property var appRouter
    required property var controller
    readonly property int currentTab: appRouter.currentMainTabIndex
    readonly property int tabActivities: AppRouter.Activities
    readonly property int tabHome: AppRouter.Home
    readonly property int tabStorage: AppRouter.Storage
    readonly property alias notification: sidebarNotification

    color: IKColors.surfaceSecondary

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: IKSpacing.s16
        spacing: IKSpacing.s16

        SyncSelectorView {
            controller: root.controller
            Layout.fillWidth: true
        }
        Column {
            Layout.fillWidth: true
            spacing: IKSpacing.s4

            IKSidebarItem {
                iconSource: "qrc:/assets/main/house.svg"
                label: qsTrId("tabTitleHome")
                selected: root.currentTab === root.tabHome
                width: parent.width

                onTriggered: root.appRouter.navigateToMainTab(root.tabHome)
            }
            IKSidebarItem {
                enabled: root.controller.hasCurrentSyncRoot
                iconSource: "qrc:/assets/main/activities.svg"
                label: qsTrId("tabTitleActivities")
                notificationColor: IKColors.statusMediumWarning
                notificationDot: root.controller.currentErrorCount > 0
                selected: root.currentTab === root.tabActivities
                width: parent.width

                onTriggered: root.appRouter.navigateToMainTab(root.tabActivities)
            }
            IKSidebarItem {
                enabled: root.controller.hasCurrentSyncRoot
                iconSource: "qrc:/assets/main/storage.svg"
                label: qsTrId("tabTitleStorage")
                selected: root.currentTab === root.tabStorage
                width: parent.width

                onTriggered: root.appRouter.navigateToMainTab(root.tabStorage)
            }
            IKSidebarItem {
                enabled: root.controller.canOpenCurrentSyncFolder
                iconSource: "qrc:/assets/main/folder.svg"
                label: qsTrId("buttonOpenFolder")
                width: parent.width

                onTriggered: root.controller.openCurrentSyncFolder()
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }

        SidebarNotification {
            id: sidebarNotification

            Layout.fillWidth: true
        }
    }
}
