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
import QtQuick.Layouts
import kDrive.UI

Item {
    id: root

    required property var controller

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: IKActivities.pagePadding
        spacing: IKActivities.sectionSpacing

        ActivitiesHeader {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            controller: root.controller
        }

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true

            IKLoadingSpinner {
                anchors.centerIn: parent
                width: IKSpacing.s32
                height: IKSpacing.s32
                visible: root.controller.loading
            }

            ActivitiesEmptyState {
                anchors.fill: parent
                visible: !root.controller.loading && !root.controller.hasActivities
            }

            ActivitiesTable {
                anchors.fill: parent
                visible: !root.controller.loading && root.controller.hasActivities
                model: root.controller.model
                controller: root.controller
            }
        }
    }
}
