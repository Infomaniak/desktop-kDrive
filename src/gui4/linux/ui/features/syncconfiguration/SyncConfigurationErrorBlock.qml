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

// Inline failure report for the sync-configuration pages. It states what cannot be done and why, so a rejected folder
// explains the rule it broke instead of only saying that it was refused.
Column {
    id: root

    property string errorTitle: ""
    property string errorText: ""

    visible: errorTitle.length > 0 || errorText.length > 0
    spacing: IKSyncConfiguration.cardTitleSpacing
    // Announced as an alert so a failure is reported without moving the focus away from what the user was doing.
    Accessible.role: Accessible.AlertMessage
    Accessible.name: [root.errorTitle, root.errorText].filter(part => part.length > 0).join(". ")

    Text {
        width: parent.width
        visible: text.length > 0
        text: root.errorTitle
        color: IKColors.syncConfigurationError
        font.pixelSize: IKFonts.subheadlineSize
        font.weight: IKFonts.emphasized
        wrapMode: Text.WordWrap
    }

    Text {
        width: parent.width
        visible: text.length > 0
        text: root.errorText
        color: IKColors.syncConfigurationError
        font.pixelSize: IKFonts.subheadlineSize
        wrapMode: Text.WordWrap
    }
}
