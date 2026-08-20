/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import kDrive.UI

Item {
    id: root

    required property var controller

    readonly property string filterLabel: controller.filter === ActivityListModel.MyActivityOnly ? qsTrId("activitiesTypeMyActivity") : qsTrId("activitiesTypeAllActivities")

    implicitHeight: IKActivities.headerHeight

    Text {
        anchors.left: parent.left
        anchors.right: filterButton.left
        anchors.rightMargin: IKSpacing.s16
        anchors.verticalCenter: parent.verticalCenter
        text: root.controller.title
        color: IKColors.textPrimary
        font.pixelSize: IKFonts.title3Size
        font.weight: IKFonts.emphasized
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    ToolButton {
        id: filterButton

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: IKActivities.filterButtonHeight
        width: filterContent.implicitWidth + IKSpacing.s16
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        onClicked: filterMenu.open()

        contentItem: Row {
            id: filterContent

            spacing: IKSpacing.s8

            IKTintedIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: IKActivities.filterIconSize
                height: IKActivities.filterIconSize
                source: root.controller.filter === ActivityListModel.AllActivities ? "qrc:/assets/main/activities/filter-all-activities.svg" : "qrc:/assets/main/activities/filter-my-activity.svg"
                color: IKColors.textPrimary
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.filterLabel
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.emphasized
            }

            IKTintedIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: IKActivities.filterIconSize
                height: IKActivities.filterIconSize
                source: "qrc:/assets/main/chevron-down.svg"
                color: IKColors.textSecondary
            }
        }

        background: Rectangle {
            radius: IKRadius.r6
            color: filterButton.hovered || filterButton.down ? IKColors.surfaceTertiary : IKColors.activitiesFilterSurface
            border.width: filterButton.visualFocus ? 2 : 0
            border.color: IKColors.accentPrimary
        }

        Accessible.name: root.filterLabel
    }

    ActivitiesFilterMenu {
        id: filterMenu

        x: Math.max(0, root.width - width)
        y: filterButton.y + filterButton.height + IKSpacing.s4
        controller: root.controller
        returnFocusItem: filterButton
    }
}
