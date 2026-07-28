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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

pragma ComponentBehavior: Bound

import QtQuick
import kDrive.UI
import "windows/main"
import "windows/onboarding"
import "windows/waiting"

IKShadowedWindow {
    id: mainWindow

    required property var appRouter
    required property var mainSidebarController
    required property var onboardingSessionManager
    required property var systemTrayController

    readonly property bool onboardingActive: onboardingSessionManager.activeSession !== null
    readonly property bool waitingActive: !onboardingActive && !appRouter.mainWindowActive

    visible: false
    contentWidth: 900
    contentHeight: 600
    minimumContentWidth: 720
    minimumContentHeight: 520
    title: onboardingActive ? onboardingSessionManager.activeSession.flowController.title : "kDrive"
    surfaceColor: {
        if (waitingActive) {
            return IKColors.surfaceSecondary;
        }
        if (onboardingActive) {
            return IKColors.onboardingSurfacePrimary;
        }
        return IKColors.surfacePrimary;
    }
    customShadowEnabled: true
    headerOverlaysContent: onboardingActive
    windowTitleVisible: false

    headerBackgroundData: Item {
        anchors.fill: parent

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            visible: !mainWindow.onboardingActive && mainWindow.appRouter.mainWindowActive
            width: IKMainWindow.sidebarWidth
            color: IKColors.surfaceSecondary
        }

        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            visible: mainWindow.onboardingActive
            width: parent.width * IKOnboarding.illustrationPanelWidthRatio
            color: IKColors.onboardingSurfaceSecondary
        }
    }

    headerData: Item {
        anchors.fill: parent
        visible: !mainWindow.onboardingActive && mainWindow.appRouter.mainWindowActive

        SidebarHeaderView {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: IKMainWindow.sidebarWidth
        }

        MainToolbar {
            anchors.left: parent.left
            anchors.leftMargin: IKMainWindow.sidebarWidth
            anchors.right: parent.right
            anchors.rightMargin: IKSpacing.s8
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            appRouter: mainWindow.appRouter
        }
    }

    onClosing: close => {
        if (mainWindow.systemTrayController.trayModeActive) {
            close.accepted = false;
            mainWindow.systemTrayController.hideMainWindow();
        } else {
            close.accepted = true;
            Qt.quit();
        }
    }

    Connections {
        target: mainWindow.onboardingSessionManager

        function onActiveSessionChanged() {
            if (mainWindow.onboardingSessionManager.activeSession) {
                onboardingLoader.session = mainWindow.onboardingSessionManager.activeSession;
                onboardingLoader.active = true;
            } else {
                onboardingLoader.active = false;
                onboardingLoader.session = null;
            }
        }
    }

    Loader {
        id: onboardingLoader

        anchors.fill: parent
        active: false
        sourceComponent: onboardingComponent
        property var session: null

        Component.onCompleted: {
            if (mainWindow.onboardingSessionManager.activeSession) {
                session = mainWindow.onboardingSessionManager.activeSession;
                active = true;
            }
        }
    }

    Loader {
        id: mainWindowLoader

        anchors.fill: parent
        active: mainWindow.appRouter.mainWindowActive && !mainWindow.onboardingActive
        sourceComponent: mainWindowComponent
    }

    Loader {
        anchors.fill: parent
        active: mainWindow.waitingActive
        sourceComponent: waitingComponent
    }

    Component {
        id: onboardingComponent

        OnboardingWindow {
            session: onboardingLoader.session
        }
    }

    Component {
        id: mainWindowComponent

        MainWindowView {
            appRouter: mainWindow.appRouter
            mainSidebarController: mainWindow.mainSidebarController
        }
    }

    Component {
        id: waitingComponent

        WaitingView {}
    }
}
