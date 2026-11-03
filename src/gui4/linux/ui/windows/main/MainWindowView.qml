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
import QtQuick.Window
import kDrive.UI

Item {
    id: root

    required property var appRouter
    required property var activitiesController
    required property var blockingErrorController
    required property var homeController
    required property var mainSidebarController
    required property var storageController

    readonly property int tabHome: AppRouter.Home
    readonly property int tabActivities: AppRouter.Activities
    readonly property int tabStorage: AppRouter.Storage
    readonly property int tabBlockingError: AppRouter.BlockingError
    readonly property real sidebarWidth: IKMainWindow.sidebarWidth
    readonly property int currentTab: appRouter.currentMainTabIndex
    readonly property bool windowVisible: root.Window.window !== null && root.Window.window.visible
    readonly property bool canGoBackInActivities: root.currentTab === root.tabActivities
                                                  && root.appRouter.activitiesNavigation.canGoBack

    Row {
        anchors.fill: parent

        SidebarView {
            id: sidebarView

            width: root.sidebarWidth
            height: parent.height
            appRouter: root.appRouter
            controller: root.mainSidebarController
            navigationEnabled: !root.blockingErrorController.active
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

                Loader {
                    anchors.fill: parent
                    active: root.currentTab === root.tabHome && root.windowVisible
                    sourceComponent: homeViewComponent
                }

                Loader {
                    anchors.fill: parent
                    active: root.currentTab === root.tabActivities && root.windowVisible
                    sourceComponent: activitiesViewComponent
                }

                Loader {
                    anchors.fill: parent
                    active: root.currentTab === root.tabStorage && root.windowVisible
                    sourceComponent: storageViewComponent
                }

                Loader {
                    anchors.fill: parent
                    active: root.currentTab === root.tabBlockingError && root.blockingErrorController.active
                    sourceComponent: blockingErrorViewComponent
                }
            }
        }
    }

    // Back to the parent Activities page, like the toolbar back button (Alt+Left, mouse back button).
    Shortcut {
        sequences: [ StandardKey.Back ]
        enabled: root.canGoBackInActivities
        onActivated: root.appRouter.activitiesNavigation.back()
    }

    TapHandler {
        acceptedButtons: Qt.BackButton
        enabled: root.canGoBackInActivities
        onTapped: root.appRouter.activitiesNavigation.back()
    }

    Connections {
        target: root.activitiesController

        function onShareLinkCopyStarted(rowId) {
            sidebarView.notification.showLoading(qsTrId("creatingShareLink"));
        }

        function onShareLinkCopied(rowId) {
            sidebarView.notification.showSuccess(qsTrId("linkCopiedToClipboardTitle"));
        }

        function onShareLinkCopyFailed(rowId) {
            sidebarView.notification.showError(qsTrId("failedToCreateShareLinkError"));
        }
    }

    Component {
        id: blockingErrorViewComponent

        BlockingErrorView {
            controller: root.blockingErrorController
        }
    }

    Component {
        id: homeViewComponent

        HomeView {
            controller: root.homeController
        }
    }

    Component {
        id: activitiesViewComponent

        ActivitiesView {
            controller: root.activitiesController
            navigation: root.appRouter.activitiesNavigation
        }
    }

    Component {
        id: storageViewComponent

        StorageView {
            controller: root.storageController
        }
    }
}
