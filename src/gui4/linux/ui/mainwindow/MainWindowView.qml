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

    readonly property int tabHome: 0
    readonly property int tabActivities: 1
    readonly property int tabStorage: 2
    readonly property int tabBlockingError: 3
    readonly property real sidebarWidth: 240
    readonly property int currentTab: appRouter.currentMainTabIndex

    Row {
        anchors.fill: parent

        MainSidebarPlaceholder {
            width: root.sidebarWidth
            height: parent.height
            appRouter: root.appRouter
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
