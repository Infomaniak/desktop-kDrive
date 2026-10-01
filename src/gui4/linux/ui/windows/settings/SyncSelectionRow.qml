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

// "Synchronization" row of a synchronized folder: custom-selection summary, then Manage, or Retry when the confirmed
// selection could not be loaded. Extra accessories, shown while `syncConfigured` is false, follow the built-in ones.
SettingsRow {
    id: root

    property bool syncConfigured: true
    property bool customSelection: false
    property bool blackListLoading: false
    property bool blackListLoadFailed: false
    // Appended to the accessible names, to tell apart the rows of several synchronizations.
    property string accessibleContext: ""

    signal retryRequested
    signal manageRequested(Item trigger)

    title: qsTrId("labelSynchronisation")

    Text {
        visible: root.syncConfigured && root.customSelection
        text: qsTrId("onboardingExclusionSummarySome")
        color: IKColors.textSecondary
        font.pixelSize: IKFonts.bodySize
    }

    IKModalButton {
        visible: root.syncConfigured && root.blackListLoadFailed
        role: IKModalButton.Tonal
        text: qsTrId("buttonRetry")
        Accessible.name: (text + " " + root.title + " " + root.accessibleContext).trim()
        onClicked: root.retryRequested()
    }

    IKModalButton {
        id: manageButton

        visible: root.syncConfigured && !root.blackListLoadFailed
        role: IKModalButton.Tonal
        text: qsTrId("buttonManage")
        Accessible.name: (text + " " + root.title + " " + root.accessibleContext).trim()
        Accessible.description: root.customSelection ? qsTrId("onboardingExclusionSummarySome") : ""
        busy: root.blackListLoading
        actionEnabled: !root.blackListLoading
        onClicked: root.manageRequested(manageButton)
    }
}
