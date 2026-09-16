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
import QtQuick.Controls
import kDrive.UI

ScrollView {
    id: root

    readonly property string navigationTitle: qsTrId("sidebarItemAdvanced")

    signal fileExclusionsRequested(var trigger)
    signal dataManagementRequested(var trigger)
    signal networkRequested(var trigger)
    signal debugRequested(var trigger)

    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    Column {
        width: root.availableWidth
        padding: IKSettings.pageMargin

        SettingsGroup {
            width: parent.width - 2 * IKSettings.pageMargin
            contentInset: 0

            SettingsNavigationRow {
                title: qsTrId("filesToExclude")
                description: qsTrId("excludeRuleFileDescription")
                enabled: false
                onNavigationRequested: trigger => root.fileExclusionsRequested(trigger)
            }

            SettingsNavigationRow {
                title: qsTrId("dataManagementSettings")
                onNavigationRequested: trigger => root.dataManagementRequested(trigger)
            }

            SettingsNavigationRow {
                title: qsTrId("networkSettings")
                enabled: false
                onNavigationRequested: trigger => root.networkRequested(trigger)
            }

            SettingsNavigationRow {
                title: qsTrId("logLevelDebug")
                separator: false
                enabled: false
                onNavigationRequested: trigger => root.debugRequested(trigger)
            }
        }
    }
}
