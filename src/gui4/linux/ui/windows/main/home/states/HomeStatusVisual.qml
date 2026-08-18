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

    width: IKMainWindow.homeStatusVisualWidth
    height: IKMainWindow.homeStatusVisualHeight

    Loader {
        id: statusAnimationLoader

        anchors.centerIn: parent
        width: root.status === HomeController.Offline
               ? IKMainWindow.homeStatusOfflineAnimationWidth
               : IKMainWindow.homeStatusAnimationWidth
        height: root.status === HomeController.Offline
                ? IKMainWindow.homeStatusOfflineAnimationHeight
                : IKMainWindow.homeStatusAnimationHeight
        active: statusAnimationLoader.sourceComponent !== null
        sourceComponent: {
            switch (root.status) {
            case HomeController.UpToDate:
                return idleAnimationComponent
            case HomeController.Syncing:
                return syncingAnimationComponent
            case HomeController.Paused:
                return pausedAnimationComponent
            case HomeController.Offline:
                return offlineAnimationComponent
            default:
                return null
            }
        }
    }

    Component {
        id: idleAnimationComponent

        HomeIdleAnimation {
            anchors.fill: parent
        }
    }

    Component {
        id: syncingAnimationComponent

        HomeSyncingAnimation {
            anchors.fill: parent
            animations.loops: Animation.Infinite
        }
    }

    Component {
        id: pausedAnimationComponent

        HomePausedAnimation {
            anchors.fill: parent
            animations.loops: Animation.Infinite
        }
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
        width: IKMainWindow.homeStatusSetupImageWidth
        height: IKMainWindow.homeStatusSetupImageHeight
        visible: root.status === HomeController.SetupRequired
        source: "qrc:/assets/main/home/status-setup-drawer.svg"
    }
}
