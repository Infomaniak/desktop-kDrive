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

    property color color: IKColors.accentSecondary
    property real minimumSweepAngle: 12
    property real maximumSweepAngle: 300
    property real strokeWidth: 3
    property int cycleDuration: 2400
    property real cycleProgress: 0

    readonly property real contractionProgress: Math.max(0, root.cycleProgress * 2 - 1)
    readonly property real sweepRange: root.maximumSweepAngle - root.minimumSweepAngle
    readonly property real sweepAngle: {
        const expansionProgress = Math.sin(Math.PI * root.cycleProgress);
        return root.minimumSweepAngle + root.sweepRange * root.smoothStep(expansionProgress);
    }
    readonly property real startAngle: {
        const baseRotation = root.cycleProgress * (720 - root.sweepRange);
        return baseRotation + root.sweepRange * root.smoothStep(root.contractionProgress);
    }

    function smoothStep(value) {
        return value * value * (3 - 2 * value);
    }

    Shape {
        anchors.fill: parent

        ShapePath {
            strokeColor: root.color
            strokeWidth: root.strokeWidth
            capStyle: ShapePath.FlatCap
            fillColor: "transparent"

            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: Math.max(0, (root.width - root.strokeWidth) / 2)
                radiusY: Math.max(0, (root.height - root.strokeWidth) / 2)
                startAngle: root.startAngle
                sweepAngle: root.sweepAngle
            }
        }
    }

    NumberAnimation on cycleProgress {
        from: 0
        to: 1
        duration: root.cycleDuration
        loops: Animation.Infinite
        running: root.visible
    }
}
