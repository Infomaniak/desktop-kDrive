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
import QtQuick.Effects
import kDrive.UI

// Renders the kDrive glyph tinted with the owning drive's color, without a background tile.
Item {
    id: root

    property color driveColor: IKColors.driveDefaultColor

    implicitWidth: IKMainWindow.syncSelectorIconSize
    implicitHeight: IKMainWindow.syncSelectorIconSize

    Image {
        anchors.fill: parent
        source: "qrc:/assets/onboarding/drive-icon-glyph.svg"
        // The glyph has no intrinsic size (12 px view box): rasterize it at 3x at least, or it is blurred once scaled.
        sourceSize.width: Math.ceil(width * Math.max(3, Screen.devicePixelRatio))
        fillMode: Image.Stretch
        smooth: true
        layer.enabled: true
        layer.effect: MultiEffect {
            colorization: 1
            colorizationColor: root.driveColor
        }
    }
}
