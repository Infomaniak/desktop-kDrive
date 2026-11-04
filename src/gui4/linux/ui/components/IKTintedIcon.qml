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

import QtQuick
import QtQuick.Effects
import kDrive.UI

Item {
    id: root

    property url source
    property color color: "white"

    readonly property size naturalSize: SvgInfo.naturalSize(source)
    readonly property real sourceRatio: naturalSize.width / Math.max(1, naturalSize.height)
    readonly property real glyphWidth: Math.min(width, height * sourceRatio)
    readonly property real glyphHeight: glyphWidth / sourceRatio

    Image {
        id: iconSource

        x: (root.width - width) / 2
        y: (root.height - height) / 2
        width: root.glyphWidth
        height: root.glyphHeight
        source: root.source
        // Qt applies the window DPR. The 2x texture reduces evenly when sampled with linear filtering.
        sourceSize.width: Math.ceil(width * 2)
        // The rectangle already preserves the aspect ratio; Stretch avoids MultiEffect's intermediate texture.
        fillMode: Image.Stretch
        smooth: true
        visible: false
    }

    MultiEffect {
        anchors.fill: iconSource
        source: iconSource
        colorization: 1
        colorizationColor: root.color
    }
}
