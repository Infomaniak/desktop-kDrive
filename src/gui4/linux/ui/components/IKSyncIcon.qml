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

// Renders a synchronization tinted with its drive's color: the bare drive glyph for a classic synchronization, the outline
// folder for an advanced one.
Item {
    id: root

    property bool advancedSync: false
    property color driveColor: IKColors.driveDefaultColor

    implicitWidth: IKMainWindow.syncSelectorIconSize
    implicitHeight: IKMainWindow.syncSelectorIconSize

    IKDriveIcon {
        anchors.fill: parent
        visible: !root.advancedSync
        driveColor: root.driveColor
    }

    IKTintedIcon {
        anchors.fill: parent
        visible: root.advancedSync
        source: "qrc:/assets/main/folder.svg"
        color: root.driveColor
    }
}
