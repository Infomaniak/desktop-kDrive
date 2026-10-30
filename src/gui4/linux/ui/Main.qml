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
import "dialogs"
import "windows/main"
import "windows/onboarding"
import "windows/waiting"

IKShadowedWindow {
    id: mainWindow

    required property var settingsController
    required property var appRouter
    required property var activitiesController
    required property var homeController
    required property var mainSidebarController
    required property var manyDeletesController
    required property var onboardingSessionManager
    required property var storageController
    required property var searchController
    required property var systemTrayController

    // A session being replaced keeps the onboarding route: no other route may load between the two sessions.
    readonly property bool onboardingActive: onboardingSessionManager.activeSession !== null
                                             || onboardingSessionManager.sessionReplacementPending
    readonly property bool manyDeletesPresentationAllowed: !onboardingActive
                                                           && appRouter.mainWindowActive
                                                           && visible
    readonly property bool waitingActive: !onboardingActive && !appRouter.mainWindowActive
    readonly property bool mainShellActive: !onboardingActive && appRouter.mainWindowActive
    readonly property bool tabNavigationActive: mainShellActive && appRouter.currentMainTabIndex !== AppRouter.BlockingError

    visible: false
    contentWidth: 900
    contentHeight: 600
    minimumContentWidth: 800
    minimumContentHeight: 520
    title: onboardingSessionManager.activeSession ? onboardingSessionManager.activeSession.flowController.title : "kDrive"
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
            anchors.leftMargin: IKSpacing.s16
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: IKMainWindow.sidebarWidth
        }

        MainToolbar {
            onSettingsRequested: mainWindow.settingsController.requestOpen()
            anchors.left: parent.left
            anchors.leftMargin: IKMainWindow.sidebarWidth + IKSpacing.s16
            anchors.right: parent.right
            anchors.rightMargin: IKSpacing.s8
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            appRouter: mainWindow.appRouter
            controller: mainWindow.homeController
            searchAvailable: mainWindow.searchController.available
            searchShortcutText: searchShortcut.nativeText
            settingsShortcutText: preferencesShortcut.nativeText
            onSearchRequested: mainWindow.openSearch()
        }
    }

    function openSearch() {
        if (mainWindow.searchController.available && !searchDialog.opened) {
            searchDialog.open();
        }
    }

    // Activities and Storage need a sync root, as in the sidebar. Navigation wraps around.
    function accessibleMainTabs() {
        if (!mainWindow.mainSidebarController.hasCurrentSyncRoot) {
            return [ AppRouter.Home ];
        }
        return [ AppRouter.Home, AppRouter.Activities, AppRouter.Storage ];
    }

    function navigateToAdjacentMainTab(step) {
        const tabs = accessibleMainTabs();
        const index = tabs.indexOf(mainWindow.appRouter.currentMainTabIndex);
        mainWindow.appRouter.navigateToMainTab(tabs[(Math.max(index, 0) + step + tabs.length) % tabs.length]);
    }

    function navigateToMainTabIfAccessible(tab) {
        if (accessibleMainTabs().indexOf(tab) >= 0) {
            mainWindow.appRouter.navigateToMainTab(tab);
        }
    }

    Shortcut {
        id: searchShortcut

        sequences: [ StandardKey.Find ] // Ctrl+F
        enabled: !mainWindow.onboardingActive && mainWindow.appRouter.mainWindowActive
                 && mainWindow.searchController.available
        context: Qt.WindowShortcut
        onActivated: mainWindow.openSearch()
    }

    Shortcut {
        sequences: [ StandardKey.Quit ] // Ctrl+Q
        autoRepeat: false
        context: Qt.WindowShortcut
        onActivated: globalModalHost.requestQuitConfirmation()
    }

    Shortcut {
        id: preferencesShortcut

        // GNOME binds no key to Preferences.
        sequences: [ "Ctrl+,", StandardKey.Preferences ] // KDE: Ctrl+Shift+,
        autoRepeat: false
        enabled: mainWindow.mainShellActive
        context: Qt.WindowShortcut
        onActivated: mainWindow.settingsController.requestOpen()
    }

    Shortcut {
        sequences: [ StandardKey.Open ] // Ctrl+O
        autoRepeat: false
        enabled: mainWindow.mainShellActive && mainWindow.mainSidebarController.canOpenCurrentSyncFolder
        context: Qt.WindowShortcut
        onActivated: mainWindow.mainSidebarController.openCurrentSyncFolder()
    }

    Shortcut {
        sequences: [ StandardKey.NextChild, "Ctrl+PgDown" ] // Ctrl+Tab
        enabled: mainWindow.tabNavigationActive
        context: Qt.WindowShortcut
        onActivated: mainWindow.navigateToAdjacentMainTab(1)
    }

    Shortcut {
        // StandardKey.PreviousChild only holds Ctrl+Shift+Backtab, which Qt never matches on Wayland.
        sequences: [ "Ctrl+Shift+Tab", "Ctrl+PgUp" ]
        enabled: mainWindow.tabNavigationActive
        context: Qt.WindowShortcut
        onActivated: mainWindow.navigateToAdjacentMainTab(-1)
    }

    Shortcut {
        sequence: "Alt+1"
        enabled: mainWindow.tabNavigationActive
        context: Qt.WindowShortcut
        onActivated: mainWindow.navigateToMainTabIfAccessible(AppRouter.Home)
    }

    Shortcut {
        sequence: "Alt+2"
        enabled: mainWindow.tabNavigationActive
        context: Qt.WindowShortcut
        onActivated: mainWindow.navigateToMainTabIfAccessible(AppRouter.Activities)
    }

    Shortcut {
        sequence: "Alt+3"
        enabled: mainWindow.tabNavigationActive
        context: Qt.WindowShortcut
        onActivated: mainWindow.navigateToMainTabIfAccessible(AppRouter.Storage)
    }

    Shortcut {
        sequences: [ StandardKey.Refresh ] // F5, Ctrl+R
        autoRepeat: false
        enabled: mainWindow.tabNavigationActive && mainWindow.appRouter.currentMainTabIndex === AppRouter.Storage
        context: Qt.WindowShortcut
        onActivated: mainWindow.storageController.refresh()
    }

    SearchDialog {
        id: searchDialog

        controller: mainWindow.searchController
        scrimInset: mainWindow.effectiveShadowMargin
        scrimRadius: mainWindow.surfaceRadius
    }

    // The dialog belongs to the main shell: it must not outlive it when onboarding or the waiting screen takes over.
    Connections {
        target: mainWindowLoader

        function onActiveChanged() {
            if (!mainWindowLoader.active) {
                searchDialog.close();
            }
        }
    }

    onClosing: close => {
        close.accepted = false;

        if (globalModalHost.hardDeletePending) {
            mainWindow.systemTrayController.showMainWindow();
            return;
        }

        if (mainWindow.systemTrayController.trayModeActive) {
            if (mainWindow.onboardingActive) {
                mainWindow.onboardingSessionManager.cancelActiveSession();
            } else {
                mainWindow.systemTrayController.hideMainWindow();
            }
        } else {
            globalModalHost.requestQuitConfirmation();
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

    GlobalModalHost {
        id: globalModalHost

        anchors.fill: parent
        manyDeletesController: mainWindow.manyDeletesController
        systemTrayController: mainWindow.systemTrayController
        presentationAllowed: mainWindow.manyDeletesPresentationAllowed
        surfaceInset: mainWindow.effectiveShadowMargin
        surfaceRadius: mainWindow.surfaceRadius
        targetWindow: mainWindow
        windowMoveArea: mainWindow.headerMoveArea
    }

    Component {
        id: onboardingComponent

        OnboardingWindow {
            session: onboardingLoader.session
            surfaceInset: mainWindow.effectiveShadowMargin
            surfaceRadius: mainWindow.surfaceRadius
        }
    }

    Component {
        id: mainWindowComponent

        MainWindowView {
            activitiesController: mainWindow.activitiesController
            appRouter: mainWindow.appRouter
            homeController: mainWindow.homeController
            mainSidebarController: mainWindow.mainSidebarController
            storageController: mainWindow.storageController
        }
    }

    Component {
        id: waitingComponent

        WaitingView {}
    }
}
