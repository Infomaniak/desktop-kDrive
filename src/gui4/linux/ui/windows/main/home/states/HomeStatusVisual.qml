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

    required property int status

    width: 228
    height: 131

    Loader {
        anchors.centerIn: parent
        active: root.status === HomeController.UpToDate
        height: 131
        sourceComponent: idleAnimationComponent
        width: 178
    }

    Component {
        id: idleAnimationComponent

        HomeIdleAnimation {
            anchors.fill: parent
        }
    }

    Loader {
        anchors.centerIn: parent
        active: root.status === HomeController.Syncing
        height: 131
        sourceComponent: syncingAnimationComponent
        width: 178
    }

    Component {
        id: syncingAnimationComponent

        HomeSyncingAnimation {
            anchors.fill: parent
            animations.loops: Animation.Infinite
        }
    }

    Loader {
        anchors.centerIn: parent
        active: root.status === HomeController.Paused
        height: 131
        sourceComponent: pausedAnimationComponent
        width: 178
    }

    Component {
        id: pausedAnimationComponent

        HomePausedAnimation {
            anchors.fill: parent
            animations.loops: Animation.Infinite
        }
    }

    Loader {
        anchors.centerIn: parent
        active: root.status === HomeController.Offline
        height: 90
        sourceComponent: offlineAnimationComponent
        width: 227
    }

    Component {
        id: offlineAnimationComponent

        HomeOfflineAnimation {
            anchors.fill: parent
            animations.loops: Animation.Infinite
        }
    }

    Image {
        anchors.centerIn: parent
        width: 126
        height: 121
        visible: root.status === HomeController.SetupRequired
        source: "qrc:/assets/main/home/status-setup-drawer.svg"
    }
}
