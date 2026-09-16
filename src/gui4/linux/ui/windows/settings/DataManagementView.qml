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
import QtQuick.Controls
import kDrive.UI

ScrollView {
    id: root

    required property var controller
    readonly property string navigationTitle: qsTrId("dataManagementSettings")

    signal matomoRequested(var trigger)
    signal sentryRequested(var trigger)

    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    Column {
        width: root.availableWidth
        padding: IKSettings.pageMargin
        spacing: IKSpacing.s16

        Text {
            width: parent.width - 2 * IKSettings.pageMargin
            text: qsTrId("dataManagementSubtitle")
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize + 2
            font.weight: IKFonts.emphasized
            wrapMode: Text.WordWrap
        }

        Text {
            width: parent.width - 2 * IKSettings.pageMargin
            text: qsTrId("dataManagementDescription")
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
        }

        IKLinkButton {
            text: qsTrId("viewSourceCode")
            external: true
            onClicked: root.controller.openSources()
        }

        Text {
            width: parent.width - 2 * IKSettings.pageMargin
            visible: root.controller.dataManagementErrorText.length > 0
            text: root.controller.dataManagementErrorText
            color: IKColors.statusStrongWarning
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.AlertMessage
        }

        SettingsGroup {
            width: parent.width - 2 * IKSettings.pageMargin
            contentInset: 0

            SettingsNavigationRow {
                title: "Matomo"
                onNavigationRequested: trigger => root.matomoRequested(trigger)
            }

            SettingsNavigationRow {
                title: "Sentry"
                separator: false
                onNavigationRequested: trigger => root.sentryRequested(trigger)
            }
        }
    }
}
