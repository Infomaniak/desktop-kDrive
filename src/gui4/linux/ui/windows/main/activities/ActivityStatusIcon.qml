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
import kDrive.UI

Item {
    id: root

    required property int status
    required property int progress

    readonly property string tooltipText: {
        switch (root.status) {
        case ActivityListModel.InProgress:
            return root.progress < 0 ? qsTrId("syncInProgressTooltip") : root.progress + "%";
        case ActivityListModel.Failed:
            return qsTrId("syncErrorTooltip");
        default:
            return qsTrId("syncSuccessTooltip");
        }
    }

    width: IKActivities.activityIconSize
    height: IKActivities.activityIconSize

    ActivityProgressRing {
        anchors.fill: parent
        visible: root.status === ActivityListModel.InProgress
        progress: root.progress
    }

    IKTintedIcon {
        anchors.fill: parent
        visible: root.status === ActivityListModel.Synchronized
        source: "qrc:/assets/main/activities/status-synchronized-outline.svg"
        color: IKColors.activitiesStatusSynchronized
    }

    IKTintedIcon {
        anchors.fill: parent
        visible: root.status === ActivityListModel.Failed
        source: "qrc:/assets/main/activities/status-error.svg"
        color: IKColors.activitiesStatusError
    }

    HoverHandler {
        id: hoverHandler
    }

    IKToolTip {
        visible: hoverHandler.hovered
        text: root.tooltipText
    }

    Accessible.name: root.tooltipText
}
