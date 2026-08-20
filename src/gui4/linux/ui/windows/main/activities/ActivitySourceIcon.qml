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

    required property int sourceType

    readonly property bool knownSource: root.sourceType === ActivityListModel.Computer || root.sourceType === ActivityListModel.Web
    readonly property string tooltipText: root.sourceType === ActivityListModel.Computer ? qsTrId("syncedFromComputer") : qsTrId("syncedFromKDriveWeb")

    visible: knownSource
    width: visible ? IKActivities.activityIconSize : 0
    height: IKActivities.activityIconSize

    IKTintedIcon {
        anchors.centerIn: parent
        width: IKActivities.sourceIconSize
        height: IKActivities.sourceIconSize
        source: root.sourceType === ActivityListModel.Computer
                ? "qrc:/assets/main/activities/source-computer.svg"
                : "qrc:/assets/main/activities/source-web.svg"
        color: IKColors.activitiesSourceIcon
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
