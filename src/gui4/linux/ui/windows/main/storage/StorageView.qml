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
import kDrive.UI

Item {
    id: root

    required property var controller

    Component.onCompleted: controller.setViewActive(true)
    Component.onDestruction: controller.setViewActive(false)

    StorageUsageCard {
        anchors.left: parent.left
        anchors.leftMargin: IKSpacing.page
        anchors.right: parent.right
        anchors.rightMargin: IKSpacing.page
        anchors.top: parent.top
        anchors.topMargin: IKSpacing.s32
        visible: root.controller.state !== StorageController.Unavailable
        controller: root.controller
        loading: root.controller.state === StorageController.Loading
    }

    StorageErrorState {
        anchors.fill: parent
        visible: root.controller.state === StorageController.Unavailable
        controller: root.controller
    }
}
