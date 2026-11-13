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
import QtQuick.Window
import kDrive.UI

Item {
    id: root

    // Prefix of the user action log ids of this file (see UserActionLog).
    readonly property string actionIdPrefix: "onboarding"

    required property var session
    // Modal scrim geometry of the hosting window: the overlay also covers the transparent custom-shadow margin, which
    // must not be dimmed because it is not part of the visible application surface.
    required property real surfaceInset
    required property real surfaceRadius
    readonly property var onboardingFlowController: session.flowController
    readonly property var driveSelectionController: session.driveSelectionController
    readonly property var drivesModel: session.availableDrivesModel
    readonly property var syncConfigurationController: session.syncConfigurationController
    // Screens of the user action log, in the order of the steps: `<actionIdPrefix>.<step>`.
    readonly property var onboardingStepNames: [ "login", "driveSelection", "synchronization", "ready" ]
    readonly property var onboardingStepComponents: [
        loginComponent,
        driveSelectionComponent,
        synchronizationComponent,
        readyComponent
    ]

    Rectangle {
        anchors.fill: parent
        color: IKColors.onboardingSurfacePrimary
    }

    // Onboarding stays loaded behind the update-required screen and while the window is in the tray: a step is logged
    // only once it is really shown, and again each time it is shown anew.
    readonly property bool shown: root.visible && root.Window.window !== null && root.Window.window.visible
    property string loggedScreenId: ""

    function logCurrentStep() {
        if (!root.shown) {
            root.loggedScreenId = "";
            return;
        }

        const stepName = root.onboardingStepNames[root.onboardingFlowController.currentStep] || "login";
        const screenId = root.actionIdPrefix + "." + stepName;
        if (screenId !== root.loggedScreenId) {
            root.loggedScreenId = screenId;
            UserActionLog.screen(screenId);
        }
    }

    onShownChanged: root.logCurrentStep()
    Component.onCompleted: logCurrentStep()

    Connections {
        target: root.onboardingFlowController

        function onCurrentStepChanged() {
            root.logCurrentStep();
        }
    }

    SyncConfigurationDialog {
        id: syncConfigurationDialog

        dialogId: "syncConfig"
        controller: root.syncConfigurationController
        scrimInset: root.surfaceInset
        scrimRadius: root.surfaceRadius
    }

    FolderDialog {
        id: localFolderDialog

        title: qsTrId("buttonSelectFolder")
        // The chosen folder is a local path: it is never logged.
        onAccepted: {
            UserActionLog.action(syncConfigurationDialog.dialogId + ".drive.folderDialog.accept")
            root.syncConfigurationController.applyCustomFolder(selectedFolder)
            root.syncConfigurationController.notifyCustomFolderDialogClosed()
        }
        onRejected: {
            UserActionLog.action(syncConfigurationDialog.dialogId + ".drive.folderDialog.reject")
            root.syncConfigurationController.notifyCustomFolderDialogClosed()
        }
    }

    Connections {
        target: root.syncConfigurationController

        function onCustomFolderRequested(initialFolder) {
            localFolderDialog.currentFolder = initialFolder
            localFolderDialog.open()
        }
    }

    Row {
        anchors.fill: parent
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 0

        Loader {
            id: contentPanel

            width: parent.width * IKOnboarding.contentPanelWidthRatio
            height: parent.height
            sourceComponent: root.onboardingStepComponents[root.onboardingFlowController.currentStep] || loginComponent
        }

        Rectangle {
            id: illustrationPanel

            width: parent.width * IKOnboarding.illustrationPanelWidthRatio
            height: parent.height
            color: IKColors.onboardingSurfaceSecondary

            OnboardingAnimationsView {
                anchors.centerIn: parent
                width: Math.min(IKOnboarding.illustrationAnimationMaxSize,
                                parent.width * IKOnboarding.illustrationAnimationFillRatio)
                height: width * contentHeightRatio
                onboardingFlowController: root.onboardingFlowController
            }
        }
    }

    Component {
        id: loginComponent

        LoginView {
            onboardingFlowController: root.onboardingFlowController
        }
    }

    Component {
        id: driveSelectionComponent

        DriveSelectionView {
            selectionController: root.driveSelectionController
            drivesModel: root.drivesModel
        }
    }

    Component {
        id: synchronizationComponent

        SynchronizationView {
            onboardingFlowController: root.onboardingFlowController
        }
    }

    Component {
        id: readyComponent

        ReadyView {
            onboardingFlowController: root.onboardingFlowController
        }
    }
}
