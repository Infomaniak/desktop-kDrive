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

Item {
    id: root

    required property var appRouter
    required property var mainSidebarController

    readonly property int tabHome: AppRouter.Home
    readonly property int tabActivities: AppRouter.Activities
    readonly property int tabStorage: AppRouter.Storage
    readonly property int tabBlockingError: AppRouter.BlockingError
    readonly property real sidebarWidth: IKMainWindow.sidebarWidth
    readonly property int currentTab: appRouter.currentMainTabIndex

    Row {
        anchors.fill: parent

        SidebarView {
            width: root.sidebarWidth
            height: parent.height
            appRouter: root.appRouter
            controller: root.mainSidebarController
        }

        Rectangle {
            width: Math.max(0, parent.width - root.sidebarWidth)
            height: parent.height
            color: IKColors.surfacePrimary

            Item {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                HomePlaceholder {
                    anchors.fill: parent
                    visible: root.currentTab === root.tabHome
                }

                ActivitiesPlaceholder {
                    anchors.fill: parent
                    visible: root.currentTab === root.tabActivities
                }

                StoragePlaceholder {
                    anchors.fill: parent
                    visible: root.currentTab === root.tabStorage
                }

                BlockingErrorPlaceholder {
                    anchors.fill: parent
                    visible: root.currentTab === root.tabBlockingError
                }
            }
        }
    }
}
