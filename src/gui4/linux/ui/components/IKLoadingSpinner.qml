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

import QtQuick
import QtQuick.Shapes
import kDrive.UI

Item {
    id: root

    property color color: IKColors.accentPrimary
    property real strokeWidth: 3
    property int rotationDuration: 900

    Shape {
        anchors.fill: parent

        ShapePath {
            strokeColor: root.color
            strokeWidth: root.strokeWidth
            capStyle: ShapePath.RoundCap
            fillColor: "transparent"

            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: Math.max(0, (root.width - root.strokeWidth) / 2)
                radiusY: Math.max(0, (root.height - root.strokeWidth) / 2)
                startAngle: 0
                sweepAngle: 270
            }
        }
    }

    RotationAnimator on rotation {
        from: 0
        to: 360
        duration: root.rotationDuration
        loops: Animation.Infinite
        running: root.visible
    }
}
