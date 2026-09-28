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

    enum Category {
        General,
        Accounts,
        Advanced
    }

    required property var controller
    required property var users
    property int selectedCategory: SettingsWindow.Category.General
    property Item accountConnectionTrigger: null
    property bool restoreAccountConnectionFocus: false

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

    function selectAdvanced() {
        accountsPane.reset(accountsRootComponent);
        advancedPane.reset(advancedRootComponent);
        selectedCategory = SettingsWindow.Category.Advanced;
    }

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
    onVisibleChanged: {
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
            canGoBack: accountsPane.canGoBack
            currentTitle: accountsPane.currentTitle
            previousTitle: accountsPane.previousTitle
            onBackRequested: accountsPane.pop()
        }

        SettingsNavigationHeader {
            anchors.left: parent.left
            anchors.leftMargin: IKSettings.sidebarWidth
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            visible: root.selectedCategory === SettingsWindow.Category.Advanced
            canGoBack: advancedPane.canGoBack
            currentTitle: advancedPane.currentTitle
            previousTitle: advancedPane.previousTitle
            onBackRequested: advancedPane.pop()
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
                    onTriggered: root.selectGeneral()
                }

                IKSidebarItem {
                    width: parent.width
                    label: qsTrId("sidebarItemAccounts")
                    iconSource: "qrc:/assets/settings/accounts.svg"
                    selected: root.selectedCategory === SettingsWindow.Category.Accounts
                    onTriggered: root.selectAccounts()
                }

                IKSidebarItem {
                    width: parent.width
                    label: qsTrId("sidebarItemAdvanced")
                    iconSource: "qrc:/assets/settings/advanced.svg"
                    selected: root.selectedCategory === SettingsWindow.Category.Advanced
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

        controller: root.controller.general
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
    }

    SettingsInformationDialog {
        id: aboutDialog

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

        controller: root.controller.syncActivation
        scrimInset: root.effectiveShadowMargin
        scrimRadius: root.surfaceRadius
        onFallbackFocusRequested: {
            if (accountsPane.currentItem && accountsPane.currentItem.focusConnectButton) {
                accountsPane.currentItem.focusConnectButton();
            }
        }
    }

    FolderDialog {
        id: localFolderDialog

        title: qsTrId("buttonSelectFolder")
        onAccepted: {
            root.controller.syncActivation.applyCustomFolder(selectedFolder);
            root.controller.syncActivation.notifyCustomFolderDialogClosed();
        }
        onRejected: root.controller.syncActivation.notifyCustomFolderDialogClosed()
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

    onClosing: {
        root.controller.network.cancelConnectionCheck();
        root.controller.network.dismissConnectionFailure();
        addExclusionRuleDialog.close();
        sendDebugLogsDialog.close();
        proxyConnectionFailureDialog.close();
        disconnectAccountDialog.close();
        localFolderDialog.close();
        root.controller.syncActivation.dismissFromHostWindow();
        releaseDialog.close();
        aboutDialog.close();
    }
}
