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

        IKErrorBanner {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            errorCount: root.controller.errorCount
            visible: root.controller.hasErrors
            onActionTriggered: root.controller.requestFixAllErrors()
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
