/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

import QtQuick
import QtQuick.Shapes
import kDrive.UI

Item {
    id: root

    required property int progress

    readonly property bool indeterminate: progress < 0
    readonly property real normalizedProgress: Math.max(0, Math.min(100, progress))
    property real displayedProgress: normalizedProgress

    width: IKActivities.activityIconSize
    height: IKActivities.activityIconSize

    Behavior on displayedProgress {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    Shape {
        anchors.fill: parent
        visible: !root.indeterminate
        preferredRendererType: Shape.CurveRenderer
        antialiasing: true

        ShapePath {
            strokeColor: IKColors.activitiesProgressTrack
            strokeWidth: IKActivities.progressStrokeWidth
            capStyle: ShapePath.RoundCap
            fillColor: "transparent"

            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: (root.width - IKActivities.progressStrokeWidth) / 2
                radiusY: (root.height - IKActivities.progressStrokeWidth) / 2
                startAngle: -90
                sweepAngle: 360
            }
        }

        ShapePath {
            strokeColor: IKColors.activitiesStatusSyncing
            strokeWidth: IKActivities.progressStrokeWidth
            capStyle: ShapePath.RoundCap
            fillColor: "transparent"

            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: (root.width - IKActivities.progressStrokeWidth) / 2
                radiusY: (root.height - IKActivities.progressStrokeWidth) / 2
                startAngle: -90
                sweepAngle: root.displayedProgress * 3.6
            }
        }
    }

    IKLoadingSpinner {
        anchors.fill: parent
        visible: root.indeterminate
        color: IKColors.activitiesStatusSyncing
        strokeWidth: IKActivities.progressStrokeWidth
    }
}
