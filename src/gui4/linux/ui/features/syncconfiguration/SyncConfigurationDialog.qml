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

IKModal {
    id: root

    required property var controller

    preferredWidth: IKSyncConfiguration.modalWidth
    // Escape cancels the current page, but never while a request the user cannot see is still running.
    escapeDismissible: !root.controller.busy
    visible: root.controller.visible
    title: {
        if (root.controller.driveConfigurationPage) {
            return qsTrId("onBoardingAdvancedSettingsDriveTitle")
        }
        if (root.controller.folderSelectionPage) {
            return qsTrId("onboardingAdvancedSettingsExclusionTitle")
        }
        return qsTrId("onboardingAdvancedSettingsDriveSelectionTitle")
    }
    onDismissRequested: root.controller.cancelCurrentPage()

    bodyData: [
        Loader {
            width: parent ? parent.width : implicitWidth
            sourceComponent: {
                if (root.controller.driveConfigurationPage) {
                    return drivePage
                }
                if (root.controller.folderSelectionPage) {
                    return folderPage
                }
                return summaryPage
            }
        }
    ]

    footerData: [
        IKModalButton {
            role: IKModalButton.Secondary
            text: qsTrId("buttonCancel")
            actionEnabled: !root.controller.busy
            onClicked: root.controller.cancelCurrentPage()
        },
        IKModalButton {
            id: validateButton

            role: IKModalButton.Primary
            text: qsTrId("buttonValidate")
            actionEnabled: root.controller.canValidate
            busy: root.controller.busy
            onClicked: root.controller.validateCurrentPage()
        }
    ]

    Component {
        id: summaryPage

        SyncConfigurationSummaryPage {
            controller: root.controller
        }
    }

    Component {
        id: drivePage

        SyncConfigurationDrivePage {
            controller: root.controller
        }
    }

    Component {
        id: folderPage

        SyncConfigurationFolderPage {
            controller: root.controller
        }
    }
}
