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
import QtQuick.Dialogs
import QtQuick.Layouts
import kDrive.UI

IKShadowedWindow {
    id: root

    // Prefix of the user action log ids of this file (see UserActionLog).
    readonly property string actionIdPrefix: "settings"

    enum Category {
        General,
        Accounts,
        Advanced
    }

    required property var controller
    required property var users
    required property var excludedFolders
    required property var advancedSyncs
    property int selectedCategory: SettingsWindow.Category.General
    property Item accountConnectionTrigger: null
    property bool restoreAccountConnectionFocus: false
    // Dialog of the running "Sync a folder with kDrive" session, created for that session only.
    property AddAdvancedSyncDialog addAdvancedSyncDialog: null
    property Item advancedSyncCreationTrigger: null
    readonly property SettingsNavigationPane currentNavigationPane: {
        switch (selectedCategory) {
        case SettingsWindow.Category.Accounts:
            return accountsPane;
        case SettingsWindow.Category.Advanced:
            return advancedPane;
        default:
            return null;
        }
    }

    function selectGeneral() {
        accountsPane.reset(accountsRootComponent);
        advancedPane.reset(advancedRootComponent);
        selectedCategory = SettingsWindow.Category.General;
    }

    function selectAccounts() {
        advancedPane.reset(advancedRootComponent);
        selectedCategory = SettingsWindow.Category.Accounts;
        users.refresh();
    }

    // `pushedComponent`, when given, opens over the Advanced root (file exclusions).
    function selectAdvanced(pushedComponent) {
        accountsPane.reset(accountsRootComponent);
        advancedPane.reset(advancedRootComponent, pushedComponent);
        selectedCategory = SettingsWindow.Category.Advanced;
    }

    // The Settings window does not block the main window, which can request another page behind these dialogs:
    // confirming one after that navigation would delete the synchronization of another drive.
    function closeSyncDeletionDialogs() {
        deleteSyncDialog.closeWithoutRestoringFocus();
        advancedSyncDeleteDialog.closeWithoutRestoringFocus();
    }

    // Prepares a navigation requested from outside the window. Returns whether to animate it: only when Accounts is
    // already displayed.
    function prepareAccountsTarget() {
        const animated = root.visible && root.selectedCategory === SettingsWindow.Category.Accounts;
        // Selecting Accounts again would request the available drives once more; a hidden window requests them when it
        // is shown.
        if (root.selectedCategory !== SettingsWindow.Category.Accounts) {
            root.selectAccounts();
        }
        root.closeSyncDeletionDialogs();
        return animated;
    }

    // Shows the page requested from outside the window, such as the Home avatar: the card of a user, the management
    // page of a drive, or the advanced synchronizations of a drive with one of them focused.
    function showAccountsTarget(userDbId, driveDbId, syncDbId) {
        const animated = root.prepareAccountsTarget();
        if (userDbId !== 0) {
            accountsPane.showAboveRoot([], animated,
                                       () => (accountsPane.rootItem() as UsersSettingsView).revealUser(userDbId));
            return;
        }

        // Already on the requested page: only focus the synchronization again, without replaying the navigation. A
        // queued navigation is older than this request, which must replace it through showAboveRoot.
        const currentPage = accountsPane.navigationPending ? null : accountsPane.currentItem;
        if (syncDbId === 0 && accountsPane.depth === 2 && currentPage instanceof DriveManagementView
                && currentPage.driveDbId === driveDbId) {
            return;
        }
        if (syncDbId !== 0 && accountsPane.depth === 3 && currentPage instanceof AdvancedSyncsView
                && currentPage.driveDbId === driveDbId) {
            (currentPage as AdvancedSyncsView).revealSync(syncDbId);
            return;
        }

        const pages = [{"component": driveManagementComponent, "properties": {"driveDbId": driveDbId}}];
        if (syncDbId !== 0) {
            pages.push({"component": advancedSyncsComponent,
                        "properties": {"driveDbId": driveDbId, "revealSyncDbId": syncDbId}});
        }
        accountsPane.showAboveRoot(pages, animated);
    }

    // Shows "Manage synchronization" above the pages the user would have crossed to reach it, so Back leads to them.
    function showSyncManagement(driveDbId, syncDbId, advancedSync) {
        const animated = root.prepareAccountsTarget();

        // Already on the requested page: replaying the navigation would drop its draft. A queued navigation is older
        // than this request, which must replace it through showAboveRoot.
        const currentPage = accountsPane.navigationPending ? null : accountsPane.currentItem;
        if (currentPage instanceof ExcludedFoldersView && currentPage.syncDbId === syncDbId) {
            return;
        }

        const pages = [{"component": driveManagementComponent, "properties": {"driveDbId": driveDbId}}];
        if (advancedSync) {
            pages.push({"component": advancedSyncsComponent, "properties": {"driveDbId": driveDbId}});
        }
        pages.push({"component": excludedFoldersComponent,
                    "properties": advancedSync ? {"syncDbId": syncDbId, "advancedSync": true} : {"syncDbId": syncDbId}});
        accountsPane.showAboveRoot(pages, animated);
    }

    // Screen of the user action log: the General page or the current page of the Accounts or Advanced pane.
    readonly property string currentScreenId: {
        switch (selectedCategory) {
        case SettingsWindow.Category.Accounts:
            return accountsPane.currentScreenId || root.actionIdPrefix + ".accounts";
        case SettingsWindow.Category.Advanced:
            return advancedPane.currentScreenId || root.actionIdPrefix + ".advanced";
        default:
            return root.actionIdPrefix + ".general";
        }
    }
    property string loggedScreenId: ""

    // Called later, so a navigation that resets a pane before changing the category logs its final screen only.
    function logScreen() {
        if (!root.visible) {
            root.loggedScreenId = "";
            return;
        }
        if (root.currentScreenId !== root.loggedScreenId) {
            root.loggedScreenId = root.currentScreenId;
            UserActionLog.screen(root.currentScreenId);
        }
    }

    onCurrentScreenIdChanged: Qt.callLater(root.logScreen)

    windowId: "settings"
    visible: false
    transientParent: null
    modality: Qt.NonModal
    title: qsTrId("settingsTitle") + " - kDrive"
    contentWidth: IKSettings.windowWidth
    contentHeight: IKSettings.windowHeight
    minimumContentWidth: IKSettings.minimumWidth
    minimumContentHeight: IKSettings.minimumHeight
    customShadowEnabled: true
    windowTitleVisible: false
    Component.onCompleted: root.controller.takePendingTarget()
    onVisibleChanged: {
        Qt.callLater(root.logScreen);
        if (visible && selectedCategory === SettingsWindow.Category.Accounts) {
            users.refresh();
            if (root.restoreAccountConnectionFocus) {
                Qt.callLater(function() {
                    if (root.accountConnectionTrigger && root.accountConnectionTrigger.enabled
                            && root.accountConnectionTrigger.visible) {
                        root.accountConnectionTrigger.forceActiveFocus(Qt.BacktabFocusReason);
                    } else if (accountsPane.currentItem && accountsPane.currentItem.focusConnectButton) {
                        accountsPane.currentItem.focusConnectButton();
                    }
                    root.accountConnectionTrigger = null;
                    root.restoreAccountConnectionFocus = false;
                });
            }
        }
    }

    headerBackgroundData: Rectangle {
        width: IKSettings.sidebarWidth
        height: parent.height
        color: IKColors.surfaceSecondary
    }

    headerData: Item {
        anchors.fill: parent

        SidebarHeaderView {
            anchors.left: parent.left
            anchors.leftMargin: IKSpacing.s16
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: IKSettings.sidebarWidth - IKSpacing.s16
            title: qsTrId("settingsTitle")
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: IKSettings.sidebarWidth + IKSettings.pageMargin
            anchors.right: parent.right
            anchors.rightMargin: IKSettings.pageMargin
            anchors.verticalCenter: parent.verticalCenter
            visible: root.selectedCategory === SettingsWindow.Category.General
            text: qsTrId("sidebarItemGeneral")
            font.pixelSize: IKFonts.headlineSize
            font.weight: IKFonts.emphasized
            color: IKColors.textPrimary
            elide: Text.ElideRight
        }

        SettingsNavigationHeader {
            anchors.left: parent.left
            anchors.leftMargin: IKSettings.sidebarWidth
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            visible: root.selectedCategory === SettingsWindow.Category.Accounts
            hasPreviousPage: accountsPane.hasPreviousPage
            currentTitle: accountsPane.currentTitle
            previousTitle: accountsPane.previousTitle
            onBackRequested: {
                UserActionLog.action(root.actionIdPrefix + ".navigation.back", { from: root.currentScreenId });
                accountsPane.pop();
            }
        }

        SettingsNavigationHeader {
            anchors.left: parent.left
            anchors.leftMargin: IKSettings.sidebarWidth
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            visible: root.selectedCategory === SettingsWindow.Category.Advanced
            hasPreviousPage: advancedPane.hasPreviousPage
            currentTitle: advancedPane.currentTitle
            previousTitle: advancedPane.previousTitle
            onBackRequested: {
                UserActionLog.action(root.actionIdPrefix + ".navigation.back", { from: root.currentScreenId });
                advancedPane.pop();
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: IKSettings.sidebarWidth
            Layout.fillHeight: true
            color: IKColors.surfaceSecondary

            Column {
                anchors.fill: parent
                anchors.margins: IKSpacing.s8

                IKSidebarItem {
                    width: parent.width
                    label: qsTrId("sidebarItemGeneral")
                    iconSource: "qrc:/assets/settings/general.svg"
                    selected: root.selectedCategory === SettingsWindow.Category.General
                    notificationDot: root.controller.general.updateAvailable
                    actionId: root.actionIdPrefix + ".sidebar.general"
                    onTriggered: root.selectGeneral()
                }

                IKSidebarItem {
                    width: parent.width
                    label: qsTrId("sidebarItemAccounts")
                    iconSource: "qrc:/assets/settings/user-sync.svg"
                    selected: root.selectedCategory === SettingsWindow.Category.Accounts
                    actionId: root.actionIdPrefix + ".sidebar.accounts"
                    onTriggered: root.selectAccounts()
                }

                IKSidebarItem {
                    width: parent.width
                    label: qsTrId("sidebarItemAdvanced")
                    iconSource: "qrc:/assets/settings/advanced.svg"
                    selected: root.selectedCategory === SettingsWindow.Category.Advanced
                    actionId: root.actionIdPrefix + ".sidebar.advanced"
                    onTriggered: root.selectAdvanced()
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            GeneralSettingsView {
                anchors.fill: parent
                visible: root.selectedCategory === SettingsWindow.Category.General
                controller: root.controller.general
                onReleaseInformationRequested: trigger => releaseDialog.showFrom(trigger)
                onAboutRequested: trigger => aboutDialog.showFrom(trigger)
            }

            SettingsNavigationPane {
                id: accountsPane

                anchors.fill: parent
                visible: root.selectedCategory === SettingsWindow.Category.Accounts
                initialItem: accountsRootComponent
                onFocusRestorationRequested: target => {
                    if (target && target.enabled && target.visible) {
                        target.forceActiveFocus(Qt.BacktabFocusReason);
                    }
                }
            }

            SettingsNavigationPane {
                id: advancedPane

                anchors.fill: parent
                visible: root.selectedCategory === SettingsWindow.Category.Advanced
                initialItem: advancedRootComponent
                onFocusRestorationRequested: target => {
                    if (target && target.enabled && target.visible) {
                        target.forceActiveFocus(Qt.BacktabFocusReason);
                    }
                }
            }
        }
    }

    Component {
        id: accountsRootComponent

        UsersSettingsView {
            controller: root.users
            activationController: root.controller.syncActivation
            onConnectAccountRequested: trigger => {
                root.accountConnectionTrigger = trigger;
                root.restoreAccountConnectionFocus = true;
                root.controller.requestAccountConnection();
            }
            onDisconnectAccountRequested: (trigger, userDbId, userName) =>
                                          disconnectAccountDialog.showFrom(trigger, userDbId, userName)
            onActivateDriveRequested: (trigger, userDbId, accountId, driveId) => {
                syncConfigurationDialog.returnFocusItem = trigger;
                root.controller.syncActivation.activate(userDbId, accountId, driveId);
            }
            onManageDriveRequested: (trigger, driveDbId) => {
                trigger.forceActiveFocus();
                accountsPane.push(driveManagementComponent, {"driveDbId": driveDbId});
            }
        }
    }

    Component {
        id: driveManagementComponent

        DriveManagementView {
            id: driveManagementView

            controller: root.controller.driveManagement
            activationController: root.controller.syncActivation
            onActivateRequested: trigger => {
                syncConfigurationDialog.returnFocusItem = trigger;
                const driveManagement = root.controller.driveManagement;
                root.controller.syncActivation.activate(driveManagement.userDbId, driveManagement.accountId,
                                                        driveManagement.driveId);
            }
            onDeleteRequested: trigger => deleteSyncDialog.showFrom(trigger)
            onManageSynchronizationRequested: trigger => {
                trigger.forceActiveFocus();
                accountsPane.push(excludedFoldersComponent,
                                  {"syncDbId": root.controller.driveManagement.mainSyncDbId});
            }
            onAdvancedSyncsRequested: trigger => {
                trigger.forceActiveFocus();
                accountsPane.push(advancedSyncsComponent, {"driveDbId": driveManagementView.driveDbId});
            }
        }
    }

    Component {
        id: advancedSyncsComponent

        AdvancedSyncsView {
            id: advancedSyncsView

            controller: root.advancedSyncs
            onManageRequested: (trigger, syncDbId) => {
                trigger.forceActiveFocus();
                accountsPane.push(excludedFoldersComponent, {"syncDbId": syncDbId, "advancedSync": true});
            }
            onDeleteRequested: (trigger, syncDbId) => {
                advancedSyncDeleteDialog.syncDbId = syncDbId;
                advancedSyncDeleteDialog.showFrom(trigger);
            }
            onAddSyncRequested: trigger => {
                root.advancedSyncCreationTrigger = trigger;
                root.advancedSyncs.openCreation();
            }
        }
    }

    Component {
        id: excludedFoldersComponent

        ExcludedFoldersView {
            id: excludedFoldersView

            // Set for an advanced synchronization, whose summary belongs to the advanced sync page.
            property bool advancedSync: false

            controller: root.excludedFolders
            // The page may have saved a new blacklist: the custom-selection label reads it again.
            Component.onDestruction: {
                if (excludedFoldersView.advancedSync) {
                    root.advancedSyncs.reloadBlackList(excludedFoldersView.syncDbId);
                } else {
                    root.controller.driveManagement.reloadBlackList();
                }
            }
            onCloseRequested: {
                if (accountsPane.currentItem === excludedFoldersView) {
                    accountsPane.pop();
                }
            }
        }
    }

    Component {
        id: advancedRootComponent

        AdvancedSettingsView {
            onFileExclusionsRequested: trigger => {
                trigger.forceActiveFocus();
                advancedPane.push(fileExclusionsComponent);
            }
            onDataManagementRequested: trigger => {
                trigger.forceActiveFocus();
                advancedPane.push(dataManagementComponent);
            }
            onDebugRequested: trigger => {
                trigger.forceActiveFocus();
                advancedPane.push(debugComponent);
            }
            onNetworkRequested: trigger => {
                trigger.forceActiveFocus();
                advancedPane.push(networkComponent);
            }
        }
    }

    Component {
        id: fileExclusionsComponent

        FileExclusionsView {
            controller: root.controller.fileExclusions
            onAddRuleRequested: trigger => addExclusionRuleDialog.showFrom(trigger)
        }
    }

    Component {
        id: dataManagementComponent

        DataManagementView {
            controller: root.controller.advanced
            onMatomoRequested: trigger => {
                trigger.forceActiveFocus();
                advancedPane.push(matomoComponent);
            }
            onSentryRequested: trigger => {
                trigger.forceActiveFocus();
                advancedPane.push(sentryComponent);
            }
        }
    }

    Component {
        id: matomoComponent

        TrackingConsentView {
            controller: root.controller.advanced
            navigationTitle: "Matomo"
            screenId: root.actionIdPrefix + ".advanced.dataManagement.matomo"
            logoSource: ThemeMode.isDark ? "qrc:/assets/settings/matomo-logo-dark.svg" : "qrc:/assets/settings/matomo-logo.svg"
            description: qsTrId("matomoDescription")
            errorText: root.controller.advanced.matomoErrorText
            trackingEnabled: root.controller.advanced.matomoEnabled
            toggleAction: value => root.controller.advanced.setMatomoEnabled(value)
        }
    }

    Component {
        id: sentryComponent

        TrackingConsentView {
            controller: root.controller.advanced
            navigationTitle: "Sentry"
            screenId: root.actionIdPrefix + ".advanced.dataManagement.sentry"
            logoSource: ThemeMode.isDark ? "qrc:/assets/settings/sentry-logo-dark.svg" : "qrc:/assets/settings/sentry-logo.svg"
            description: qsTrId("sentryDescription")
            errorText: root.controller.advanced.sentryErrorText
            trackingEnabled: root.controller.advanced.sentryEnabled
            toggleAction: value => root.controller.advanced.setSentryEnabled(value)
        }
    }

    Component {
        id: networkComponent

        NetworkSettingsView {
            controller: root.controller.network
            onConnectionFailureRequested: trigger => proxyConnectionFailureDialog.showFrom(trigger)
        }
    }

    Component {
        id: debugComponent

        DebugSettingsView {
            controller: root.controller.advanced
            onSendLogsRequested: trigger => sendDebugLogsDialog.showFrom(trigger)
        }
    }

    SettingsInformationDialog {
        id: releaseDialog

        dialogId: root.actionIdPrefix + ".general.releaseInfoDialog"
        controller: root.controller.general
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
    }

    SettingsInformationDialog {
        id: aboutDialog

        dialogId: root.actionIdPrefix + ".general.aboutDialog"
        controller: root.controller.general
        about: true
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
    }

    AddExclusionRuleDialog {
        id: addExclusionRuleDialog

        controller: root.controller.fileExclusions
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
    }

    SendDebugLogsDialog {
        id: sendDebugLogsDialog

        controller: root.controller.advanced
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
    }

    ProxyConnectionFailureDialog {
        id: proxyConnectionFailureDialog

        controller: root.controller.network
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
    }

    DisconnectAccountDialog {
        id: disconnectAccountDialog

        controller: root.users
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
        onFallbackFocusRequested: {
            if (accountsPane.currentItem && accountsPane.currentItem.focusConnectButton) {
                accountsPane.currentItem.focusConnectButton();
            }
        }
    }

    SyncConfigurationDialog {
        id: syncConfigurationDialog

        dialogId: root.actionIdPrefix + ".syncActivation"
        controller: root.controller.syncActivation
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
        onFallbackFocusRequested: {
            if (accountsPane.currentItem && accountsPane.currentItem.focusConnectButton) {
                accountsPane.currentItem.focusConnectButton();
            }
        }
    }

    DeleteSyncDialog {
        id: deleteSyncDialog

        dialogId: root.actionIdPrefix + ".accounts.driveManagement.deleteSyncDialog"
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
        busy: root.controller.driveManagement.deletePending
        onDeleteConfirmed: root.controller.driveManagement.deleteMainSync()
        onFallbackFocusRequested: {
            if (accountsPane.currentItem) {
                accountsPane.currentItem.forceActiveFocus(Qt.BacktabFocusReason);
            }
        }

        Connections {
            target: root.controller.driveManagement
            enabled: deleteSyncDialog.opened

            function onDeleteSucceeded() {
                deleteSyncDialog.close();
            }

            function onDeleteFailed() {
                deleteSyncDialog.failed = true;
            }

            function onDriveRemoved() {
                deleteSyncDialog.close();
            }
        }
    }

    Component {
        id: addAdvancedSyncDialogComponent

        AddAdvancedSyncDialog {
            // The dialog targets the drive of the advanced syncs page.
            driveColor: root.advancedSyncs.driveColor
            scrimInset: root.effectiveShadowMargin
            scrimRadius: root.surfaceRadius
            onReleased: root.advancedSyncs.releaseCreation()
            onFallbackFocusRequested: {
                if (accountsPane.currentItem) {
                    accountsPane.currentItem.forceActiveFocus(Qt.BacktabFocusReason);
                }
            }
        }
    }

    FolderDialog {
        id: advancedSyncLocalFolderDialog

        title: qsTrId("buttonSelectFolder")
        // The chosen folder is a local path: it is never logged.
        onAccepted: {
            UserActionLog.action(root.actionIdPrefix + ".accounts.advancedSyncs.addDialog.folderDialog.accept");
            root.advancedSyncs.creation?.applyLocalFolder(selectedFolder);
            root.advancedSyncs.creation?.notifyLocalFolderDialogClosed();
        }
        onRejected: {
            UserActionLog.action(root.actionIdPrefix + ".accounts.advancedSyncs.addDialog.folderDialog.reject");
            root.advancedSyncs.creation?.notifyLocalFolderDialogClosed();
        }
    }

    Connections {
        target: root.advancedSyncs

        // The controller is an initial property, not a binding: the dialog never sees it turn null.
        function onCreationChanged() {
            const creation = root.advancedSyncs.creation;
            if (creation) {
                root.addAdvancedSyncDialog = addAdvancedSyncDialogComponent.createObject(root, {
                    "controller": creation,
                    "returnFocusItem": root.advancedSyncCreationTrigger
                });
            } else {
                advancedSyncLocalFolderDialog.close();
                if (root.addAdvancedSyncDialog) {
                    root.addAdvancedSyncDialog.destroy();
                    root.addAdvancedSyncDialog = null;
                }
            }
            root.advancedSyncCreationTrigger = null;
        }
    }

    // "Manage exclusions" on the errors page: Advanced, then its file exclusions page.
    Connections {
        target: root.controller

        function onFileExclusionsRequested() {
            root.selectAdvanced(fileExclusionsComponent);
        }

        function onAccountsRequested() {
            accountsPane.reset(accountsRootComponent);
            root.selectAccounts();
        }
    }

    Connections {
        target: root.advancedSyncs.creation

        function onLocalFolderRequested(initialFolder) {
            advancedSyncLocalFolderDialog.currentFolder = initialFolder;
            advancedSyncLocalFolderDialog.open();
        }
    }

    DeleteSyncDialog {
        id: advancedSyncDeleteDialog

        property var syncDbId: 0

        dialogId: root.actionIdPrefix + ".accounts.advancedSyncs.deleteSyncDialog"
        dialogDetails: ({ syncDbId: advancedSyncDeleteDialog.syncDbId })
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
        busy: root.advancedSyncs.deletePending
        onDeleteConfirmed: root.advancedSyncs.deleteSync(advancedSyncDeleteDialog.syncDbId)
        onFallbackFocusRequested: {
            if (accountsPane.currentItem) {
                accountsPane.currentItem.forceActiveFocus(Qt.BacktabFocusReason);
            }
        }

        Connections {
            target: root.advancedSyncs
            enabled: advancedSyncDeleteDialog.opened

            function onDeleteSucceeded(syncDbId) {
                if (syncDbId === advancedSyncDeleteDialog.syncDbId) {
                    advancedSyncDeleteDialog.close();
                }
            }

            function onDeleteFailed(syncDbId) {
                if (syncDbId === advancedSyncDeleteDialog.syncDbId) {
                    advancedSyncDeleteDialog.failed = true;
                }
            }
        }

        Connections {
            target: root.controller.driveManagement
            enabled: advancedSyncDeleteDialog.opened

            // Deleting the last synchronization of a drive without a main one returns to the Accounts root.
            function onDriveRemoved() {
                advancedSyncDeleteDialog.close();
            }
        }
    }

    Connections {
        target: root.controller.driveManagement

        // The managed drive lost its last synchronization: its page has nothing left to manage.
        function onDriveRemoved() {
            accountsPane.reset(accountsRootComponent);
        }
    }

    FolderDialog {
        id: localFolderDialog

        title: qsTrId("buttonSelectFolder")
        // The chosen folder is a local path: it is never logged.
        onAccepted: {
            UserActionLog.action(syncConfigurationDialog.dialogId + ".drive.folderDialog.accept");
            root.controller.syncActivation.applyCustomFolder(selectedFolder);
            root.controller.syncActivation.notifyCustomFolderDialogClosed();
        }
        onRejected: {
            UserActionLog.action(syncConfigurationDialog.dialogId + ".drive.folderDialog.reject");
            root.controller.syncActivation.notifyCustomFolderDialogClosed();
        }
    }

    Connections {
        target: root.controller

        function onTargetPending() {
            root.controller.takePendingTarget();
        }

        function onUserShowRequested(userDbId) {
            root.showAccountsTarget(userDbId, 0, 0);
        }

        function onDriveShowRequested(driveDbId) {
            root.showAccountsTarget(0, driveDbId, 0);
        }

        function onAdvancedSyncShowRequested(driveDbId, syncDbId) {
            root.showAccountsTarget(0, driveDbId, syncDbId);
        }

        function onSyncManagementShowRequested(driveDbId, syncDbId, advancedSync) {
            root.showSyncManagement(driveDbId, syncDbId, advancedSync);
        }
    }

    Connections {
        target: root.controller.syncActivation

        function onCustomFolderRequested(initialFolder) {
            localFolderDialog.currentFolder = initialFolder;
            localFolderDialog.open();
        }

        function onVisibleChanged() {
            if (!root.controller.syncActivation.visible) {
                localFolderDialog.close();
            }
        }
    }

    Shortcut {
        sequences: [ StandardKey.Quit ] // Ctrl+Q
        autoRepeat: false
        context: Qt.WindowShortcut
        onActivated: {
            UserActionLog.action("shortcut.quit", { window: "settings" });
            root.controller.requestQuitConfirmation();
        }
    }

    // Same entry points as the sidebar. Navigation wraps around.
    function selectCategory(category) {
        switch (category) {
        case SettingsWindow.Category.General:
            root.selectGeneral();
            break;
        case SettingsWindow.Category.Accounts:
            root.selectAccounts();
            break;
        case SettingsWindow.Category.Advanced:
            root.selectAdvanced();
            break;
        }
    }

    function selectAdjacentCategory(step) {
        const categoryCount = 3;
        root.selectCategory((root.selectedCategory + step + categoryCount) % categoryCount);
    }

    Shortcut {
        sequences: [ StandardKey.NextChild, "Ctrl+PgDown" ] // Ctrl+Tab
        context: Qt.WindowShortcut
        onActivated: {
            UserActionLog.action("shortcut.nextTab", { window: "settings" });
            root.selectAdjacentCategory(1);
        }
    }

    Shortcut {
        // StandardKey.PreviousChild only holds Ctrl+Shift+Backtab, which Qt never matches on Wayland.
        sequences: [ "Ctrl+Shift+Tab", "Ctrl+PgUp" ]
        context: Qt.WindowShortcut
        onActivated: {
            UserActionLog.action("shortcut.previousTab", { window: "settings" });
            root.selectAdjacentCategory(-1);
        }
    }

    Shortcut {
        sequence: "Alt+1"
        context: Qt.WindowShortcut
        onActivated: {
            UserActionLog.action("shortcut.settingsGeneral");
            root.selectCategory(SettingsWindow.Category.General);
        }
    }

    Shortcut {
        sequence: "Alt+2"
        context: Qt.WindowShortcut
        onActivated: {
            UserActionLog.action("shortcut.settingsAccounts");
            root.selectCategory(SettingsWindow.Category.Accounts);
        }
    }

    Shortcut {
        sequence: "Alt+3"
        context: Qt.WindowShortcut
        onActivated: {
            UserActionLog.action("shortcut.settingsAdvanced");
            root.selectCategory(SettingsWindow.Category.Advanced);
        }
    }

    Shortcut {
        sequences: [ StandardKey.Back ] // Alt+Left
        autoRepeat: false
        enabled: root.currentNavigationPane !== null && root.currentNavigationPane.canGoBack
        context: Qt.WindowShortcut
        onActivated: {
            UserActionLog.action("shortcut.back", { from: root.currentScreenId });
            root.currentNavigationPane.pop();
        }
    }

    onClosing: {
        root.controller.network.cancelConnectionCheck();
        root.controller.network.dismissConnectionFailure();
        addExclusionRuleDialog.close();
        sendDebugLogsDialog.close();
        proxyConnectionFailureDialog.close();
        disconnectAccountDialog.close();
        deleteSyncDialog.close();
        advancedSyncDeleteDialog.close();
        localFolderDialog.close();
        advancedSyncLocalFolderDialog.close();
        root.advancedSyncs.releaseCreation();
        root.controller.syncActivation.dismissFromHostWindow();
        releaseDialog.close();
        aboutDialog.close();
    }
}
