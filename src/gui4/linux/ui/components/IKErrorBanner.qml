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
import kDrive.UI

// Synchronization error banner shared by Home and Activities. The owning view supplies the height its screen
// specifies and handles `actionTriggered`; the banner itself never reads a controller.
IKInformationBlock {
    id: root

    required property int errorCount

    signal actionTriggered

    // Gives the keyboard focus to the "Fix errors" button, for instance when the user comes back from the errors page.
    function focusAction() {
        root.focusPrimaryAction();
    }

    title: qsTrId("informationBlockSynchroErrorTitle", root.errorCount)
    titleTextFormat: Text.StyledText
    subtitle: qsTrId("informationBlockSynchroErrorSubtitle")
    surfaceColor: IKColors.errorBannerSurface
    primaryActionText: qsTrId("buttonFixErrors")
    onPrimaryActionTriggered: root.actionTriggered()
}
