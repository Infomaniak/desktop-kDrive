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

Item {
    id: root

    required property var controller

    ColumnLayout {
        anchors.bottomMargin: IKSpacing.s24
        anchors.fill: parent
        anchors.leftMargin: IKSpacing.s24
        anchors.rightMargin: IKSpacing.s24
        anchors.topMargin: IKSpacing.s32
        spacing: IKSpacing.s24

        HomeGreeting {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            controller: root.controller
            visible: root.controller.status !== HomeController.SetupRequired && root.controller.status !== HomeController.Loading
        }
        // One banner at a time, chosen by HomeController.banner: two would push the shortcuts card out of a
        // minimum-size window.
        IKInformationBlock {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            title: qsTrId("informationBlockKDriveFullTitle")
            subtitle: qsTrId("informationBlockKDriveFullSubtitle")
            subtitleMaximumLineCount: IKMainWindow.informationBlockSubtitleMaxLines
            iconSource: "qrc:/assets/main/home/hard-drive.svg"
            // Only an admin can add storage; the others can only close the banner.
            primaryActionText: root.controller.driveAdmin ? qsTrId("buttonAddStorage") : ""
            primaryActionExternal: true
            dismissText: qsTrId("buttonClose")
            actionId: "main.home.driveFullBanner.addStorage"
            visible: root.controller.banner === HomeController.DriveFull
            onPrimaryActionTriggered: root.controller.openDriveUpsell()
            onDismissRequested: root.controller.dismissDriveFull()
        }
        IKInformationBlock {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            title: qsTrId("informationBlockBackErrorTitle")
            subtitle: qsTrId("informationBlockBackErrorSubtitle")
            subtitleMaximumLineCount: IKMainWindow.informationBlockSubtitleMaxLines
            iconSource: "qrc:/assets/main/home/circle-cross.svg"
            primaryActionText: qsTrId("buttonRetry")
            actionId: "main.home.serverErrorBanner.retry"
            visible: root.controller.banner === HomeController.ServerError
            onPrimaryActionTriggered: root.controller.retrySync()
        }
        IKErrorBanner {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            errorCount: root.controller.errorCount
            actionId: "main.home.errorBanner.showErrors"
            visible: root.controller.banner === HomeController.SyncErrors
            onActionTriggered: root.controller.showErrors()
        }
        IKInformationBlock {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            title: qsTrId("onboardingV4Title")
            subtitle: qsTrId("onboardingV4Subtitle")
            iconSource: "qrc:/assets/main/home/kdrive-product.svg"
            primaryActionText: qsTrId("onboardingV4Faq")
            primaryActionExternal: true
            dismissText: qsTrId("buttonClose")
            actionId: "main.home.v4OnboardingBanner.faq"
            visible: root.controller.banner === HomeController.V4Onboarding
            onPrimaryActionTriggered: root.controller.openV4OnboardingFaq()
            onDismissRequested: root.controller.dismissV4Onboarding()
        }
        Row {
            id: panelsRow

            readonly property bool hasDriveShortcuts: root.controller.hasCurrentDrive
            readonly property real panelsWidth: Math.max(0, width - (hasDriveShortcuts ? spacing : 0))

            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: IKSpacing.s24

            HomeStatusPanel {
                controller: root.controller
                height: panelsRow.height
                width: panelsRow.hasDriveShortcuts ? Math.max(0, panelsRow.panelsWidth - driveWebShortcuts.implicitWidth) : panelsRow.width
            }
            DriveWebShortcutsView {
                id: driveWebShortcuts

                controller: root.controller
                height: panelsRow.height
                visible: panelsRow.hasDriveShortcuts
                width: implicitWidth
            }
        }
    }
}
